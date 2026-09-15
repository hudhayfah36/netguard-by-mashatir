#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_sntp.h"
#include "config_store.h"
#include "time_service.h"

static const char *TAG = "time_service";

static void apply_timezone(void) {
    // POSIX TZ has the opposite sign: UTC-5 means UTC+05:00.
    int offset = g_net_cfg.utc_offset_minutes;
    char tz[24];
    int hours = abs(offset) / 60;
    int minutes = abs(offset) % 60;
    snprintf(tz, sizeof(tz), "UTC%c%d:%02d", offset >= 0 ? '-' : '+', hours, minutes);
    setenv("TZ", tz, 1);
    tzset();
}

void time_service_start(void) {
    apply_timezone();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "time.cloudflare.com");
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP started; scheduled sites fail closed until synchronized");
}

bool time_service_is_valid(void) {
    return time(NULL) >= 1700000000; // later than 2023-11-14
}

int time_service_local_minute(void) {
    time_t now = time(NULL);
    struct tm local = {0};
    localtime_r(&now, &local);
    return local.tm_hour * 60 + local.tm_min;
}

void time_service_set_utc_offset_minutes(int offset_minutes) {
    if (offset_minutes < -720 || offset_minutes > 840) return;
    g_net_cfg.utc_offset_minutes = offset_minutes;
    apply_timezone();
}

int time_service_get_utc_offset_minutes(void) { return g_net_cfg.utc_offset_minutes; }
