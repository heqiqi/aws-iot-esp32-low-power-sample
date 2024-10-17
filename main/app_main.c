
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "power_manage.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_event_legacy.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_log.h"
#include "esp_sleep.h"

#define BUTTON_GPIO 15
#define ESP_INTR_FLAG_DEFAULT 0


int aws_iot_demo_main( int argc, char ** argv );
void send_msg_to_queue(void *pvParameters);
void receive_msg_from_queue(void *pvParameters);

static const char *TAG = "AWS_IOT_POWER";

static xQueueHandle gpio_evt_queue = NULL;
static QueueHandle_t device_status_queue = NULL;

typedef struct {
    int sender_id;
    char device_msg[50];
} DeviceStatus;


static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void button_task(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            // 延迟一小段时间以消除按键抖动
            vTaskDelay(pdMS_TO_TICKS(50));
            // 读取按键当前状态
            int button_state = gpio_get_level(io_num);
            
            if(button_state == 0) {
                ESP_LOGI(TAG, "Button pressed");
                send_msg_to_queue((void*)0);
                //xTaskCreate(send_msg_to_queue, "Button pressed", 2048, (void*)0, 5, NULL);
            } else {
                ESP_LOGI(TAG, "Button released");
            }
        }
    }
}

void key_monitor(){
    // 创建一个队列来处理gpio事件
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    
    // 配置GPIO15
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_ANYEDGE;  // 上升沿和下降沿都触发中断
    io_conf.pin_bit_mask = (1ULL<<BUTTON_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    // 安装GPIO中断服务
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    
    // 添加中断处理程序
    gpio_isr_handler_add(BUTTON_GPIO, gpio_isr_handler, (void*) BUTTON_GPIO);

    // 创建按键处理任务
    xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);
}

void iot_task(void *pvParameters)
{
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        aws_iot_demo_main(0,NULL);
    }
}

void deep_sleep_task(void *pvParameters)
{
    for(int i=0;i<15;i++){
         vTaskDelay(pdMS_TO_TICKS(1*1000));
         UBaseType_t queueSize = uxQueueMessagesWaiting(device_status_queue);
         if(queueSize > 0){
            ESP_LOGI(TAG,"device_status_queue is not empty, queue size: %d, recount down",queueSize);
            i = 0;
         }else{
            ESP_LOGI(TAG,"enter deep sleep mode, after %d seconds",15-i);
         }
    }
   
    ESP_LOGI(TAG,"time to enter deep sleep mode");
    enter_deep_sleep_mode(DEEP_SLEEP_DURATION * uS_TO_S_FACTOR);
    
}

void send_msg_to_queue(void *pvParameters)
{
    int sender_id = (int)pvParameters;
    char msg[50];
    DeviceStatus device_status;
    device_status.sender_id = sender_id;
    uint64_t time_us = esp_timer_get_time();
    snprintf(msg, sizeof(msg), "{\"sender\":%d, \"msg\": \"btn message on ts:%lld\"}", sender_id, time_us);
    strcpy(device_status.device_msg, msg);
    if (xQueueSend(device_status_queue, &device_status, pdMS_TO_TICKS(1000)) != pdPASS) {
            ESP_LOGE(TAG, "Failed to send message to device_status_queue");
    } else {
            ESP_LOGI(TAG, "Sent message to device_status_queue: %s", device_status.device_msg);
    }
}

void receive_msg_from_queue(void *pvParameters)
{
    int task_id = (int)pvParameters;
    DeviceStatus received_device_status;
    if (xQueueReceive(device_status_queue, &received_device_status, pdMS_TO_TICKS(2000)) != pdPASS) {
            ESP_LOGW(TAG, "task_id: %d, device_status_queue size: %d, ready to deep sleep, lower power mode",task_id, uxQueueMessagesWaiting(device_status_queue));
            
    } else {
            ESP_LOGI(TAG, "task_id: %d, Received message from device_status_queue: %s ,queue size: %d",task_id, received_device_status.device_msg, uxQueueMessagesWaiting(device_status_queue) );
    }
}

void app_main()
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %"PRIu32" bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    esp_log_level_set("*", ESP_LOG_INFO);
    
    /* Initialize NVS partition */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        /* NVS partition was truncated
         * and needs to be erased */
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    device_status_queue = xQueueCreate(50, sizeof(DeviceStatus));

    key_monitor();
    //setup wifi
    wifi_init();
    xTaskCreate(&iot_task, "iot_core_mqtt_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "[APP] Getting wakeup_reason");
    get_wakeup_reason();
    int queue_size = uxQueueMessagesWaiting(device_status_queue);

    ESP_LOGI(TAG, "[APP] device status queue size: %d", queue_size);
    
    ESP_LOGI(TAG, "[APP] check the local msg queue, and enter_deep_sleep_mode");
    
    xTaskCreate(&deep_sleep_task, "deep_sleep_task", 2048, NULL, 5, NULL);
}
