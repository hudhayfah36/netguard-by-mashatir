#pragma once
#include <stdbool.h>
#include "esp_err.h"
#include "esp_http_server.h"

void auth_init(void);
bool auth_is_configured(void);
esp_err_t auth_set_password(const char *password);
bool auth_login(const char *password, char session_hex[33], char csrf_hex[33]);
void auth_logout(const char *session_hex);
bool auth_authorize(httpd_req_t *req, bool require_csrf);
const char *auth_session_csrf(const char *session_hex);
uint32_t auth_session_remaining_seconds(void);
