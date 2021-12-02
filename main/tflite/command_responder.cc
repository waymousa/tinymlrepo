/* Copyright 2019 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

//#include <string.h>

#include "command_responder.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

extern "C" {
  #include "ui.h"
    #include "respond.h"
}

#define TFLITE_YES 0x01
#define TFLITE_NO 0x02
#define TFLITE_UNKNOWN 0x03
#define TFLITE_OTHER 0xFF

extern QueueHandle_t xQueueMqttData;


// The default implementation writes out the name of the recognized command
// to the error console. Real applications will want to take some custom
// action instead, and should implement their own versions of this function.
void RespondToCommand(tflite::ErrorReporter* error_reporter,
                      int32_t current_time, const char* found_command,
                      uint8_t score, bool is_new_command) {  

  const char* yes = "yes";
  const char* no = "no";
  const char* unknown = "unknown";
  
  uint32_t mqttData;

  if (is_new_command) {
    TF_LITE_REPORT_ERROR(error_reporter, "Heard %s (%d) @%dms", found_command,
                         score, current_time);
  
    
                         
    if (found_command == yes){
      mqttData = TFLITE_YES;
      xQueueSend(xQueueMqttData,&mqttData,portMAX_DELAY);
      ui_textarea_add("Heard yes.\n", NULL, 0);
      respond("y");
    } else if (found_command == no){
      mqttData = TFLITE_NO;
      xQueueSend(xQueueMqttData,&mqttData,portMAX_DELAY);
      ui_textarea_add("Heard no.\n", NULL, 0);
      respond("n");
    } else if (found_command == unknown){
      mqttData = TFLITE_UNKNOWN;
      xQueueSend(xQueueMqttData,&mqttData,portMAX_DELAY);
      ui_textarea_add("Heard unknown.\n", NULL, 0);
      respond("u");
    } else {
      mqttData = TFLITE_OTHER;
      xQueueSend(xQueueMqttData,&mqttData,portMAX_DELAY);
      ui_textarea_add("Heard silence.\n", NULL, 0);
    }    
    
  }
}
