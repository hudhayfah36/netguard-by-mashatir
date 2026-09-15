#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

// Persisted network configuration (stored in NVS namespace "netcfg")
typedef struct {
    char ssid[64];
    char password[64];
    char local_ip[16];   // dotted-decimal strings
    char gateway[16];
    char subnet[16];
    bool use_static;      // true => apply local_ip/gateway/subnet, false => DHCP
    int16_t utc_offset_minutes;
} net_config_t;

// Global, in-RAM copy of the current configuration. Loaded at boot by
// config_store_load(). Other modules (wifi_manager, web_server) read/write
// this directly; config_store_save() is what actually commits it to flash.
extern net_config_t g_net_cfg;

// Loads g_net_cfg from NVS. If no config exists yet, leaves safe defaults
// (empty SSID, DHCP mode) so the caller can detect "unconfigured" state.
void config_store_load(void);

// Persists the current contents of g_net_cfg to NVS.
esp_err_t config_store_save(void);
