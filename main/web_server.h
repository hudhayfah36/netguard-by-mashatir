#pragma once

// Starts esp_http_server with the dashboard + REST routes registered.
// Runs its own internal task (esp_http_server manages this); we pin it to
// Core 1 via httpd_config_t so it never contends with the DNS engine on Core 0.
void web_server_start(void);
