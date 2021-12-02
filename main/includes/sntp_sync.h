
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"

EventGroupHandle_t sntp_event_group;

#define TIMESET_BIT BIT0
#define TIME_NOT_SET_BIT BIT1

void sntp_task(void *args);