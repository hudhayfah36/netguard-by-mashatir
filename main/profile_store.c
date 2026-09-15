#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include "nvs.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "profile_store.h"

#define NS "profiles"
typedef struct { bool used; char name[PROFILE_NAME_LEN]; uint32_t ip_be; char blocks[MAX_PROFILE_DOMAINS][PROFILE_DOMAIN_LEN]; uint8_t block_count; site_schedule_t schedules[MAX_PROFILE_SCHEDULES]; uint8_t schedule_count; } persisted_profile_t;
static device_profile_t *s_profiles; static SemaphoreHandle_t s_lock;

static bool domain(const char *in, char out[PROFILE_DOMAIN_LEN]) {
    if (!in) return false;
    size_t n = strnlen(in, PROFILE_DOMAIN_LEN);
    if (!n || n == PROFILE_DOMAIN_LEN) return false;
    if (in[n - 1] == '.') --n;
    if (!n) return false;
    int label = 0;
    for (size_t i=0;i<n;i++) { unsigned char c=in[i]; if (c=='.') { if (!label || label>63 || out[i-1]=='-') return false; label=0; out[i]='.'; } else { if (!(isalnum(c)||c=='-') || (!label&&c=='-')) return false; out[i]=tolower(c); ++label; } }
    if (!label || label>63 || out[n-1]=='-') return false;
    out[n]='\0'; return true;
}
static bool suffix(const char *q,const char *e) { size_t ql=strlen(q),el=strlen(e); return !strcasecmp(q,e)||(ql>el&&q[ql-el-1]=='.'&&!strcasecmp(q+ql-el,e)); }
static bool save_locked(int id) { persisted_profile_t p={0}; device_profile_t *s=&s_profiles[id]; p.used=s->used; memcpy(p.name,s->name,sizeof(p.name)); p.ip_be=s->ip_be; memcpy(p.blocks,s->blocks,sizeof(p.blocks)); p.block_count=s->block_count; memcpy(p.schedules,s->schedules,sizeof(p.schedules)); p.schedule_count=s->schedule_count; char key[8]; snprintf(key,sizeof(key),"p%d",id); nvs_handle_t h; if(nvs_open(NS,NVS_READWRITE,&h)!=ESP_OK)return false; esp_err_t e=nvs_set_blob(h,key,&p,sizeof(p)); if(e==ESP_OK)e=nvs_commit(h); nvs_close(h); return e==ESP_OK; }
void profile_store_init(void) { s_lock=xSemaphoreCreateMutex(); s_profiles=heap_caps_calloc(MAX_PROFILES,sizeof(*s_profiles),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT); if(!s_profiles)s_profiles=calloc(MAX_PROFILES,sizeof(*s_profiles)); if(!s_profiles)return; nvs_handle_t h; if(nvs_open(NS,NVS_READONLY,&h)!=ESP_OK)return; for(int i=0;i<MAX_PROFILES;i++){persisted_profile_t p={0};size_t n=sizeof(p);char key[8];snprintf(key,sizeof(key),"p%d",i);if(nvs_get_blob(h,key,&p,&n)==ESP_OK&&n==sizeof(p)&&p.used&&p.block_count<=MAX_PROFILE_DOMAINS&&p.schedule_count<=MAX_PROFILE_SCHEDULES){s_profiles[i].used=true;memcpy(s_profiles[i].name,p.name,sizeof(p.name));s_profiles[i].ip_be=p.ip_be;memcpy(s_profiles[i].blocks,p.blocks,sizeof(p.blocks));s_profiles[i].block_count=p.block_count;memcpy(s_profiles[i].schedules,p.schedules,sizeof(p.schedules));s_profiles[i].schedule_count=p.schedule_count;}} nvs_close(h); }
int profile_store_find_ip(uint32_t ip_be){if(!s_profiles)return -1;xSemaphoreTake(s_lock,portMAX_DELAY);for(int i=0;i<MAX_PROFILES;i++)if(s_profiles[i].used&&s_profiles[i].ip_be==ip_be){xSemaphoreGive(s_lock);return i;}xSemaphoreGive(s_lock);return -1;}
int profile_store_get_all(device_profile_t out[],int max){if(!s_profiles)return 0;xSemaphoreTake(s_lock,portMAX_DELAY);int n=max<MAX_PROFILES?max:MAX_PROFILES;memcpy(out,s_profiles,n*sizeof(*out));xSemaphoreGive(s_lock);return n;}
bool profile_store_add(const char *name,const char *ip){if(!s_profiles||!name||strnlen(name,PROFILE_NAME_LEN)>=PROFILE_NAME_LEN)return false;struct in_addr a;if(inet_pton(AF_INET,ip,&a)!=1)return false;xSemaphoreTake(s_lock,portMAX_DELAY);int id=-1;for(int i=0;i<MAX_PROFILES;i++){if(s_profiles[i].used&&(!strcasecmp(s_profiles[i].name,name)||s_profiles[i].ip_be==a.s_addr)){xSemaphoreGive(s_lock);return false;}if(!s_profiles[i].used&&id<0)id=i;}if(id>=0){memset(&s_profiles[id],0,sizeof(*s_profiles));s_profiles[id].used=true;strncpy(s_profiles[id].name,name,PROFILE_NAME_LEN-1);s_profiles[id].ip_be=a.s_addr;if(!save_locked(id))memset(&s_profiles[id],0,sizeof(*s_profiles));}xSemaphoreGive(s_lock);return id>=0&&s_profiles[id].used;}
bool profile_store_remove(int id){if(id<0||id>=MAX_PROFILES||!s_profiles)return false;xSemaphoreTake(s_lock,portMAX_DELAY);if(!s_profiles[id].used){xSemaphoreGive(s_lock);return false;}device_profile_t old=s_profiles[id];memset(&s_profiles[id],0,sizeof(old));bool ok=save_locked(id);if(!ok)s_profiles[id]=old;xSemaphoreGive(s_lock);return ok;}
bool profile_store_add_block(int id,const char *raw){char d[PROFILE_DOMAIN_LEN];if(id<0||id>=MAX_PROFILES||!domain(raw,d))return false;xSemaphoreTake(s_lock,portMAX_DELAY);device_profile_t*s=&s_profiles[id];bool ok=s->used&&s->block_count<MAX_PROFILE_DOMAINS;for(int i=0;i<s->block_count;i++)if(!strcasecmp(s->blocks[i],d))ok=false;if(ok){strcpy(s->blocks[s->block_count++],d);ok=save_locked(id);if(!ok)--s->block_count;}xSemaphoreGive(s_lock);return ok;}
bool profile_store_remove_block(int id,const char *raw){char d[PROFILE_DOMAIN_LEN];if(id<0||id>=MAX_PROFILES||!domain(raw,d))return false;xSemaphoreTake(s_lock,portMAX_DELAY);device_profile_t*s=&s_profiles[id];bool ok=false;for(int i=0;i<s->block_count;i++)if(!strcasecmp(s->blocks[i],d)){memmove(&s->blocks[i],&s->blocks[i+1],(s->block_count-i-1)*PROFILE_DOMAIN_LEN);--s->block_count;ok=save_locked(id);break;}xSemaphoreGive(s_lock);return ok;}
bool profile_store_add_schedule(int id,const char*raw,int start,int end){char d[PROFILE_DOMAIN_LEN];if(id<0||id>=MAX_PROFILES||!domain(raw,d)||start<0||end<0||start>=1440||end>=1440||start==end)return false;xSemaphoreTake(s_lock,portMAX_DELAY);device_profile_t*s=&s_profiles[id];bool ok=s->used&&s->schedule_count<MAX_PROFILE_SCHEDULES;for(int i=0;i<s->schedule_count;i++)if(!strcasecmp(s->schedules[i].domain,d))ok=false;if(ok){site_schedule_t*e=&s->schedules[s->schedule_count++];memset(e,0,sizeof(*e));strncpy(e->domain,d,MAX_DOMAIN_LEN-1);e->start_minute=start;e->end_minute=end;e->enabled=true;ok=save_locked(id);if(!ok)--s->schedule_count;}xSemaphoreGive(s_lock);return ok;}
bool profile_store_remove_schedule(int id,const char*raw){char d[PROFILE_DOMAIN_LEN];if(id<0||id>=MAX_PROFILES||!domain(raw,d))return false;xSemaphoreTake(s_lock,portMAX_DELAY);device_profile_t*s=&s_profiles[id];bool ok=false;for(int i=0;i<s->schedule_count;i++)if(!strcasecmp(s->schedules[i].domain,d)){memmove(&s->schedules[i],&s->schedules[i+1],(s->schedule_count-i-1)*sizeof(site_schedule_t));--s->schedule_count;ok=save_locked(id);break;}xSemaphoreGive(s_lock);return ok;}
bool profile_store_is_blocked(int id,const char*raw){if(id<0||id>=MAX_PROFILES||!s_profiles)return false;char d[PROFILE_DOMAIN_LEN];if(!domain(raw,d))return false;xSemaphoreTake(s_lock,portMAX_DELAY);device_profile_t*s=&s_profiles[id];for(int i=0;i<s->block_count;i++)if(suffix(d,s->blocks[i])){xSemaphoreGive(s_lock);return true;}xSemaphoreGive(s_lock);return false;}
bool profile_store_query(int id,const char*raw,bool valid,int min,bool*allowed){if(id<0||id>=MAX_PROFILES||!s_profiles)return false;char d[PROFILE_DOMAIN_LEN];if(!domain(raw,d))return false;xSemaphoreTake(s_lock,portMAX_DELAY);device_profile_t*s=&s_profiles[id];for(int i=0;i<s->schedule_count;i++)if(s->schedules[i].enabled&&suffix(d,s->schedules[i].domain)){int a=s->schedules[i].start_minute,b=s->schedules[i].end_minute;*allowed=valid&&(a<b?(min>=a&&min<b):(min>=a||min<b));xSemaphoreGive(s_lock);return true;}xSemaphoreGive(s_lock);return false;}
void profile_store_count_query(int id,bool blocked){if(id<0||id>=MAX_PROFILES||!s_profiles)return;xSemaphoreTake(s_lock,portMAX_DELAY);s_profiles[id].total_queries++;if(blocked)s_profiles[id].blocked_queries++;xSemaphoreGive(s_lock);}
void profile_store_indicator(int id,const char*d,uint32_t when){static const struct{const char*domain;const char*category;} list[]={{"nordvpn.com","VPN"},{"expressvpn.com","VPN"},{"protonvpn.com","VPN"},{"surfshark.com","VPN"},{"mullvad.net","VPN"},{"windscribe.com","VPN"},{"openvpn.net","VPN"},{"wireguard.com","VPN"},{"cloudflare-dns.com","DoH"},{"dns.google","DoH"}};if(id<0||id>=MAX_PROFILES||!s_profiles)return;for(size_t i=0;i<sizeof(list)/sizeof(list[0]);i++)if(suffix(d,list[i].domain)){xSemaphoreTake(s_lock,portMAX_DELAY);device_profile_t*s=&s_profiles[id];profile_alert_t*a=&s->alerts[s->alert_head++%MAX_PROFILE_ALERTS];memset(a,0,sizeof(*a));a->when=when;strncpy(a->category,list[i].category,sizeof(a->category)-1);strcpy(a->signal,"domain lookup");if(s->alert_count<MAX_PROFILE_ALERTS)s->alert_count++;xSemaphoreGive(s_lock);break;}}
