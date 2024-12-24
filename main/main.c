#include <app_chat.h>
#include <app_display.h>
#include <app_network.h>
#include <app_sr.h>
#include <app_tts.h>

#include "esp32s3_bsp_board.h"
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static char *TAG = "app_main";

void app_main(void)
{
    bsp_board_init();
    esp32s3_bsp_sdcard_mount();
    app_network_start();
    app_sr_start();
    app_chat_start();
    app_tts_start();
    app_display_start();
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5 * 1000));
        ESP_LOGD(TAG, "Current Free Memory\t%d\t\t%d",
         heap_caps_get_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
         heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
        ESP_LOGD(TAG, "Min. Ever Free Size\t%d\t\t%d",
                 heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL),
                 heap_caps_get_minimum_free_size(MALLOC_CAP_SPIRAM));
    }

}