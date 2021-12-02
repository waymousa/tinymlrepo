#include <time.h>
#include <string.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "core2forAWS.h"
#include "wifi.h"
#include "ui.h"
#include "sntp_sync.h"
#include "sdmmc_cmd.h"
#define MOUNT_POINT "/sdcard"
#define FILENAME_LENGTH 31
#define MIC_BUFF_SIZE 1024*1000
static const char *TAG = "SD";

extern QueueHandle_t xQueueMicData;

int64_t time_get_time() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (tv.tv_sec * 1000LL + (tv.tv_usec / 1000LL));
}

void sd_task(void *args) 
{
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    xEventGroupWaitBits(sntp_event_group, TIMESET_BIT, false, true, portMAX_DELAY);
    
    sdmmc_card_t * card;
    esp_err_t ret;

    time_t now;
    struct tm timeinfo;
    time(&now);

    // file name => /sdcard/YY-MM-DDTHH_MM_SSZ.csv
    char strftime_buf[FILENAME_LENGTH]; 
    setenv("TZ", "UTC", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), 
        MOUNT_POINT"/%y-%m-%dT%H_%M_%SZ.wav", &timeinfo);
    
    ret = Core2ForAWS_SDcard_Mount(MOUNT_POINT, &card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize the sd card");
        return;
    } 

    ESP_LOGI(TAG, "Success to initialize the sd card");
    sdmmc_card_print_info(stdout, card);
    
    // buffer pointer
    int8_t * i2s_readraw_buffPtr;
    while(1){

        if(xQueueMicData != 0)
        {
            if (xQueueReceive(xQueueMicData,&i2s_readraw_buffPtr,(TickType_t)10))
            {
                
                // the sd card and the screen share the SPI bus - take the mutex
                xSemaphoreTake(spi_mutex, portMAX_DELAY);
                spi_poll();
                // open the file
                ESP_LOGE(TAG, "Opening file %s", strftime_buf);
                FILE* f = fopen(strftime_buf, "w+");
                if (f == NULL) {
                    ESP_LOGE(TAG, "Failed to open file for writing, error : %d", errno);
                    xSemaphoreGive(spi_mutex);
                } else {
                    ESP_LOGE(TAG, "Succeeded to open file for writing");
                    // write the contents of the buffer
                    fwrite(i2s_readraw_buffPtr, 1, MIC_BUFF_SIZE, f);
                    // close the file
                    fclose(f);
                    // CRITICAL - free the buffer 
                    free(i2s_readraw_buffPtr);
                    // give the mutex back
                    xSemaphoreGive(spi_mutex);
                }
            }
        }

    }

}
