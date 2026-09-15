#include <string.h>
#include <strings.h>  // strncasecmp
#include <ctype.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "blocklist.h"

static const char *TAG = "blocklist";
#define NVS_NS "blocklist"

// Buffer of MAX_BLOCKLIST fixed-width domain strings. Allocated once in
// blocklist_init(), preferring PSRAM since 200 * 128 bytes = 25.6 KB, which
// we don't want competing with internal DMA-capable heap over long uptimes.
static char (*s_blocklist)[MAX_DOMAIN_LEN] = NULL;
static volatile int s_count = 0;
static SemaphoreHandle_t s_mutex = NULL;

// ----------------------------------------------------------------------------
// Helpers
// ----------------------------------------------------------------------------

// Lowercases and strips a trailing '.' in place. Truncates to buffer size.
static void normalize_domain(const char *in, char *out, size_t out_size) {
    size_t len = strnlen(in, out_size - 1);
    // trim leading/trailing whitespace
    size_t start = 0;
    while (start < len && isspace((unsigned char)in[start])) start++;
    size_t end = len;
    while (end > start && isspace((unsigned char)in[end - 1])) end--;

    size_t j = 0;
    for (size_t i = start; i < end && j < out_size - 1; i++) {
        out[j++] = (char)tolower((unsigned char)in[i]);
    }
    out[j] = '\0';
    if (j > 0 && out[j - 1] == '.') {
        out[j - 1] = '\0';
    }
}

static bool valid_domain(const char *domain) {
    size_t len = strlen(domain);
    if (len == 0 || len > 253) return false;
    int label = 0;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = (unsigned char)domain[i];
        if (c == '.') { if (label == 0 || label > 63 || domain[i - 1] == '-') return false; label = 0; continue; }
        if (!(isalnum(c) || c == '-') || (label == 0 && c == '-')) return false;
        ++label;
    }
    return label > 0 && label <= 63 && domain[len - 1] != '-';
}

// Persists the entire current list + count to NVS. Simple/robust approach:
// for <=200 short strings this completes in well under a second and is only
// triggered on add/remove, not on every DNS lookup.
static esp_err_t persist_all_locked(void) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(err));
        return err;
    }
    int32_t old_count = 0;
    nvs_get_i32(h, "count", &old_count);
    err = nvs_set_i32(h, "count", s_count);
    char key[16];
    for (int i = 0; i < s_count && err == ESP_OK; i++) {
        snprintf(key, sizeof(key), "d%d", i);
        err = nvs_set_str(h, key, s_blocklist[i]);
    }
    for (int i = s_count; i < old_count && err == ESP_OK; i++) {
        snprintf(key, sizeof(key), "d%d", i);
        err = nvs_erase_key(h, key);
    }
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

// ----------------------------------------------------------------------------
// Public API
// ----------------------------------------------------------------------------

void blocklist_init(void) {
    s_mutex = xSemaphoreCreateMutex();

    size_t bytes = (size_t)MAX_BLOCKLIST * MAX_DOMAIN_LEN;
    s_blocklist = heap_caps_calloc(MAX_BLOCKLIST, MAX_DOMAIN_LEN, MALLOC_CAP_SPIRAM);
    if (s_blocklist) {
        ESP_LOGI(TAG, "Blocklist buffer (%u bytes) allocated in PSRAM.", (unsigned)bytes);
    } else {
        ESP_LOGW(TAG, "PSRAM allocation failed, falling back to internal heap.");
        s_blocklist = heap_caps_calloc(MAX_BLOCKLIST, MAX_DOMAIN_LEN, MALLOC_CAP_8BIT);
        if (!s_blocklist) {
            ESP_LOGE(TAG, "FATAL: could not allocate blocklist buffer at all.");
            return;
        }
    }

    // ---- Load from NVS ----
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No existing blocklist namespace (err=%s), starting empty.",
                 esp_err_to_name(err));
        return;
    }

    int32_t count = 0;
    nvs_get_i32(h, "count", &count);
    if (count > MAX_BLOCKLIST) count = MAX_BLOCKLIST;
    if (count < 0) count = 0;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_count = 0;
    char key[16];
    for (int i = 0; i < count; i++) {
        snprintf(key, sizeof(key), "d%d", i);
        size_t len = MAX_DOMAIN_LEN;
        if (nvs_get_str(h, key, s_blocklist[s_count], &len) == ESP_OK) {
            s_count++;
        }
    }
    xSemaphoreGive(s_mutex);
    nvs_close(h);

    ESP_LOGI(TAG, "Loaded %d blocklist entries from flash.", s_count);
}

bool blocklist_add(const char *domain_raw) {
    if (!s_blocklist || !domain_raw) return false;

    char domain[MAX_DOMAIN_LEN];
    normalize_domain(domain_raw, domain, sizeof(domain));
    if (!valid_domain(domain)) return false;

    bool added = false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool exists = false;
    for (int i = 0; i < s_count; i++) {
        if (strcasecmp(domain, s_blocklist[i]) == 0) { exists = true; break; }
    }
    if (!exists && s_count < MAX_BLOCKLIST) {
        strncpy(s_blocklist[s_count], domain, MAX_DOMAIN_LEN - 1);
        s_blocklist[s_count][MAX_DOMAIN_LEN - 1] = '\0';
        s_count++;
        added = true;
    }
    if (added && persist_all_locked() != ESP_OK) {
        s_count--;
        memset(s_blocklist[s_count], 0, MAX_DOMAIN_LEN);
        added = false;
    }
    xSemaphoreGive(s_mutex);

    if (added) ESP_LOGI(TAG, "Added domain: %s", domain);
    return added;
}

bool blocklist_remove(const char *domain_raw) {
    if (!s_blocklist || !domain_raw) return false;

    char domain[MAX_DOMAIN_LEN];
    normalize_domain(domain_raw, domain, sizeof(domain));

    bool removed = false;
    int removed_index = -1;
    char saved[MAX_DOMAIN_LEN] = {0};
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < s_count; i++) {
        if (strcasecmp(domain, s_blocklist[i]) == 0) {
            memcpy(saved, s_blocklist[i], sizeof(saved));
            // shift remaining entries down to fill the gap
            for (int j = i; j < s_count - 1; j++) {
                memcpy(s_blocklist[j], s_blocklist[j + 1], MAX_DOMAIN_LEN);
            }
            memset(s_blocklist[s_count - 1], 0, MAX_DOMAIN_LEN);
            s_count--;
            removed = true;
            removed_index = i;
            break;
        }
    }
    if (removed && persist_all_locked() != ESP_OK) {
        // Restore the exact in-memory list if flash persistence failed.
        for (int j = s_count; j > removed_index; --j) memcpy(s_blocklist[j], s_blocklist[j - 1], MAX_DOMAIN_LEN);
        memcpy(s_blocklist[removed_index], saved, MAX_DOMAIN_LEN);
        s_count++;
        removed = false;
    }
    xSemaphoreGive(s_mutex);

    if (removed) ESP_LOGI(TAG, "Removed domain: %s", domain);
    return removed;
}

bool blocklist_is_blocked(const char *query_domain_raw) {
    if (!s_blocklist || !query_domain_raw) return false;

    char q[MAX_DOMAIN_LEN];
    normalize_domain(query_domain_raw, q, sizeof(q));
    size_t qlen = strlen(q);

    bool blocked = false;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < s_count; i++) {
        size_t elen = strlen(s_blocklist[i]);
        if (elen == 0) continue;
        if (strcasecmp(q, s_blocklist[i]) == 0) { blocked = true; break; }
        // Subdomain check: q must end with "." + entry
        if (qlen > elen + 1) {
            const char *suffix_start = q + (qlen - elen);
            if (q[qlen - elen - 1] == '.' && strcasecmp(suffix_start, s_blocklist[i]) == 0) {
                blocked = true;
                break;
            }
        }
    }
    xSemaphoreGive(s_mutex);
    return blocked;
}

int blocklist_count(void) {
    int c;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    c = s_count;
    xSemaphoreGive(s_mutex);
    return c;
}

int blocklist_get_all(char out[][MAX_DOMAIN_LEN], int max) {
    int n;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    n = (s_count < max) ? s_count : max;
    for (int i = 0; i < n; i++) {
        memcpy(out[i], s_blocklist[i], MAX_DOMAIN_LEN);
    }
    xSemaphoreGive(s_mutex);
    return n;
}
