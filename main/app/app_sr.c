#include "app_sr.h"

#include <app_audio.h>
#include <app_chat.h>
#include <app_network.h>
#include <data_handle.h>
#include <esp32s3_bsp_board.h>
#include <esp_check.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <func_testing.h>
#include <model_path.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_afe_sr_models.h"
#include "esp_log.h"

#define I2S_CHANNEL_NUM               1
#define MAX_AUDIO_IDLE_FRAMES         100
#define MAX_AUDIO_DATA_QUEUE_SIZE     20

static const char *TAG = "app_sr";
static sr_data_t *g_sr_data = NULL;
static esp_afe_sr_iface_t *afe_handle = NULL;
static sr_handle_t  g_sr_handle;

static void sr_state_unlock(void)
{
    assert(g_sr_handle.state_mux && "app_sr_start must be called first");
    xSemaphoreGiveRecursive(g_sr_handle.state_mux);
}

static bool sr_state_lock(uint32_t timeout_ms)
{
    assert(g_sr_handle.state_mux && "bsp_display_start must be called first");
    const TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(g_sr_handle.state_mux, timeout_ticks) == pdTRUE;
}

int get_sr_state(void)
{
    sr_state_lock(0);
    const uint8_t state = g_sr_handle.state;
    sr_state_unlock();
    return state;
}

void set_sr_state(int new_state)
{
    sr_state_lock(0);
    g_sr_handle.state = new_state;
    sr_state_unlock();
}

static void audio_feed_task(void *arg)
{
    ESP_LOGI(TAG, "Starting audio feed");
    size_t bytes_read = 0;
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *) arg;
    int feed_channel = I2S_CHANNEL_NUM;
    int audio_chunksize = afe_handle->get_feed_chunksize(afe_data);
    int16_t *audio_buffer = heap_caps_malloc(audio_chunksize * sizeof(int16_t) * feed_channel, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    assert(audio_buffer);
    g_sr_data->afe_in_buffer = audio_buffer;
    while (true) {
        int state = get_sr_state();
        esp32s3_bsp_i2s_read((char *)audio_buffer, audio_chunksize * I2S_CHANNEL_NUM * sizeof(int16_t), &bytes_read, portMAX_DELAY);
        if (get_wifi_connected_status()) {
            afe_handle->feed(afe_data, audio_buffer);
            int send_state = get_chat_send_status();
            if (APP_SR_AUDIO_START_COLLECT == state && send_state) {
                //audio_data_t *data = (audio_data_t *)malloc(sizeof(audio_data_t));
                audio_data_t *data = heap_caps_calloc(1, sizeof(audio_data_t), MALLOC_CAP_SPIRAM);
                if (NULL != data) {
                    memcpy(data->data, audio_buffer, bytes_read);
                    data->data_len = (int)bytes_read;
                    push_audio_data_queue(data);
                }
            }
        }
    }
}

static void audio_detect_task(void *arg)
{
    ESP_LOGI(TAG, "Starting audio detection");
    static afe_vad_state_t local_state;
    static uint8_t frame_keep = 0;
    esp_afe_sr_data_t *afe_data = (esp_afe_sr_data_t *) arg;
    while (true) {
        int sr_state = get_sr_state();
        afe_fetch_result_t *res = afe_handle->fetch(afe_data);
        if (!res || res->ret_value == ESP_FAIL) {
            ESP_LOGW(TAG, "AFE Fetch Fail");
            continue;
        }
        if (res->wakeup_state == WAKENET_CHANNEL_VERIFIED) {
            ESP_LOGI(TAG, "=======AFE Fetch  WAKENET_CHANNEL_VERIFIED==========");
            sr_state = APP_SR_STATE_WAKED;
            set_sr_state(APP_SR_STATE_WAKED);
            g_sr_data->afe_handle->disable_wakenet(afe_data);
        }
        if (APP_SR_AUDIO_START_COLLECT  == sr_state ) {
            if (local_state != res->vad_state) {
                local_state = res->vad_state;
                frame_keep = 0;
            } else {
                frame_keep++;
            }
            if ((MAX_AUDIO_IDLE_FRAMES == frame_keep) && (AFE_VAD_SILENCE == res->vad_state)) {
                set_sr_state(APP_SR_AUDIO_END_COLLECT);
            }
        }
        if (APP_SR_STATE_ONCE_END == sr_state) {
            set_sr_state(APP_SR_STATE_IDLE);
            g_sr_data->afe_handle->enable_wakenet(afe_data);
        }
    }
}

esp_err_t app_sr_start(void)
{
    esp_err_t ret = ESP_OK;
    init_audio_data_queue(MAX_AUDIO_DATA_QUEUE_SIZE);
    g_sr_handle.state_mux = xSemaphoreCreateMutex();
    assert(g_sr_handle.state_mux);
    g_sr_data = heap_caps_calloc(1, sizeof(sr_data_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_RETURN_ON_FALSE(NULL != g_sr_data, ESP_ERR_NO_MEM, TAG, "Failed create sr data");
    srmodel_list_t *models = esp_srmodel_init("/sdcard");
    afe_handle = (esp_afe_sr_iface_t *)&ESP_AFE_SR_HANDLE;
    afe_config_t afe_config = AFE_CONFIG_DEFAULT();
    afe_handle = (esp_afe_sr_iface_t *)&ESP_AFE_SR_HANDLE;
    afe_config.memory_alloc_mode = AFE_MEMORY_ALLOC_MORE_PSRAM;
    afe_config.wakenet_init = true;
    afe_config.wakenet_model_name = esp_srmodel_filter(models, ESP_WN_PREFIX, NULL);;
    afe_config.voice_communication_init = false;
    afe_config.pcm_config.total_ch_num = 1;
    afe_config.pcm_config.mic_num = 1;
    afe_config.pcm_config.ref_num = 0;
    afe_config.wakenet_mode = DET_MODE_90;
    afe_config.se_init = false;
    afe_config.aec_init = false;
    esp_afe_sr_data_t *afe_data = afe_handle->create_from_config(&afe_config);
    g_sr_data->afe_handle = afe_handle;
    g_sr_data->afe_data = afe_data;
    BaseType_t ret_val = xTaskCreatePinnedToCore(&audio_feed_task, "Feed Task", 8 * 1024, (void *)afe_data, 5,&g_sr_data->feed_task, 0);
    ESP_GOTO_ON_FALSE(pdPASS == ret_val, ESP_FAIL, err, TAG,  "Failed create audio feed task");
    ret_val = xTaskCreatePinnedToCore(&audio_detect_task, "Detect Task", 8 * 1024, (void *)afe_data, 5, &g_sr_data->detect_task, 1);
    ESP_GOTO_ON_FALSE(pdPASS == ret_val, ESP_FAIL, err, TAG,  "Failed create audio detect task");
    ret_val = xTaskCreatePinnedToCore(&audio_play_task, "Detect Task", 4 * 1024, (void *)afe_data, 5, &g_sr_data->audio_handle_task, 1);
    ESP_GOTO_ON_FALSE(pdPASS == ret_val, ESP_FAIL, err, TAG,  "Failed create audio play task");
    return ESP_OK;
err:
    //app_sr_stop();
    return ret;
}





