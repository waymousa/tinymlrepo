
#include <time.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_log.h"

#include "wifi.h"
#include "sntp_sync.h"

#include "mpu6886.h"

static const char *TAG = "MPU";

struct acceldata { 
    long long ts; 
    float ax,ay,az;   
} acceldata1;

QueueHandle_t xQueueAccelData;

static int64_t time_get_time() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL));
}

void mpu_task(void *args) {

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    xEventGroupWaitBits(sntp_event_group, TIMESET_BIT, false, true, portMAX_DELAY);

    xQueueAccelData = xQueueCreate(60,sizeof(struct acceldata)); 

    int sensor_status = MPU6886_Init();

    printf("MPU6886_Init status: %i \r\n", sensor_status);
    
    while(1){
        
        // wait 20ms
        vTaskDelay(pdMS_TO_TICKS(20));
        // get timestamp & accel data
        acceldata1.ts = time_get_time(); 
        MPU6886_GetAccelData(&acceldata1.ax, &acceldata1.ay, &acceldata1.az);
        // send collected data to a queue
        xQueueSend(xQueueAccelData,&acceldata1,portMAX_DELAY);

        char buffer[50];
        sprintf(buffer, "%lli,%.4f,%.4f,%.4f", acceldata1.ts, acceldata1.ax, acceldata1.ay, acceldata1.az);
        ESP_LOGD(TAG, "%s", buffer);

    }

}
