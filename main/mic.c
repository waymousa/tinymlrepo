#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/i2s.h"
#include "core2forAWS.h"
#include "mic.h"
#include "wifi.h"
#include "ui.h"
#include "sntp_sync.h"
#include "microphone.h"

#define MIC_QUEUE_DEPTH 10
#define MIC_BUFF_SIZE 1024*1000

static const char *TAG = "MIC";

QueueHandle_t xQueueMicData;

void mic_task(void *args) {

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    xEventGroupWaitBits(sntp_event_group, TIMESET_BIT, false, true, portMAX_DELAY);

    Microphone_Init();
    
    xQueueMicData = xQueueCreate(MIC_QUEUE_DEPTH,sizeof(char *)); 
    size_t bytesread;

    while(1){
        // allocate memory in PSRAM and return its pointer
        int8_t * i2s_readraw_buff = heap_caps_malloc(MIC_BUFF_SIZE, MALLOC_CAP_SPIRAM);
        // dump mic data in the buffer
        i2s_read(I2S_NUM_0, (char*)i2s_readraw_buff, 
            MIC_BUFF_SIZE, &bytesread, pdMS_TO_TICKS(100));
        // send the pointer to a queue
        xQueueSend(xQueueMicData,&i2s_readraw_buff,portMAX_DELAY);
        // 
        ESP_LOGI(TAG, "bytesread: %i", bytesread);

    }

}
