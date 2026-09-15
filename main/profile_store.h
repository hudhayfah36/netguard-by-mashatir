#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "schedule_store.h"

#define MAX_PROFILES 16
#define PROFILE_NAME_LEN 32
#define MAX_PROFILE_DOMAINS 16
#define MAX_PROFILE_SCHEDULES 16
#define MAX_PROFILE_ALERTS 20
#define PROFILE_DOMAIN_LEN 64

typedef struct { uint32_t when; char category[24]; char signal[16]; } profile_alert_t;
typedef struct {
    bool used; char name[PROFILE_NAME_LEN]; uint32_t ip_be;
    char blocks[MAX_PROFILE_DOMAINS][PROFILE_DOMAIN_LEN]; uint8_t block_count;
    site_schedule_t schedules[MAX_PROFILE_SCHEDULES]; uint8_t schedule_count;
    uint32_t total_queries, blocked_queries;
    profile_alert_t alerts[MAX_PROFILE_ALERTS]; uint8_t alert_head, alert_count;
} device_profile_t;

void profile_store_init(void);
int profile_store_find_ip(uint32_t ip_be);
int profile_store_get_all(device_profile_t out[], int max);
bool profile_store_add(const char *name, const char *ip);
bool profile_store_remove(int id);
bool profile_store_add_block(int id, const char *domain);
bool profile_store_remove_block(int id, const char *domain);
bool profile_store_add_schedule(int id, const char *domain, int start, int end);
bool profile_store_remove_schedule(int id, const char *domain);
bool profile_store_query(int id, const char *domain, bool clock_valid, int minute, bool *allowed);
bool profile_store_is_blocked(int id, const char *domain);
void profile_store_count_query(int id, bool blocked);
void profile_store_indicator(int id, const char *domain, uint32_t when);
