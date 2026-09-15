#pragma once
#include <stdbool.h>

#define MAX_BLOCKLIST   200
#define MAX_DOMAIN_LEN  128

// Allocates the blocklist buffer (preferring PSRAM), creates the guarding
// mutex, and loads any previously-saved entries from NVS. Must be called
// once during app_main() before the DNS engine or web server start.
void blocklist_init(void);

// Adds a domain (case-insensitive, trailing dot stripped). Returns false on
// duplicate, full list, or invalid input. Persists to NVS on success.
bool blocklist_add(const char *domain);

// Removes a domain. Returns false if not found. Persists to NVS on success.
bool blocklist_remove(const char *domain);

// Thread-safe lookup. Matches exact domain OR any subdomain of a blocked
// entry (e.g. blocking "example.com" also blocks "ads.example.com").
bool blocklist_is_blocked(const char *domain);

// Current number of entries.
int blocklist_count(void);

// Copies up to `max` domain strings into `out` (each MAX_DOMAIN_LEN bytes).
// Returns the number of entries copied. Used by the web server to render
// the current list.
int blocklist_get_all(char out[][MAX_DOMAIN_LEN], int max);
