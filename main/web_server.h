#pragma once

// Starts the local setup/API HTTP service. The mobile app owns the frontend.
// Runs its own internal task (esp_http_server manages this); we pin it to
// Core 1 via httpd_config_t so it never contends with the DNS engine on Core 0.
void web_server_start(void);
