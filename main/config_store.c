#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "config_store.h"

static const char *TAG = "config_store";
#define NVS_NS "netcfg"

net_config_t g_net_cfg = {
    .ssid = "",
    .password = "",
    .local_ip = "192.168.1.200",
    .gateway  = "192.168.1.1",
    .subnet   = "255.255.255.0",
    .use_static = false,
    .utc_offset_minutes = 300,
};

// Small helper: read a string value from NVS into a fixed-size buffer,
// leaving `dst` untouched (with its default) if the key doesn't exist yet.
static void nvs_get_str_or_default(nvs_handle_t h, const char *key, char *dst, size_t dst_size) {
    size_t len = dst_size;
    esp_err_t err = nvs_get_str(h, key, dst, &len);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "nvs_get_str(%s) failed: %s", key, esp_err_to_name(err));
    }
    // ESP_ERR_NVS_NOT_FOUND: dst keeps its pre-existing default value.
}

void config_store_load(void) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READONLY, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No existing '%s' namespace yet (err=%s) - using defaults.",
                 NVS_NS, esp_err_to_name(err));
        return;
    }

    nvs_get_str_or_default(h, "ssid", g_net_cfg.ssid, sizeof(g_net_cfg.ssid));
    nvs_get_str_or_default(h, "pass", g_net_cfg.password, sizeof(g_net_cfg.password));
    nvs_get_str_or_default(h, "ip",   g_net_cfg.local_ip, sizeof(g_net_cfg.local_ip));
    nvs_get_str_or_default(h, "gw",   g_net_cfg.gateway, sizeof(g_net_cfg.gateway));
    nvs_get_str_or_default(h, "sn",   g_net_cfg.subnet, sizeof(g_net_cfg.subnet));

    uint8_t use_static_u8 = g_net_cfg.use_static ? 1 : 0;
    nvs_get_u8(h, "static", &use_static_u8);
    g_net_cfg.use_static = (use_static_u8 != 0);
    int16_t offset = 300;
    if (nvs_get_i16(h, "tzoff", &offset) == ESP_OK && offset >= -720 && offset <= 840) g_net_cfg.utc_offset_minutes = offset;

    nvs_close(h);
    ESP_LOGI(TAG, "Loaded config: ssid='%s' static=%d ip=%s gw=%s sn=%s",
             g_net_cfg.ssid, g_net_cfg.use_static, g_net_cfg.local_ip,
             g_net_cfg.gateway, g_net_cfg.subnet);
}

esp_err_t config_store_save(void) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace '%s' for writing: %s",
                  NVS_NS, esp_err_to_name(err));
        return err;
    }
    err = nvs_set_str(h, "ssid", g_net_cfg.ssid);
    if (err == ESP_OK) err = nvs_set_str(h, "pass", g_net_cfg.password);
    if (err == ESP_OK) err = nvs_set_str(h, "ip", g_net_cfg.local_ip);
    if (err == ESP_OK) err = nvs_set_str(h, "gw", g_net_cfg.gateway);
    if (err == ESP_OK) err = nvs_set_str(h, "sn", g_net_cfg.subnet);
    if (err == ESP_OK) err = nvs_set_u8(h, "static", g_net_cfg.use_static ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_i16(h, "tzoff", g_net_cfg.utc_offset_minutes);
    if (err == ESP_OK) err = nvs_commit(h);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Network config committed to flash.");
    }
    nvs_close(h);
    return err;
}
