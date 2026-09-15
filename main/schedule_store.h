#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "blocklist.h"

#define MAX_SCHEDULES 64

typedef struct {
    char domain[MAX_DOMAIN_LEN];
    uint16_t start_minute; // inclusive, local time, 0..1439
    uint16_t end_minute;   // exclusive, local time, 0..1439
    bool enabled;
} site_schedule_t;

void schedule_store_init(void);
bool schedule_store_add(const char *domain, int start_minute, int end_minute);
bool schedule_store_remove(const char *domain);
bool schedule_store_set_enabled(const char *domain, bool enabled);
int schedule_store_get_all(site_schedule_t out[], int max);
// Returns true when an enabled matching schedule exists. allowed is meaningful
// only when a match exists; an invalid clock always produces allowed=false.
bool schedule_store_query(const char *domain, bool clock_valid, int local_minute, bool *allowed);
