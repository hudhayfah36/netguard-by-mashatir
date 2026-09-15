#pragma once
#include <stdbool.h>
#include <stdint.h>

// Starts SNTP and applies the persisted fixed UTC offset. A time-limited
// domain must remain blocked until time_service_is_valid() is true.
void time_service_start(void);
bool time_service_is_valid(void);
int time_service_local_minute(void);
void time_service_set_utc_offset_minutes(int offset_minutes);
int time_service_get_utc_offset_minutes(void);
