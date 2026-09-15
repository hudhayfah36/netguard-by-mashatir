#pragma once
#include <stdbool.h>

// Initializes esp_netif + Wi-Fi in station mode using g_net_cfg (loaded via
// config_store_load() beforehand). Applies static IP if g_net_cfg.use_static
// is set, otherwise uses DHCP. If g_net_cfg.ssid is empty, starts a fallback
// WPA2 SoftAP ("NetGuard-Mashatir Setup") instead so the device stays reachable
// for initial configuration via the web GUI.
//
// Blocks (up to ~15s) waiting for STA connection before returning.
// Returns true if STA connected successfully, false if it fell back to AP
// mode or timed out.
bool wifi_manager_init(void);

// Returns the current station IP as a dotted-decimal string, or the SoftAP
// IP if running in fallback mode. Buffer must be at least 16 bytes.
void wifi_manager_get_ip_str(char *out, size_t out_size);

// Last station disconnection reason from ESP-IDF; 0 means no recorded failure.
int wifi_manager_get_last_disconnect_reason(void);
