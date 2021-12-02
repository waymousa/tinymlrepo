#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sntp.h"
#include "sntp.h"

#include "wifi.h"
#include "sntp_sync.h"

static const char *TAG = "SNTP";

void obtain_time(void);
void initialize_sntp(void);

void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

void sntp_task(void *args)
{
    sntp_event_group = xEventGroupCreate();
    xEventGroupClearBits(sntp_event_group, TIMESET_BIT);
    xEventGroupSetBits(sntp_event_group, TIME_NOT_SET_BIT);

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    obtain_time();
    
    xEventGroupClearBits(sntp_event_group, TIME_NOT_SET_BIT);
    xEventGroupSetBits(sntp_event_group, TIMESET_BIT);
    
    while(1){
        vTaskDelay(1000);
    }

}

void obtain_time(void)
{
    initialize_sntp();
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000/portTICK_PERIOD_MS);
    }
    time(&now);
    localtime_r(&now, &timeinfo);
}

void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();
}

