#include <time.h>
#include <string.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "core2forAWS.h"
#include "wifi.h"

#include "atecc608.h"
#include "i2c_device.h"
#include "iotex_emb.h"
#include "abi_pack.h"
#include "signer.h"
#include "pb_proto.h"

static const char *TAG = "IOTEX";

#define IS_SETUP 0 
#define PRINT_KEYS 1

#define IOTEX_VERSION 1
#define IOTEX_GAS_LIMIT 1000000
#define IOTEX_CONTRACT "io1jjwlujpk7wztptwdjvun268ccsadsd7dtl2alq"
#define IOTEX_EMB_MAX_ACB_LEN 1024

void iotex_task(void *args) 
{
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    
    ATCA_STATUS atca_status_ret;
    uint8_t signature[64];
    uint8_t public_key[64];
    // message to sign  - 'hello from AWS EduKit' -> 0x68656c6c6f2066726f6d20415753204564754b6974
    uint8_t msg[32] = { 
        0x7d, 0x38, 0xde, 0x03, 0xdd, 0x4e, 0x59, 0xdf, 
        0x9f, 0xf5, 0x05, 0x95, 0x93, 0x9f, 0xaa, 0x2d, 
        0xf5, 0x3b, 0xc7, 0x36, 0x6d, 0xdb, 0x57, 0x06, 
        0xe1, 0xbd, 0x49, 0xbf, 0xe6, 0x75, 0xd5, 0x31 
    };

    // Sign the message using private key in slot No. 2

    i2c_take_port(ATECC608_I2C_PORT, portMAX_DELAY);
    atca_status_ret =  atcab_sign(2, msg, signature);
    i2c_free_port(ATECC608_I2C_PORT);
    printf("atca_status_ret: %i \r\n", atca_status_ret);
    printf("signature 2: \r\n");
    for (int i = 0; i < 64; i++) {
        printf("%02X", signature[i]);
    } 
    printf("\n");

    // Calculate the public key based on private key in slot No. 2

    i2c_take_port(ATECC608_I2C_PORT, portMAX_DELAY);
    atca_status_ret = atcab_get_pubkey(2, public_key);
    i2c_free_port(ATECC608_I2C_PORT);
    printf("atca_status_ret: %i \r\n", atca_status_ret);
    printf("atcab_read_pubkey 2: \r\n");
    for (int i = 0; i < 64; i++) {
        printf("%02X", public_key[i]);
    } 
    printf("\n");
 

    while(1){
        
        vTaskDelay(pdMS_TO_TICKS(1000));
        printf("Hello form IOTEX\r\n");

    }

}
