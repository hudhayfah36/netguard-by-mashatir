/*
 * ============================================================================
 *  ESP32-S3 DNS SINKHOLE & PARENTAL CONTROL APPLIANCE  (native ESP-IDF)
 * ============================================================================
 *  Target: ESP32-S3-N16R8 (16 MB Flash / 8 MB Octal PSRAM)
 *
 *  Build:
 *      idf.py set-target esp32s3
 *      idf.py menuconfig     (confirm PSRAM/OPI settings from sdkconfig.defaults)
 *      idf.py build flash monitor
 *
 *  Architecture
 *  ----------------------------------------------------------------------
 *  - Core 0 : dns_engine task -- raw UDP socket on port 53, parses queries,
 *             checks the blocklist, replies NXDOMAIN or forwards upstream.
 *  - Core 1 : esp_http_server (pinned via httpd_config_t.core_id) serving
 *             the dashboard + REST API. Fully event-driven / non-blocking.
 *  - Shared state (blocklist, counters) is PSRAM-backed where practical and
 *    guarded by FreeRTOS mutexes inside blocklist.c.
 * ============================================================================
 */

#include <stdio.h>
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_psram.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config_store.h"
#include "blocklist.h"
#include "wifi_manager.h"
#include "dns_engine.h"
#include "web_server.h"
#include "auth.h"
#include "schedule_store.h"
#include "time_service.h"
#include "profile_store.h"

static const char *TAG = "main";

// Periodically logs system health -- useful for confirming no leaks/
// fragmentation over long uptimes (the original design goal of keeping
// the blocklist in PSRAM rather than churn-prone internal heap).
static void stats_task(void *arg) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
        ESP_LOGI(TAG, "STATS | Queries: %u | Blocked: %u | Free Heap: %u | Free PSRAM: %u | Blocklist: %d",
                 (unsigned)g_total_queries,
                 (unsigned)g_blocked_queries,
                 (unsigned)esp_get_free_heap_size(),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                 blocklist_count());
    }
}

void app_main(void) {
    ESP_LOGI(TAG, "=== NetGuard by Mashatir booting ===");

    // ---- NVS init (required before any Preferences-style storage or Wi-Fi) ----
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // ---- PSRAM sanity check ----
    if (esp_psram_is_initialized()) {
        ESP_LOGI(TAG, "PSRAM ready. Total: %u bytes, Free: %u bytes",
                 (unsigned)heap_caps_get_total_size(MALLOC_CAP_SPIRAM),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    } else {
        ESP_LOGW(TAG, "WARNING: PSRAM not initialized! Check menuconfig (Octal PSRAM) "
                      "and that CONFIG_SPIRAM=y is set. Falling back to internal heap.");
    }

    // ---- Network stack + default event loop (required by esp_wifi/esp_netif) ----
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // ---- Load persisted configuration ----
    config_store_load();
    auth_init();
    blocklist_init();
    schedule_store_init();
    profile_store_init();

    // ---- Bring up Wi-Fi (static IP / DHCP / fallback AP handled internally) ----
    wifi_manager_init();
    time_service_start();

    // ---- Start the DNS engine (Core 0) ----
    dns_engine_start();

    // ---- Start the web dashboard (Core 1) ----
    web_server_start();

    // ---- Periodic stats logger ----
    xTaskCreate(stats_task, "stats_task", 4096, NULL, 1, NULL);

    ESP_LOGI(TAG, "=== Setup complete ===");
}
