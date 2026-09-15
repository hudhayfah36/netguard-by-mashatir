#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "dns_engine.h"
#include "blocklist.h"
#include "schedule_store.h"
#include "time_service.h"
#include "profile_store.h"

static const char *TAG = "dns_engine";

#define DNS_PORT             53
#define DNS_PACKET_MAX_LEN   1232  // safe EDNS UDP payload without IP fragmentation
#define UPSTREAM_TIMEOUT_MS  700

static const char *UPSTREAM_DNS_PRIMARY   = "1.1.1.3"; // Cloudflare malware/adult filter
static const char *UPSTREAM_DNS_SECONDARY = "1.0.0.3"; // Cloudflare Family fallback

volatile uint32_t g_total_queries = 0;
volatile uint32_t g_blocked_queries = 0;
int64_t g_boot_time_us = 0;

static int s_dns_sock = -1;
static int s_upstream_sock = -1;

// ============================================================================
// DNS name / packet parsing helpers
// ============================================================================

// Parses a DNS name (sequence of length-prefixed labels, zero-terminated)
// starting at `offset` within buf[0..len). Does NOT follow compression
// pointers -- client queries never use compression in the Question section.
// Returns the offset just past the terminating zero byte, or -1 on error.
static int parse_dns_name(const uint8_t *buf, int len, int offset, char *out, int out_size) {
    int pos = offset;
    int out_len = 0;
    out[0] = '\0';

    while (pos < len) {
        uint8_t label_len = buf[pos];
        if (label_len == 0) {
            return pos + 1; // consume terminating zero
        }
        if ((label_len & 0xC0) == 0xC0 || label_len > 63) {
            return -1; // unexpected compression pointer in a query
        }
        pos++;
        if (pos + label_len > len) return -1; // truncated/malformed

        if (out_len > 0 && out_len < out_size - 1) {
            out[out_len++] = '.';
        }
        for (int i = 0; i < label_len && out_len < out_size - 1; i++) {
            out[out_len++] = (char)buf[pos + i];
        }
        out[out_len] = '\0';
        pos += label_len;
    }
    return -1; // ran off the end without a terminator
}

// Extracts the queried domain from a raw DNS query packet. `question_end` is
// set to the offset just past QTYPE+QCLASS (needed to size the NXDOMAIN reply).
static bool parse_dns_query(const uint8_t *buf, int len, char *domain_out, int domain_out_size,
                             int *question_end) {
    if (len < 12) return false;

    uint16_t qdcount = (buf[4] << 8) | buf[5];
    if (qdcount != 1 || (buf[2] & 0x80)) return false;

    int after_name = parse_dns_name(buf, len, 12, domain_out, domain_out_size);
    if (after_name < 0) return false;

    if (after_name + 4 > len) return false; // QTYPE(2) + QCLASS(2)
    *question_end = after_name + 4;
    return true;
}

// Builds an authoritative NXDOMAIN response by cloning the query's header ID
// and full Question section, then rewriting the flags/RCODE. Returns the
// response length, or -1 on failure.
static int build_nxdomain_response(const uint8_t *query, int question_end,
                                    uint8_t *out, int out_size) {
    if (question_end < 12 || question_end > out_size) return -1;

    memcpy(out, query, question_end); // header + question section verbatim

    // Byte 2: QR=1, Opcode=copy, AA=1, TC=0, RD=copy
    out[2] = 0x80 | (query[2] & 0x78) | 0x04 | (query[2] & 0x01);
    // Byte 3: RA=1, Z=0, RCODE=3 (NXDOMAIN)
    out[3] = 0x80 | 0x03;
    // ANCOUNT = NSCOUNT = ARCOUNT = 0 (QDCOUNT at bytes 4-5 is left as copied)
    out[6] = 0; out[7] = 0;
    out[8] = 0; out[9] = 0;
    out[10] = 0; out[11] = 0;

    return question_end;
}

// ============================================================================
// Upstream forwarding
// ============================================================================

// Forwards the raw query to an upstream resolver and relays the raw response
// back to the original client. Blocking with a bounded timeout -- safe here
// because this entire engine runs on its own pinned task/core and never
// shares a thread with the HTTP server.
static void forward_upstream(const uint8_t *query, int query_len,
                              const struct sockaddr_in *client_addr, socklen_t client_addr_len) {
    uint8_t resp_buf[DNS_PACKET_MAX_LEN];
    const char *servers[2] = { UPSTREAM_DNS_PRIMARY, UPSTREAM_DNS_SECONDARY };

    for (int attempt = 0; attempt < 2; attempt++) {
        struct sockaddr_in upstream_addr = {0};
        upstream_addr.sin_family = AF_INET;
        upstream_addr.sin_port = htons(53);
        if (inet_pton(AF_INET, servers[attempt], &upstream_addr.sin_addr) != 1) {
            continue;
        }

        // Connected UDP filters packets from other hosts at the socket layer.
        if (connect(s_upstream_sock, (struct sockaddr *)&upstream_addr, sizeof(upstream_addr)) != 0) continue;
        int sent = send(s_upstream_sock, query, query_len, 0);
        if (sent < 0) {
            ESP_LOGW(TAG, "sendto upstream %s failed (errno=%d)", servers[attempt], errno);
            continue;
        }

        // s_upstream_sock has SO_RCVTIMEO set to UPSTREAM_TIMEOUT_MS at creation.
        int n = recv(s_upstream_sock, resp_buf, sizeof(resp_buf), 0);
        // Also validate DNS ID and that this is a response before relaying it.
        if (n >= 12 && resp_buf[0] == query[0] && resp_buf[1] == query[1] && (resp_buf[2] & 0x80)) {
            sendto(s_dns_sock, resp_buf, n, 0,
                   (struct sockaddr *)client_addr, client_addr_len);
            return;
        }
        ESP_LOGD(TAG, "No valid response from upstream %s (attempt %d)", servers[attempt], attempt + 1);
    }
    ESP_LOGE(TAG, "All upstream resolvers failed or timed out for this query.");
}

// ============================================================================
// Main DNS engine task (Core 0)
// ============================================================================

static void dns_engine_task(void *arg) {
    static uint8_t query_buf[DNS_PACKET_MAX_LEN];
    static uint8_t resp_buf[DNS_PACKET_MAX_LEN];
    char domain[MAX_DOMAIN_LEN];

    ESP_LOGI(TAG, "DNS engine task running on core %d", xPortGetCoreID());

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);

        int len = recvfrom(s_dns_sock, query_buf, sizeof(query_buf), 0,
                            (struct sockaddr *)&client_addr, &client_addr_len);
        if (len <= 0) {
            continue; // timeout or spurious wakeup, loop again
        }

        int question_end = 0;
        if (!parse_dns_query(query_buf, len, domain, sizeof(domain), &question_end)) {
            ESP_LOGW(TAG, "Failed to parse malformed/unsupported query (len=%d), dropping.", len);
            continue;
        }

        g_total_queries++;
        int profile_id = profile_store_find_ip(client_addr.sin_addr.s_addr);
        if (profile_id >= 0) profile_store_indicator(profile_id, domain,
            time_service_is_valid() ? (uint32_t)time(NULL) : (uint32_t)(esp_timer_get_time() / 1000000));

        if (blocklist_is_blocked(domain)) {
            g_blocked_queries++;
            profile_store_count_query(profile_id, true);
            int resp_len = build_nxdomain_response(query_buf, question_end, resp_buf, sizeof(resp_buf));
            if (resp_len > 0) {
                sendto(s_dns_sock, resp_buf, resp_len, 0,
                       (struct sockaddr *)&client_addr, client_addr_len);
            }
        } else {
            bool profile_allowed = false;
            bool profile_scheduled = profile_store_query(profile_id, domain, time_service_is_valid(),
                time_service_is_valid() ? time_service_local_minute() : 0, &profile_allowed);
            bool scheduled_allowed = false;
            bool scheduled = schedule_store_query(domain, time_service_is_valid(),
                                                  time_service_is_valid() ? time_service_local_minute() : 0,
                                                  &scheduled_allowed);
            bool policy_blocked = (profile_scheduled && !profile_allowed) || (scheduled && !scheduled_allowed) || profile_store_is_blocked(profile_id, domain);
            profile_store_count_query(profile_id, policy_blocked);
            if (policy_blocked) {
                int resp_len = build_nxdomain_response(query_buf, question_end, resp_buf, sizeof(resp_buf));
                if (resp_len > 0) sendto(s_dns_sock, resp_buf, resp_len, 0,
                                         (struct sockaddr *)&client_addr, client_addr_len);
            } else {
                forward_upstream(query_buf, len, &client_addr, client_addr_len);
            }
        }
    }
}

// ============================================================================
// Public entry point
// ============================================================================

void dns_engine_start(void) {
    g_boot_time_us = esp_timer_get_time();

    // ---- Listening socket (port 53) ----
    s_dns_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_dns_sock < 0) {
        ESP_LOGE(TAG, "Failed to create DNS listening socket (errno=%d)", errno);
        return;
    }

    int reuse = 1;
    setsockopt(s_dns_sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    struct sockaddr_in bind_addr = {0};
    bind_addr.sin_family = AF_INET;
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    bind_addr.sin_port = htons(DNS_PORT);

    if (bind(s_dns_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) != 0) {
        ESP_LOGE(TAG, "Failed to bind UDP port %d (errno=%d)", DNS_PORT, errno);
        close(s_dns_sock);
        s_dns_sock = -1;
        return;
    }
    ESP_LOGI(TAG, "Listening for DNS queries on UDP port %d", DNS_PORT);

    // ---- Upstream forwarding socket (ephemeral local port, with recv timeout) ----
    s_upstream_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_upstream_sock < 0) {
        ESP_LOGE(TAG, "Failed to create upstream socket (errno=%d)", errno);
        close(s_dns_sock);
        s_dns_sock = -1;
        return;
    }
    struct timeval tv = {
        .tv_sec = UPSTREAM_TIMEOUT_MS / 1000,
        .tv_usec = (UPSTREAM_TIMEOUT_MS % 1000) * 1000,
    };
    setsockopt(s_upstream_sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // ---- Pin the engine task to Core 0, leaving Core 1 for the HTTP server ----
    xTaskCreatePinnedToCore(
        dns_engine_task,
        "dns_engine",
        8192,
        NULL,
        10,          // relatively high priority: this is latency-sensitive
        NULL,
        0            // Core 0
    );
}
