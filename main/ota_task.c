// https://github.com/aws/amazon-freertos/blob/c409203d0031190c3544ad9275506c0ae2acef6f/demos/ota/ota_demo_core_mqtt/ota_demo_core_mqtt.c

#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "ota_task.h"
#include "wifi.h"
#include "ui.h"

#include "ota_os_freertos.h"

#include "ota.h"
#include "ota_pal.h"
#include "ota_config.h"
#include "ota_appversion32.h"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"

#include "core2forAWS.h"

extern AWS_IoT_Client mqtt_client;
extern EventGroupHandle_t mqtt_event_group;
extern SemaphoreHandle_t xMQTTSemaphore;

const AppVersion32_t appFirmwareVersion =
{
    .u.x.major = 1,
    .u.x.minor = 1,
    .u.x.build = 1
};

static const char *TAG = "OTA02";

extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");

#define democonfigCLIENT_IDENTIFIER    "0123ae5b4d28d83701"

/**
 * @brief The delay used in the main OTA Demo task loop to periodically output
 * the OTA statistics like number of packets received, dropped, processed and
 * queued per connection.
 */
#define otaexampleEXAMPLE_TASK_DELAY_MS                  ( 1000U )

/**
 * @brief The maximum size of the file paths used in the demo.
 */
#define otaexampleMAX_FILE_PATH_SIZE                     ( 260U )

/**
 * @brief The maximum size of the stream name required for downloading update
 * file from streaming service.
 */
#define otaexampleMAX_STREAM_NAME_SIZE                   ( 128U )

/**
 * @brief The common prefix for all OTA topics.
 *
 * Thing name is substituted with a wildcard symbol `+`. OTA agent
 * registers with MQTT broker with the thing name in the topic. This topic
 * filter is used to match incoming packet received and route them to OTA.
 * Thing name is not needed for this matching.
 */
#define otaexampleTOPIC_PREFIX                                 "$aws/things/+/"

/**
 * @brief Wildcard topic filter for job notification.
 * The filter is used to match the constructed job notify topic filter from OTA
 * agent and register appropirate callback for it.
 */
#define otaexampleJOB_NOTIFY_TOPIC_FILTER                      otaexampleTOPIC_PREFIX "jobs/notify-next"

/**
 * @brief Length of job notification topic filter.
 */
#define otaexampleJOB_NOTIFY_TOPIC_FILTER_LENGTH               ( ( uint16_t ) ( sizeof( otaexampleJOB_NOTIFY_TOPIC_FILTER ) - 1 ) )

/**
 * @brief Wildcard topic filter for matching job response messages.
 * This topic filter is used to match the responses from OTA service for OTA
 * agent job requests. The topic filter is a reserved topic which is not
 * subscribed with MQTT broker.
 */
#define otaexampleJOB_ACCEPTED_RESPONSE_TOPIC_FILTER           otaexampleTOPIC_PREFIX "jobs/$next/get/accepted"

/**
 * @brief Length of job accepted response topic filter.
 */
#define otaexampleJOB_ACCEPTED_RESPONSE_TOPIC_FILTER_LENGTH    ( ( uint16_t ) ( sizeof( otaexampleJOB_ACCEPTED_RESPONSE_TOPIC_FILTER ) - 1 ) )

/**
 * @brief Wildcard topic filter for matching OTA data packets.
 *  The filter is used to match the constructed data stream topic filter from
 * OTA agent and register appropirate callback for it.
 */
#define otaexampleDATA_STREAM_TOPIC_FILTER           otaexampleTOPIC_PREFIX  "streams/#"

/**
 * @brief Length of data stream topic filter.
 */
#define otaexampleDATA_STREAM_TOPIC_FILTER_LENGTH    ( ( uint16_t ) ( sizeof( otaexampleDATA_STREAM_TOPIC_FILTER ) - 1 ) )

/**
 * @brief Default topic filter for OTA.
 * This is used to route all the packets for OTA reserved topics which OTA agent
 * has not subscribed for.
 */
#define otaexampleDEFAULT_TOPIC_FILTER              otaexampleTOPIC_PREFIX "jobs/#"

/**
 * @brief Length of default topic filter.
 */
#define otaexampleDEFAULT_TOPIC_FILTER_LENGTH       ( ( uint16_t ) ( sizeof( otaexampleDEFAULT_TOPIC_FILTER ) - 1 ) )

/**
 * @brief The number of ticks to wait for the OTA Agent to complete the shutdown.
 */
#define otaexampleOTA_SHUTDOWN_WAIT_TICKS           ( 0U )

/**
 * @brief Unsubscribe from the job topics when shutdown is called.
 */
#define otaexampleUNSUBSCRIBE_AFTER_OTA_SHUTDOWN    ( 1U )

static uint8_t pucUpdateFilePath[ otaexampleMAX_FILE_PATH_SIZE ];

/**
 * @brief Certificate File path buffer.
 */
static uint8_t pucCertFilePath[ otaexampleMAX_FILE_PATH_SIZE ];

/**
 * @brief Stream name buffer.
 */
static uint8_t pucStreamName[ otaexampleMAX_STREAM_NAME_SIZE ];

/**
 * @brief Decode memory.
 */
static uint8_t pucDecodeMem[ otaconfigFILE_BLOCK_SIZE ];


/**
 * @brief Bitmap memory.
 */
static uint8_t pucBitmap[ OTA_MAX_BLOCK_BITMAP_SIZE ];


static SemaphoreHandle_t xBufferSemaphore;

/**
 * @brief Event buffer.
 */
static OtaEventData_t pxEventBuffer[ otaconfigMAX_NUM_OTA_DATA_BUFFERS ];

static bool matchEndWildcardsSpecialCases( const char * pTopicFilter,
                                           uint16_t topicFilterLength,
                                           uint16_t filterIndex )
{
    bool matchFound = false;

    assert( pTopicFilter != NULL );
    assert( topicFilterLength != 0 );

    /* Check if the topic filter has 2 remaining characters and it ends in
     * "/#". This check handles the case to match filter "sport/#" with topic
     * "sport". The reason is that the '#' wildcard represents the parent and
     * any number of child levels in the topic name.*/
    if( ( topicFilterLength >= 3U ) &&
        ( filterIndex == ( topicFilterLength - 3U ) ) &&
        ( pTopicFilter[ filterIndex + 1U ] == '/' ) &&
        ( pTopicFilter[ filterIndex + 2U ] == '#' ) )

    {
        matchFound = true;
    }

    /* Check if the next character is "#" or "+" and the topic filter ends in
     * "/#" or "/+". This check handles the cases to match:
     *
     * - Topic filter "sport/+" with topic "sport/".
     * - Topic filter "sport/#" with topic "sport/".
     */
    if( ( filterIndex == ( topicFilterLength - 2U ) ) &&
        ( pTopicFilter[ filterIndex ] == '/' ) )
    {
        /* Check that the last character is a wildcard. */
        matchFound = ( ( pTopicFilter[ filterIndex + 1U ] == '+' ) ||
                       ( pTopicFilter[ filterIndex + 1U ] == '#' ) ) ? true : false;
    }

    return matchFound;
}

static bool matchWildcards( const char * pTopicName,
                            uint16_t topicNameLength,
                            const char * pTopicFilter,
                            uint16_t topicFilterLength,
                            uint16_t * pNameIndex,
                            uint16_t * pFilterIndex,
                            bool * pMatch )
{
    bool shouldStopMatching = false;
    bool locationIsValidForWildcard;

    assert( pTopicName != NULL );
    assert( topicNameLength != 0 );
    assert( pTopicFilter != NULL );
    assert( topicFilterLength != 0 );
    assert( pNameIndex != NULL );
    assert( pFilterIndex != NULL );
    assert( pMatch != NULL );

    /* Wild card in a topic filter is only valid either at the starting position
     * or when it is preceded by a '/'.*/
    locationIsValidForWildcard = ( ( *pFilterIndex == 0u ) ||
                                   ( pTopicFilter[ *pFilterIndex - 1U ] == '/' )
                                   ) ? true : false;

    if( ( pTopicFilter[ *pFilterIndex ] == '+' ) && ( locationIsValidForWildcard == true ) )
    {
        bool nextLevelExistsInTopicName = false;
        bool nextLevelExistsinTopicFilter = false;

        /* Move topic name index to the end of the current level. The end of the
         * current level is identified by the last character before the next level
         * separator '/'. */
        while( *pNameIndex < topicNameLength )
        {
            /* Exit the loop if we hit the level separator. */
            if( pTopicName[ *pNameIndex ] == '/' )
            {
                nextLevelExistsInTopicName = true;
                break;
            }

            ( *pNameIndex )++;
        }

        /* Determine if the topic filter contains a child level after the current level
         * represented by the '+' wildcard. */
        if( ( *pFilterIndex < ( topicFilterLength - 1U ) ) &&
            ( pTopicFilter[ *pFilterIndex + 1U ] == '/' ) )
        {
            nextLevelExistsinTopicFilter = true;
        }

        /* If the topic name contains a child level but the topic filter ends at
         * the current level, then there does not exist a match. */
        if( ( nextLevelExistsInTopicName == true ) &&
            ( nextLevelExistsinTopicFilter == false ) )
        {
            *pMatch = false;
            shouldStopMatching = true;
        }

        /* If the topic name and topic filter have child levels, then advance the
         * filter index to the level separator in the topic filter, so that match
         * can be performed in the next level.
         * Note: The name index already points to the level separator in the topic
         * name. */
        else if( nextLevelExistsInTopicName == true )
        {
            ( *pFilterIndex )++;
        }
        else
        {
            /* If we have reached here, the the loop terminated on the
             * ( *pNameIndex < topicNameLength) condition, which means that have
             * reached past the end of the topic name, and thus, we decrement the
             * index to the last character in the topic name.*/
            ( *pNameIndex )--;
        }
    }

    /* '#' matches everything remaining in the topic name. It must be the
     * last character in a topic filter. */
    else if( ( pTopicFilter[ *pFilterIndex ] == '#' ) &&
             ( *pFilterIndex == ( topicFilterLength - 1U ) ) &&
             ( locationIsValidForWildcard == true ) )
    {
        /* Subsequent characters don't need to be checked for the
         * multi-level wildcard. */
        *pMatch = true;
        shouldStopMatching = true;
    }
    else
    {
        /* Any character mismatch other than '+' or '#' means the topic
         * name does not match the topic filter. */
        *pMatch = false;
        shouldStopMatching = true;
    }

    return shouldStopMatching;
}


static bool matchTopicFilter( const char * pTopicName,
                              uint16_t topicNameLength,
                              const char * pTopicFilter,
                              uint16_t topicFilterLength )
{
    bool matchFound = false, shouldStopMatching = false;
    uint16_t nameIndex = 0, filterIndex = 0;

    assert( pTopicName != NULL );
    assert( topicNameLength != 0 );
    assert( pTopicFilter != NULL );
    assert( topicFilterLength != 0 );

    while( ( nameIndex < topicNameLength ) && ( filterIndex < topicFilterLength ) )
    {
        /* Check if the character in the topic name matches the corresponding
         * character in the topic filter string. */
        if( pTopicName[ nameIndex ] == pTopicFilter[ filterIndex ] )
        {
            /* If the topic name has been consumed but the topic filter has not
             * been consumed, match for special cases when the topic filter ends
             * with wildcard character. */
            if( nameIndex == ( topicNameLength - 1U ) )
            {
                matchFound = matchEndWildcardsSpecialCases( pTopicFilter,
                                                            topicFilterLength,
                                                            filterIndex );
            }
        }
        else
        {
            /* Check for matching wildcards. */
            shouldStopMatching = matchWildcards( pTopicName,
                                                 topicNameLength,
                                                 pTopicFilter,
                                                 topicFilterLength,
                                                 &nameIndex,
                                                 &filterIndex,
                                                 &matchFound );
        }

        if( ( matchFound == true ) || ( shouldStopMatching == true ) )
        {
            break;
        }

        /* Increment indexes. */
        nameIndex++;
        filterIndex++;
    }

    if( matchFound == false )
    {
        /* If the end of both strings has been reached, they match. This represents the
         * case when the topic filter contains the '+' wildcard at a non-starting position.
         * For example, when matching either of "sport/+/player" OR "sport/hockey/+" topic
         * filters with "sport/hockey/player" topic name. */
        matchFound = ( ( nameIndex == topicNameLength ) &&
                       ( filterIndex == topicFilterLength ) ) ? true : false;
    }

    return matchFound;
}

static void prvOTAAgentTask( void * pParam )
{
    /* Calling OTA agent task. */
    ESP_LOGI(TAG, "Hello from prvOTAAgentTask 1");
    OTA_EventProcessingTask( pParam );
    LogInfo( ( "OTA Agent stopped." ) );

    vTaskDelete( NULL );
}

static void prvOtaEventBufferFree( OtaEventData_t * const pxBuffer )
{
    if( xSemaphoreTake( xBufferSemaphore, portMAX_DELAY ) == pdTRUE )
    {
        pxBuffer->bufferUsed = false;
        ( void ) xSemaphoreGive( xBufferSemaphore );
    }
    else
    {
        LogError( ( "Failed to get buffer semaphore." ) );
    }
}

static OtaEventData_t * prvOtaEventBufferGet( void )
{
    uint32_t ulIndex = 0;
    OtaEventData_t * pxFreeBuffer = NULL;

    if( xSemaphoreTake( xBufferSemaphore, portMAX_DELAY ) == pdTRUE )
    {
        for( ulIndex = 0; ulIndex < otaconfigMAX_NUM_OTA_DATA_BUFFERS; ulIndex++ )
        {
            if( pxEventBuffer[ ulIndex ].bufferUsed == false )
            {
                pxEventBuffer[ ulIndex ].bufferUsed = true;
                pxFreeBuffer = &pxEventBuffer[ ulIndex ];
                break;
            }
        }

        ( void ) xSemaphoreGive( xBufferSemaphore );
    }
    else
    {
        LogError( ( "Failed to get buffer semaphore." ) );
    }
    return pxFreeBuffer;
}

void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
                                    IoT_Publish_Message_Params *params, void *pData) 
{
    
    ESP_LOGI(TAG, "Subscribe callback");
    ESP_LOGI(TAG, "%.*s\t%.*s", topicNameLen, topicName, (int) params->payloadLen, (char *)params->payload);

    bool xIsMatch = false;

    xIsMatch = matchTopicFilter( topicName,
                              topicNameLen,
                              otaexampleJOB_NOTIFY_TOPIC_FILTER,
                              otaexampleJOB_NOTIFY_TOPIC_FILTER_LENGTH);

    if( xIsMatch == true )
    {    
        OtaEventData_t * pxEventData;
        OtaEventMsg_t pxEventMsg = { 0 };
        pxEventData = prvOtaEventBufferGet();
        if( pxEventData != NULL ) {
            memcpy( pxEventData->data, params->payload, params->payloadLen );
            pxEventData->dataLength = params->payloadLen;
            pxEventMsg.eventId = OtaAgentEventReceivedJobDocument;
            pxEventMsg.pEventData = pxEventData;
            OTA_SignalEvent( &pxEventMsg );
            return;
        } else {
            LogError( ( "Error: No OTA data buffers available.\r\n" ) );
        }

    }


    xIsMatch = matchTopicFilter(topicName,
                            topicNameLen,
                            otaexampleJOB_ACCEPTED_RESPONSE_TOPIC_FILTER,
                            otaexampleJOB_ACCEPTED_RESPONSE_TOPIC_FILTER_LENGTH);
    if( xIsMatch == true ) 
    {
        OtaEventData_t * pxEventData;
        OtaEventMsg_t pxEventMsg = { 0 };
        pxEventData = prvOtaEventBufferGet();
        if( pxEventData != NULL ) {
            memcpy( pxEventData->data, params->payload, params->payloadLen );
            pxEventData->dataLength = params->payloadLen;
            pxEventMsg.eventId = OtaAgentEventReceivedJobDocument;
            pxEventMsg.pEventData = pxEventData;
            OTA_SignalEvent( &pxEventMsg );
            return;
        } else {
            LogError( ( "Error: No OTA data buffers available.\r\n" ) );
        }

    }

    xIsMatch = matchTopicFilter(topicName,
                        topicNameLen,
                        otaexampleDATA_STREAM_TOPIC_FILTER,
                        otaexampleDATA_STREAM_TOPIC_FILTER_LENGTH);
    
    if( xIsMatch == true ) 
    {
        OtaEventData_t * pxEventData;
        OtaEventMsg_t pxEventMsg = { 0 };
        pxEventData = prvOtaEventBufferGet();
        if( pxEventData != NULL ) {
            memcpy( pxEventData->data, params->payload, params->payloadLen );
            pxEventData->dataLength = params->payloadLen;
            pxEventMsg.eventId = OtaAgentEventReceivedFileBlock;
            pxEventMsg.pEventData = pxEventData;
            OTA_SignalEvent( &pxEventMsg );
            return;
        } else {
            LogError( ( "Error: No OTA data buffers available.\r\n" ) );
        }

    }

}

static OtaMqttStatus_t prvMqttSubscribe( const char * pcTopicFilter,
                                         uint16_t usTopicFilterLength,
                                         uint8_t ucQOS )
{
    
    OtaMqttStatus_t xOtaMqttStatus = OtaMqttSubscribeFailed;
   
    if(xSemaphoreTake(xMQTTSemaphore,pdMS_TO_TICKS(100)) == pdTRUE ) {

        IoT_Error_t rc = aws_iot_mqtt_subscribe(&mqtt_client, pcTopicFilter, strlen(pcTopicFilter), ucQOS, iot_subscribe_callback_handler, NULL);
        if(SUCCESS != rc) {
            ESP_LOGE(TAG, "Error subscribing : %d ", rc);
        } else{
            ESP_LOGI(TAG, "Subscribed to topic '%s'", pcTopicFilter);
            xOtaMqttStatus = OtaMqttSuccess;
        }
        xSemaphoreGive(xMQTTSemaphore);

    } else {
         ESP_LOGE(TAG, "Could not take semaphore");
    }

    return xOtaMqttStatus;
}

static OtaMqttStatus_t prvMqttPublish( const char * const pcTopic,
                                       uint16_t usTopicLen,
                                       const char * pcMsg,
                                       uint32_t ulMsgSize,
                                       uint8_t ucQOS )
{

    OtaMqttStatus_t xOtaMqttStatus = OtaMqttPublishFailed;
    
    IoT_Publish_Message_Params paramsQOS0;
    paramsQOS0.qos = QOS0;
    paramsQOS0.payload = pcMsg;
    paramsQOS0.payloadLen = ulMsgSize;
    paramsQOS0.isRetained = 0;

    if(xSemaphoreTake(xMQTTSemaphore,pdMS_TO_TICKS(100)) == pdTRUE ) {
        IoT_Error_t rc = aws_iot_mqtt_publish(&mqtt_client, pcTopic, usTopicLen, &paramsQOS0);
        if (rc != SUCCESS){
            ESP_LOGE(TAG, "Publish QOS0 error %i", rc);
        } else {
            ESP_LOGI(TAG, "Published to topic: '%s', message: '%s'", pcTopic, pcMsg);
            xOtaMqttStatus = OtaMqttSuccess;
        }
        xSemaphoreGive(xMQTTSemaphore);
    } else {
        ESP_LOGE(TAG, "Could not take semaphore");
    }

    return xOtaMqttStatus;
}


static OtaMqttStatus_t prvMqttUnSubscribe( const char * pcTopicFilter,
                                           uint16_t usTopicFilterLength,
                                           uint8_t ucQOS )
{
    
    ESP_LOGI(TAG, "Hello from prvMqttUnSubscribe");
    vTaskDelay(pdMS_TO_TICKS(1000));
    OtaMqttStatus_t xOtaMqttStatus = OtaMqttSuccess;
    return xOtaMqttStatus;
}



static void prvSetOtaInterfaces( OtaInterfaces_t * pxOtaInterfaces )
{
    /* Initialize OTA library OS Interface. */
    pxOtaInterfaces->os.event.init = OtaInitEvent_FreeRTOS;
    pxOtaInterfaces->os.event.send = OtaSendEvent_FreeRTOS;
    pxOtaInterfaces->os.event.recv = OtaReceiveEvent_FreeRTOS;
    pxOtaInterfaces->os.event.deinit = OtaDeinitEvent_FreeRTOS;
    pxOtaInterfaces->os.timer.start = OtaStartTimer_FreeRTOS;
    pxOtaInterfaces->os.timer.stop = OtaStopTimer_FreeRTOS;
    pxOtaInterfaces->os.timer.delete = OtaDeleteTimer_FreeRTOS;
    pxOtaInterfaces->os.mem.malloc = Malloc_FreeRTOS;
    pxOtaInterfaces->os.mem.free = Free_FreeRTOS;
    /* Initialize the OTA library MQTT Interface.*/
    pxOtaInterfaces->mqtt.subscribe = prvMqttSubscribe;
    pxOtaInterfaces->mqtt.publish = prvMqttPublish;
    pxOtaInterfaces->mqtt.unsubscribe = prvMqttUnSubscribe;
    /* Initialize the OTA library PAL Interface.*/
    pxOtaInterfaces->pal.getPlatformImageState = otaPal_GetPlatformImageState;
    pxOtaInterfaces->pal.setPlatformImageState = otaPal_SetPlatformImageState;
    pxOtaInterfaces->pal.writeBlock = otaPal_WriteBlock;
    pxOtaInterfaces->pal.activate = otaPal_ActivateNewImage;
    pxOtaInterfaces->pal.closeFile = otaPal_CloseFile;
    pxOtaInterfaces->pal.reset = otaPal_ResetDevice;
    pxOtaInterfaces->pal.abort = otaPal_Abort;
    pxOtaInterfaces->pal.createFile = otaPal_CreateFileForRx;
}

static void prvOtaAppCallback( OtaJobEvent_t xEvent, const void * pData )
{
    OtaErr_t xOtaError = OtaErrUninitialized;

    switch( xEvent )
    {
        case OtaJobEventActivate:
            LogInfo( ( "Received OtaJobEventActivate callback from OTA Agent." ) );

            /* Activate the new firmware image. */
            OTA_ActivateNewImage();

            /* Initiate Shutdown of OTA Agent.
             * If it is required that the unsubscribe operations are not
             * performed while shutting down please set the second parameter to
             * 0 instead of 1.
             */
            OTA_Shutdown( otaexampleOTA_SHUTDOWN_WAIT_TICKS, otaexampleUNSUBSCRIBE_AFTER_OTA_SHUTDOWN );

            /* Requires manual activation of new image.*/
            LogError( ( "New image activation failed." ) );

            break;

        case OtaJobEventFail:
            LogInfo( ( "Received OtaJobEventFail callback from OTA Agent." ) );

            /* Nothing special to do. The OTA agent handles it. */
            break;

        case OtaJobEventStartTest:

            /* This demo just accepts the image since it was a good OTA update and networking
             * and services are all working (or we would not have made it this far). If this
             * were some custom device that wants to test other things before validating new
             * image, this would be the place to kick off those tests before calling
             * OTA_SetImageState() with the final result of either accepted or rejected. */

            LogInfo( ( "Received OtaJobEventStartTest callback from OTA Agent." ) );
            xOtaError = OTA_SetImageState( OtaImageStateAccepted );

            if( xOtaError != OtaErrNone )
            {
                LogError( ( " Failed to set image state as accepted." ) );
            }
            else
            {
                LogInfo( ( "Successfully updated with the new image." ) );
            }

            break;

        case OtaJobEventProcessed:
            LogDebug( ( "Received OtaJobEventProcessed callback from OTA Agent." ) );

            if( pData != NULL )
            {
                prvOtaEventBufferFree( ( OtaEventData_t * ) pData );
            }

            break;

        case OtaJobEventSelfTestFailed:
            LogDebug( ( "Received OtaJobEventSelfTestFailed callback from OTA Agent." ) );

            /* Requires manual activation of previous image as self-test for
             * new image downloaded failed.*/
            LogError( ( "Self-test of new image failed, shutting down OTA Agent." ) );

            /* Initiate Shutdown of OTA Agent.
             * If it is required that the unsubscribe operations are not
             * performed while shutting down please set the second parameter to 0 instead of 1
             */
            OTA_Shutdown( otaexampleOTA_SHUTDOWN_WAIT_TICKS, otaexampleUNSUBSCRIBE_AFTER_OTA_SHUTDOWN );

            break;

        default:
            LogDebug( ( "Received invalid callback event from OTA Agent." ) );
    }
}

void ota_task(void *args) {

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);
    xEventGroupWaitBits(mqtt_event_group, CONNECTED_BIT, false, true, portMAX_DELAY);

    /* Initialize semaphore for buffer operations. */
    xBufferSemaphore = xSemaphoreCreateMutex();
    
    /* Status indicating a successful demo or not. */
    BaseType_t xStatus = pdFAIL;

    /* OTA library return status. */
    OtaErr_t xOtaError = OtaErrUninitialized;

    /* OTA event message used for sending event to OTA Agent.*/
    OtaEventMsg_t xEventMsg = { 0 };

    char * pClientIdentifier = "uniqueClientID";
    
    /* OTA interface context required for library interface functions.*/
    OtaInterfaces_t xOtaInterfaces;

    /* OTA library packet statistics per job.*/
    OtaAgentStatistics_t xOtaStatistics = { 0 };

    /* OTA Agent state returned from calling OTA_GetState.*/
    OtaState_t xOtaState = OtaAgentStateStopped;
    
    prvSetOtaInterfaces( &xOtaInterfaces );

    static OtaAppBuffer_t xOtaBuffer =
    {
        .pUpdateFilePath    = pucUpdateFilePath,
        .updateFilePathsize = otaexampleMAX_FILE_PATH_SIZE,
        .pCertFilePath      = pucCertFilePath,
        .certFilePathSize   = otaexampleMAX_FILE_PATH_SIZE,
        .pStreamName        = pucStreamName,
        .streamNameSize     = otaexampleMAX_STREAM_NAME_SIZE,
        .pDecodeMemory      = pucDecodeMem,
        .decodeMemorySize   = otaconfigFILE_BLOCK_SIZE,
        .pFileBitmap        = pucBitmap,
        .fileBitmapSize     = OTA_MAX_BLOCK_BITMAP_SIZE,
    };

     /*************************** Init OTA Library. ***************************/

    if( ( xOtaError = OTA_Init( &xOtaBuffer,
                                &xOtaInterfaces,
                                ( const uint8_t * ) ( democonfigCLIENT_IDENTIFIER ),
                                prvOtaAppCallback ) ) != OtaErrNone )
    {
        LogError( ( "Failed to initialize OTA Agent, exiting = %u.", xOtaError ) );
    }
    else
    {
        xStatus = pdPASS;
    }

    /************************ Create OTA Agent Task. ************************/

    if( xStatus == pdPASS )
    {   
        xStatus = xTaskCreatePinnedToCore(&prvOTAAgentTask, "aws_iot_agent_task", 4096 * 5, NULL, 1, NULL, 1);

        if( xStatus != pdPASS )
        {
            LogError( ( "Failed to create OTA agent task:" ) );
        }
    }
    
    
    /****************************** Start OTA ******************************/

    if( xStatus == pdPASS )
    {
        /* Send start event to OTA Agent.*/
        xEventMsg.eventId = OtaAgentEventStart;
        OTA_SignalEvent( &xEventMsg );
    }

    /******************** Loop and display OTA statistics ********************/

    if( xStatus == pdPASS )
    {
        while( ( xOtaState = OTA_GetState() ) != OtaAgentStateStopped )
        {
            /* Get OTA statistics for currently executing job. */
            if( xOtaState != OtaAgentStateSuspended )
            {
                OTA_GetStatistics( &xOtaStatistics );

                LogInfo( ( " Received: %u   Queued: %u   Processed: %u   Dropped: %u",
                           xOtaStatistics.otaPacketsReceived,
                           xOtaStatistics.otaPacketsQueued,
                           xOtaStatistics.otaPacketsProcessed,
                           xOtaStatistics.otaPacketsDropped ) );
            }

            ESP_LOGI(TAG, "Hello from OTA Task");
            vTaskDelay( pdMS_TO_TICKS( otaexampleEXAMPLE_TASK_DELAY_MS ) );
            
        }
    }

}

