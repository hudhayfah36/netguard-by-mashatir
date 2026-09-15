#pragma once
#include <stdint.h>

// Creates the UDP listening socket on port 53 and spawns the DNS engine
// task pinned to Core 0. Must be called after Wi-Fi is up and blocklist_init()
// has run. Non-blocking (the task runs independently).
void dns_engine_start(void);

// Running counters, safe to read from other tasks (32-bit aligned word
// reads/writes are atomic on Xtensa/RISC-V for this use case; for stats
// display purposes exact consistency isn't critical).
extern volatile uint32_t g_total_queries;
extern volatile uint32_t g_blocked_queries;
extern int64_t g_boot_time_us;
