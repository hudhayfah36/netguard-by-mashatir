#include <string.h>
#include <stdio.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/ip4_addr.h"
#include "wifi_manager.h"
#include "config_store.h"

static const char *TAG = "wifi_manager";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT       BIT1
#define WIFI_CONNECT_TIMEOUT_MS 30000
#define WIFI_MAX_RETRY      10

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_count = 0;
static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif = NULL;
static bool s_ap_fallback_active = false;
static volatile bool s_sta_connected = false;
static volatile int s_last_disconnect_reason = 0;
#define SETUP_AP_PASSWORD "netguard@1234"

static void set_device_dns_resolvers(void) {
    esp_netif_dns_info_t dns = { .ip.type = ESP_IPADDR_TYPE_V4 };
    if (ip4addr_aton("1.1.1.3", (ip4_addr_t *)&dns.ip.u_addr.ip4)) {
        esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_MAIN, &dns);
    }
    if (ip4addr_aton("1.0.0.3", (ip4_addr_t *)&dns.ip.u_addr.ip4)) {
        esp_netif_set_dns_info(s_sta_netif, ESP_NETIF_DNS_BACKUP, &dns);
    }
}

static void event_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disconnected = (wifi_event_sta_disconnected_t *)event_data;
        s_sta_connected = false;
        s_last_disconnect_reason = disconnected ? disconnected->reason : -1;
        if (s_retry_count < WIFI_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_count++;
            ESP_LOGW(TAG, "Wi-Fi disconnected (reason=%d), retrying (%d/%d)...",
                     disconnected ? disconnected->reason : -1, s_retry_count, WIFI_MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        set_device_dns_resolvers(); // NTP must not depend on clients' NetGuard DNS assignment.
        s_retry_count = 0;
        s_last_disconnect_reason = 0;
        s_sta_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

// Starts a password-protected SoftAP so the device remains reachable at a known
// address (192.168.4.1) when no SSID has been configured yet.
static void start_fallback_ap(void) {
    if (s_ap_fallback_active) return;
    ESP_LOGW(TAG, "Starting setup AP 'NetGuard-Mashatir Setup'");

    s_ap_netif = esp_netif_create_default_wifi_ap();

    wifi_config_t ap_config = {
        .ap = {
            .ssid = "NetGuard-Mashatir Setup",
            .ssid_len = strlen("NetGuard-Mashatir Setup"),
            .password = "",
            .channel = 1,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    strncpy((char *)ap_config.ap.password, SETUP_AP_PASSWORD, sizeof(ap_config.ap.password) - 1);
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap_config);
    esp_wifi_start();
    s_ap_fallback_active = true;
}

bool wifi_manager_init(void) {
    s_wifi_event_group = xEventGroupCreate();

    s_sta_netif = esp_netif_create_default_wifi_sta();
    esp_netif_set_hostname(s_sta_netif, "netguard-mashatir");

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    if (strlen(g_net_cfg.ssid) == 0) {
        // No credentials saved yet -- go straight to fallback AP mode.
        start_fallback_ap();
        return false;
    }

    // ---- Apply static IP configuration (before connecting) if requested ----
    if (g_net_cfg.use_static) {
        esp_netif_dhcpc_stop(s_sta_netif);

        esp_netif_ip_info_t ip_info = {0};
        bool ok = ip4addr_aton(g_net_cfg.local_ip, (ip4_addr_t *)&ip_info.ip) &&
                  ip4addr_aton(g_net_cfg.gateway, (ip4_addr_t *)&ip_info.gw) &&
                  ip4addr_aton(g_net_cfg.subnet, (ip4_addr_t *)&ip_info.netmask);

        if (ok) {
            esp_netif_set_ip_info(s_sta_netif, &ip_info);
            ESP_LOGI(TAG, "Static IP configured: %s / GW %s / SN %s",
                     g_net_cfg.local_ip, g_net_cfg.gateway, g_net_cfg.subnet);
        } else {
            ESP_LOGW(TAG, "Static IP strings invalid, re-enabling DHCP.");
            esp_netif_dhcpc_start(s_sta_netif);
        }
    }

    wifi_config_t wifi_config = {0};
    size_t ssid_len = strnlen(g_net_cfg.ssid, sizeof(wifi_config.sta.ssid) - 1);
    size_t pass_len = strnlen(g_net_cfg.password, sizeof(wifi_config.sta.password) - 1);
    memcpy(wifi_config.sta.ssid, g_net_cfg.ssid, ssid_len);
    memcpy(wifi_config.sta.password, g_net_cfg.password, pass_len);
    // Scan every 2.4 GHz channel. Leave the auth-mode threshold at the ESP-IDF
    // default so WPA/WPA2 mixed-mode home routers are not rejected before auth.
    wifi_config.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to SSID '%s'...", g_net_cfg.ssid);

    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE,
        pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Wi-Fi connected successfully.");
        return true;
    }

    ESP_LOGE(TAG, "Failed to connect within timeout - starting fallback SoftAP.");
    start_fallback_ap();
    return false;
}

int wifi_manager_get_last_disconnect_reason(void) { return s_last_disconnect_reason; }

void wifi_manager_get_ip_str(char *out, size_t out_size) {
    esp_netif_ip_info_t ip_info;
    // A fallback AP may have started before a slow station connection later
    // succeeds. Always report the station address once it is actually online.
    esp_netif_t *netif = s_sta_connected ? s_sta_netif : (s_ap_fallback_active ? s_ap_netif : s_sta_netif);
    if (netif && esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        esp_ip4addr_ntoa(&ip_info.ip, out, out_size);
    } else {
        strncpy(out, "0.0.0.0", out_size - 1);
    }
}
