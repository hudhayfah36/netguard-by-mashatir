#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <stdio.h>
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "schedule_store.h"

#define SCHEDULE_NS "schedule"
static site_schedule_t s_entries[MAX_SCHEDULES];
static int s_count;
static SemaphoreHandle_t s_mutex;

static bool normalize(const char *in, char out[MAX_DOMAIN_LEN]) {
    if (!in) return false;
    size_t n = strnlen(in, MAX_DOMAIN_LEN); if (!n || n == MAX_DOMAIN_LEN) return false;
    if (in[n - 1] == '.') --n;
    if (!n) return false;
    bool label_start = true; int label_len = 0;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)in[i];
        if (c == '.') { if (label_start || label_len > 63 || out[i - 1] == '-') return false; label_start = true; label_len = 0; out[i] = '.'; continue; }
        if (!(isalnum(c) || c == '-') || (label_start && c == '-')) return false;
        out[i] = (char)tolower(c); label_start = false; ++label_len;
    }
    if (label_start || label_len > 63 || out[n - 1] == '-') return false;
    out[n] = '\0'; return true;
}
static bool match(const char *q, const char *e) { size_t ql = strlen(q), el = strlen(e); return strcasecmp(q, e) == 0 || (ql > el && q[ql - el - 1] == '.' && strcasecmp(q + ql - el, e) == 0); }
static esp_err_t persist_locked(void) {
    nvs_handle_t h; esp_err_t err = nvs_open(SCHEDULE_NS, NVS_READWRITE, &h); if (err != ESP_OK) return err;
    int32_t old = 0; nvs_get_i32(h, "count", &old); if (old < 0 || old > MAX_SCHEDULES) old = 0;
    err = nvs_set_i32(h, "count", s_count);
    char key[16];
    for (int i = 0; i < s_count && err == ESP_OK; ++i) { snprintf(key, sizeof(key), "e%d", i); err = nvs_set_blob(h, key, &s_entries[i], sizeof(s_entries[i])); }
    for (int i = s_count; i < old && err == ESP_OK; ++i) { snprintf(key, sizeof(key), "e%d", i); err = nvs_erase_key(h, key); }
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}
void schedule_store_init(void) {
    s_mutex = xSemaphoreCreateMutex(); if (!s_mutex) return;
    nvs_handle_t h; if (nvs_open(SCHEDULE_NS, NVS_READONLY, &h) != ESP_OK) return;
    int32_t count = 0; nvs_get_i32(h, "count", &count); if (count < 0) count = 0; if (count > MAX_SCHEDULES) count = MAX_SCHEDULES;
    for (int i = 0; i < count; ++i) {
        char key[16]; size_t len = sizeof(s_entries[0]); snprintf(key, sizeof(key), "e%d", i);
        if (nvs_get_blob(h, key, &s_entries[s_count], &len) == ESP_OK && len == sizeof(s_entries[0]) && s_entries[s_count].start_minute < 1440 && s_entries[s_count].end_minute < 1440) ++s_count;
    }
    nvs_close(h); ESP_LOGI("schedule", "Loaded %d schedules", s_count);
}
bool schedule_store_add(const char *raw, int start, int end) {
    site_schedule_t e = {.start_minute = start, .end_minute = end, .enabled = true};
    if (!normalize(raw, e.domain) || blocklist_is_blocked(e.domain) || start < 0 || start >= 1440 || end < 0 || end >= 1440 || start == end) return false;
    xSemaphoreTake(s_mutex, portMAX_DELAY); bool ok = s_count < MAX_SCHEDULES;
    for (int i = 0; i < s_count; ++i) if (strcasecmp(e.domain, s_entries[i].domain) == 0) ok = false;
    if (ok) { s_entries[s_count++] = e; ok = persist_locked() == ESP_OK; if (!ok) --s_count; } xSemaphoreGive(s_mutex); return ok;
}
bool schedule_store_remove(const char *raw) {
    char domain[MAX_DOMAIN_LEN]; if (!normalize(raw, domain)) return false; xSemaphoreTake(s_mutex, portMAX_DELAY); bool ok = false;
    for (int i = 0; i < s_count; ++i) {
        if (strcasecmp(domain, s_entries[i].domain) == 0) {
            site_schedule_t saved = s_entries[i];
            memmove(&s_entries[i], &s_entries[i + 1], (s_count - i - 1) * sizeof(s_entries[0])); --s_count;
            ok = persist_locked() == ESP_OK;
            if (!ok) { memmove(&s_entries[i + 1], &s_entries[i], (s_count - i) * sizeof(s_entries[0])); s_entries[i] = saved; ++s_count; }
            break;
        }
    }
    xSemaphoreGive(s_mutex); return ok;
}
bool schedule_store_set_enabled(const char *raw, bool enabled) {
    char domain[MAX_DOMAIN_LEN]; if (!normalize(raw, domain)) return false; xSemaphoreTake(s_mutex, portMAX_DELAY); bool ok = false;
    for (int i = 0; i < s_count; ++i) {
        if (strcasecmp(domain, s_entries[i].domain) == 0) { bool old = s_entries[i].enabled; s_entries[i].enabled = enabled; ok = persist_locked() == ESP_OK; if (!ok) s_entries[i].enabled = old; break; }
    }
    xSemaphoreGive(s_mutex); return ok;
}
int schedule_store_get_all(site_schedule_t out[], int max) { xSemaphoreTake(s_mutex, portMAX_DELAY); int n = s_count < max ? s_count : max; memcpy(out, s_entries, n * sizeof(*out)); xSemaphoreGive(s_mutex); return n; }
bool schedule_store_query(const char *raw, bool clock_valid, int minute, bool *allowed) {
    char domain[MAX_DOMAIN_LEN]; if (!normalize(raw, domain)) return false; xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (int i = 0; i < s_count; ++i) {
        if (s_entries[i].enabled && match(domain, s_entries[i].domain)) {
            bool in = s_entries[i].start_minute < s_entries[i].end_minute ? minute >= s_entries[i].start_minute && minute < s_entries[i].end_minute : minute >= s_entries[i].start_minute || minute < s_entries[i].end_minute;
            *allowed = clock_valid && in; xSemaphoreGive(s_mutex); return true;
        }
    }
    xSemaphoreGive(s_mutex); return false;
}
