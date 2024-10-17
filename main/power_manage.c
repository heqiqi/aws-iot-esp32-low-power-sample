#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "power_manage.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include "driver/gpio.h"
#include "protocol_examples_common.h"

#define BUTTON_GPIO_0 GPIO_NUM_0

static const char *TAG = "ESP32_POWER_MANAGE";

extern QueueHandle_t globalQueue;


void wifi_init() {
    ESP_ERROR_CHECK(example_connect());
}

void disable_rtc_wdt() {
    // init rtc watchdog
    esp_err_t err = esp_task_wdt_init(0, true);
    if (err != ESP_OK) {
        // err info
         ESP_LOGI(TAG, "[APP] esp_task_wdt_init err..");
    }
 
    // disable watchdog
    esp_task_wdt_delete(NULL);
}

void enter_deep_sleep_mode(int duration) {
    ESP_LOGI(TAG, "[APP] Going to deep sleep for %d seconds...", DEEP_SLEEP_DURATION);
   
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << BUTTON_GPIO_0),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf);
    esp_sleep_enable_ext0_wakeup(BUTTON_GPIO_0, 0);
     esp_deep_sleep(duration); 
    
}

int get_wakeup_reason(){

    esp_sleep_wakeup_cause_t wakeup_reason;
 
    wakeup_reason = esp_sleep_get_wakeup_cause();
 
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_EXT0:
            ESP_LOGW(TAG,"Wakeup caused by external signal using RTC_IO");
            break;
        case ESP_SLEEP_WAKEUP_EXT1:
            ESP_LOGW(TAG,"Wakeup caused by external signal using RTC_CNTL");
            break;
        case ESP_SLEEP_WAKEUP_TIMER:
          ESP_LOGW(TAG,"Wakeup caused by timer");
            break;
        case ESP_SLEEP_WAKEUP_TOUCHPAD:
            ESP_LOGI(TAG,"Wakeup caused by touchpad");
            break;
        case ESP_SLEEP_WAKEUP_ULP:
            ESP_LOGW(TAG,"Wakeup caused by ULP program");
            break;
        default:
            ESP_LOGI(TAG,"Wakeup was not caused by deep sleep: %d\n", wakeup_reason);
            break;
    }

    return wakeup_reason;
}
