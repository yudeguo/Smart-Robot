#include "app_display.h"

#include <esp32s3_bsp_board.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <stdint.h>
#include <string.h>
#include <sys/stat.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define IMAGE_MAX_SIZE  (240*240*3)

static uint8_t *s_image_buffer;
static SemaphoreHandle_t emoji_mux;
static uint8_t s_emoji_type;
static const char *TAG = "app_display";

enum
{
    EMOJI_SUB_DIR_START,
    EMOJI_SUB_DIR_LOOP,
    EMOJI_SUB_DIR_END,
    EMOJI_SUB_DIR_MAX,
};

static void emoji_type_unlock(void)
{
    assert(emoji_mux && "app_display_start must be called first");
    xSemaphoreGiveRecursive(emoji_mux);
}

static bool emoji_type_lock(uint32_t timeout_ms)
{
    assert(emoji_mux && "app_display_start must be called first");
    const TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(emoji_mux, timeout_ticks) == pdTRUE;
}

uint8_t get_emoji_type(void)
{
    emoji_type_lock(0);
    uint8_t emoji_type = s_emoji_type;
    emoji_type_unlock();
    return emoji_type;
}

void set_emoji_type(uint8_t emoji_type)
{
    emoji_type_lock(0);
    s_emoji_type = emoji_type;
    emoji_type_unlock();
}

static void emoji_display(uint8_t emoji_type, uint8_t emoji_sub_dir)
{
    struct stat st;
    char emoji_play_path[32] ={0};
    uint8_t loop_times = 1;
    if (emoji_sub_dir == EMOJI_SUB_DIR_LOOP) {
        loop_times = 3;
    }
    for (int i = 0; i < loop_times; i++) {
        int emoji_frame_index = 1;
        while (true) {
            sniprintf(emoji_play_path,sizeof(emoji_play_path)-1,"/sdcard/emoji/%d/%d/frame%d.bin",emoji_type,
                emoji_sub_dir,emoji_frame_index);
            if (stat(emoji_play_path, &st) == 0) {
                FILE *f = fopen(emoji_play_path, "rb");
                if (NULL == f) {
                    ESP_LOGE(TAG, "Failed to open emoji_play_path");
                    break;
                }
                memset(s_image_buffer, 0, IMAGE_MAX_SIZE);
                fread(s_image_buffer, 1,IMAGE_MAX_SIZE, f);
                esp32s3_draw_image(s_image_buffer);
                fclose(f);
                emoji_frame_index++;
            }else {
                emoji_frame_index = 1;
                break;
            }
        }
    }
}

static void display_task(void *arg)
{
    while (true) {
        uint8_t emoji_type = get_emoji_type();
        for (int emoji_sub_dir_index = 0; emoji_sub_dir_index < EMOJI_SUB_DIR_MAX; emoji_sub_dir_index++) {
            emoji_display(emoji_type, emoji_sub_dir_index);
        }
    }
}

void app_display_start()
{
    emoji_mux = xSemaphoreCreateMutex();
    assert(emoji_mux);
    s_image_buffer = heap_caps_calloc(1, IMAGE_MAX_SIZE, MALLOC_CAP_SPIRAM);
    assert(s_image_buffer);
    BaseType_t ret = xTaskCreatePinnedToCore(display_task,"Display Task", 10 * 1024, NULL, 1, NULL, 1);
    ESP_ERROR_CHECK_WITHOUT_ABORT((pdPASS == ret) ? ESP_OK : ESP_FAIL);
}
