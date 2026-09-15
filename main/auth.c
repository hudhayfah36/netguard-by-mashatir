#include <string.h>
#include "nvs.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "mbedtls/sha256.h"
#include "auth.h"

#define AUTH_NS "auth"
#define ITERATIONS 10000
static const char *TAG = "auth";
static uint8_t s_salt[16], s_hash[32];
static bool s_configured;
static char s_session[33], s_csrf[33];
static int64_t s_last_activity_us;
#define SESSION_IDLE_US (12LL * 60 * 60 * 1000000)

static void hex(const uint8_t *in, size_t n, char *out) {
    static const char chars[] = "0123456789abcdef";
    for (size_t i = 0; i < n; ++i) { out[i * 2] = chars[in[i] >> 4]; out[i * 2 + 1] = chars[in[i] & 15]; }
    out[n * 2] = '\0';
}

static void password_hash(const char *password, const uint8_t salt[16], uint8_t out[32]) {
    uint8_t current[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, salt, 16);
    mbedtls_sha256_update(&ctx, (const unsigned char *)password, strlen(password));
    mbedtls_sha256_finish(&ctx, current);
    for (int i = 1; i < ITERATIONS; ++i) {
        mbedtls_sha256_starts(&ctx, 0);
        mbedtls_sha256_update(&ctx, current, sizeof(current));
        mbedtls_sha256_update(&ctx, salt, 16);
        mbedtls_sha256_finish(&ctx, current);
    }
    mbedtls_sha256_free(&ctx);
    memcpy(out, current, sizeof(current));
}

void auth_init(void) {
    nvs_handle_t h;
    if (nvs_open(AUTH_NS, NVS_READONLY, &h) == ESP_OK) {
        size_t salt_len = sizeof(s_salt), hash_len = sizeof(s_hash);
        s_configured = nvs_get_blob(h, "salt", s_salt, &salt_len) == ESP_OK && salt_len == sizeof(s_salt) &&
                       nvs_get_blob(h, "hash", s_hash, &hash_len) == ESP_OK && hash_len == sizeof(s_hash);
        nvs_close(h);
    }
    ESP_LOGI(TAG, "Admin password %s", s_configured ? "configured" : "not configured");
}

bool auth_is_configured(void) { return s_configured; }

esp_err_t auth_set_password(const char *password) {
    if (!password || strlen(password) < 12 || strlen(password) > 63) return ESP_ERR_INVALID_ARG;
    esp_fill_random(s_salt, sizeof(s_salt));
    password_hash(password, s_salt, s_hash);
    nvs_handle_t h;
    esp_err_t err = nvs_open(AUTH_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    if ((err = nvs_set_blob(h, "salt", s_salt, sizeof(s_salt))) == ESP_OK)
        err = nvs_set_blob(h, "hash", s_hash, sizeof(s_hash));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err == ESP_OK) s_configured = true;
    return err;
}

static bool constant_equal(const uint8_t *a, const uint8_t *b, size_t n) {
    uint8_t diff = 0; for (size_t i = 0; i < n; ++i) diff |= a[i] ^ b[i]; return diff == 0;
}

bool auth_login(const char *password, char session_hex[33], char csrf_hex[33]) {
    uint8_t candidate[32], random[16];
    if (!s_configured || !password) return false;
    password_hash(password, s_salt, candidate);
    if (!constant_equal(candidate, s_hash, sizeof(candidate))) return false;
    esp_fill_random(random, sizeof(random)); hex(random, sizeof(random), s_session);
    esp_fill_random(random, sizeof(random)); hex(random, sizeof(random), s_csrf);
    strcpy(session_hex, s_session); strcpy(csrf_hex, s_csrf);
    s_last_activity_us = esp_timer_get_time();
    return true;
}

void auth_logout(const char *session_hex) { if (session_hex && strcmp(session_hex, s_session) == 0) s_session[0] = s_csrf[0] = '\0'; }
const char *auth_session_csrf(const char *session_hex) {
    return session_hex && s_session[0] != '\0' && strcmp(session_hex, s_session) == 0 ? s_csrf : NULL;
}

static bool cookie_value(httpd_req_t *req, char out[33]) {
    char cookie[160] = {0};
    if (httpd_req_get_hdr_value_str(req, "Cookie", cookie, sizeof(cookie)) != ESP_OK) return false;
    const char *p = strstr(cookie, "NGSID="); if (!p) return false; p += 6;
    size_t n = strcspn(p, "; "); if (n != 32) return false;
    memcpy(out, p, 32); out[32] = '\0'; return true;
}

bool auth_authorize(httpd_req_t *req, bool require_csrf) {
    char session[33], csrf[40] = {0};
    if (!cookie_value(req, session) || strcmp(session, s_session) != 0 || !s_session[0] || esp_timer_get_time() - s_last_activity_us > SESSION_IDLE_US) { s_session[0] = s_csrf[0] = '\0'; return false; }
    if (!require_csrf) { s_last_activity_us = esp_timer_get_time(); return true; }
    bool ok = httpd_req_get_hdr_value_str(req, "X-CSRF-Token", csrf, sizeof(csrf)) == ESP_OK &&
           constant_equal((const uint8_t *)csrf, (const uint8_t *)s_csrf, 32) && csrf[32] == '\0';
    if (ok) s_last_activity_us = esp_timer_get_time();
    return ok;
}
uint32_t auth_session_remaining_seconds(void) { if (!s_session[0]) return 0; int64_t left = SESSION_IDLE_US - (esp_timer_get_time() - s_last_activity_us); return left > 0 ? (uint32_t)(left / 1000000) : 0; }
