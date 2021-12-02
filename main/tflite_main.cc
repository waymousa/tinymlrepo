#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "main_functions.h"

#include "wifi.h"

QueueHandle_t xQueueMqttData;

extern "C" void app_main_tflite( void *pvParams)
{
  xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
  xQueueMqttData = xQueueCreate(32,sizeof(uint32_t));
  setup();
  while (true) {
    loop();
    vTaskDelay ( pdMS_TO_TICKS( 20 ) );
  }
}

