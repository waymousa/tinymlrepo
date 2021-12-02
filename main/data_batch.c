#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "core2forAWS.h"
#include "wifi.h"
#include "sntp_sync.h"
#include "data_batch.h"

static const char *TAG = "DAT";

extern QueueHandle_t xQueueAccelData;
QueueHandle_t xQueueBatchData;

void data_batch_task(void *args) 
{
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    xEventGroupWaitBits(sntp_event_group, TIMESET_BIT, false, true, portMAX_DELAY);
    
    struct acceldata { 
        long long ts; 
        float ax,ay,az;   
    } acceldata1;

    xQueueBatchData = xQueueCreate(8,BUFFER_RECORDS*sizeof(struct acceldata)); 

    // Allocate a buffer
    unsigned char * mqtt_buffer = heap_caps_malloc(BUFFER_RECORDS*sizeof(struct acceldata), MALLOC_CAP_SPIRAM);
    if (mqtt_buffer == NULL) {
        ESP_LOGE(TAG, "Cannot malloc buffer");
        return;
    }

    int cursor = 0;

    while(1){

        if(xQueueAccelData != 0)
        {
            if (xQueueReceive(xQueueAccelData,&acceldata1,(TickType_t)10))
            {
                // Copy struct to buffer
                for(size_t i=0; i<sizeof(struct acceldata);i++)
                {
                    mqtt_buffer[cursor] = ((unsigned char*)&acceldata1)[i];
                    cursor++;
                }
                
                // If buffer is full send it to another queue
                if (cursor ==  BUFFER_RECORDS*sizeof(struct acceldata))
                {
                    
                    ESP_LOGW(TAG, "Buffer full");
                          
                    xQueueSend(xQueueBatchData,&mqtt_buffer,portMAX_DELAY);
                    
                    cursor = 0;

                    ESP_LOGW(TAG, "Buffer reset");
                }
                
                char buffer[50];
                sprintf(buffer, "%lli,%.4f,%.4f,%.4f", acceldata1.ts, acceldata1.ax, acceldata1.ay, acceldata1.az);
                ESP_LOGD(TAG, "%s", buffer);
                
            }
        }

    }

}
