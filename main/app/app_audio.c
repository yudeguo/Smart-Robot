#include "app_audio.h"

#include <app_chat.h>
#include <app_sr.h>
#include <app_tts.h>
#include <esp32s3_bsp_board.h>
#include <esp_err.h>
#include <esp_log.h>
#include <ff.h>
#include <sys/stat.h>
#include <sys/unistd.h>

#define AUDIO_WAV_HEADER_SIZE      44
#define DEFAULT_AUDIO_PLAY_DATA    320

#define SR_AUDIO_WAKE_PATH         "/sdcard/audio/echo_cn_wake.pcm"
#define SR_AUDIO_OVER_PATH         "/sdcard/audio/echo_cn_end.wav"
#define SR_TTS_AUDIO_PATH          "/sdcard/tts.pcm"

static const char *TAG = "app_audio";

static FILE * g_tts_audio = NULL;


void create_tts_audio()
{
    struct stat st;
    if (stat(SR_TTS_AUDIO_PATH, &st) == 0) {
        unlink(SR_TTS_AUDIO_PATH);
    }
    g_tts_audio = fopen(SR_TTS_AUDIO_PATH, "a+");
    if (NULL == g_tts_audio) {
        ESP_LOGE("open %s failed ",SR_TTS_AUDIO_PATH);
    }
}

void write_tts_audio(const char *data, int len)
{
    if (NULL != g_tts_audio) {
        fwrite(data, len, 1, g_tts_audio);
    }
}

void close_tts_audio()
{
    if (NULL != g_tts_audio) {
        fclose(g_tts_audio);
    }
}

esp_err_t app_audio_play(const char *path)
{
    struct stat st ={};
    size_t bytes_read = 0;
    size_t bytes_written = 0;;
    if (stat(path, &st) != 0) {
        ESP_LOGE(TAG,"Audio file %s not exit...",path);
        return ESP_FAIL;
    }
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        ESP_LOGE(TAG,"Open audio file %s error",path);
        return ESP_FAIL;
    }
    uint16_t *audio_data = (uint16_t *)malloc(DEFAULT_AUDIO_PLAY_DATA);
    assert(audio_data);
    while ((bytes_read = fread(audio_data, 1,DEFAULT_AUDIO_PLAY_DATA , fp)) > 0) {
        size_t frames_to_write = bytes_read / sizeof(uint16_t);
        esp32s3_bsp_i2s_write(audio_data,frames_to_write*sizeof(uint16_t), &bytes_written, portMAX_DELAY);
    }
    fclose(fp);
    free(audio_data);
    return ESP_OK;
}

void audio_play_task(void *arg)
{
    while (true) {
        int sr_state = get_sr_state();
        switch (sr_state) {
            case APP_SR_STATE_WAKED:
                ESP_LOGI(TAG,"Receive [APP_SR_STATE_WAKED] state");
                start_aliyun_ws_client();
                set_sr_state(APP_SR_AUDIO_SERVER_START);
                break;
            case APP_SR_AUDIO_SERVER_START:
                int audio_server_state = get_chat_send_status();
                ESP_LOGI(TAG,"Receive [APP_SR_AUDIO_SERVER_START [ %d ] ] state",audio_server_state);
                if (audio_server_state) {
                    app_audio_play(SR_AUDIO_WAKE_PATH);
                    set_sr_state(APP_SR_AUDIO_START_COLLECT);
                }
                break;
            case APP_SR_AUDIO_END_COLLECT:
                ESP_LOGI(TAG,"Receive [APP_SR_AUDIO_END_COLLECT] state");
                send_wss_task_finish_msg();//发送语音转文字结束消息
            break;
            case APP_SR_TTS_SERVER_START:
                ESP_LOGI(TAG,"Receive [APP_SR_TTS_SERVER_START] state");
                stop_aliyun_ws_client();// 关闭语音转文字websockt
                start_aliyun_tts_client(); //开启文字转语音websocket
                set_sr_state(APP_SR_START_FIELD_CHART);
                break;
            case APP_SR_START_FIELD_CHART:
                int tts_server_state = get_tts_state();
                ESP_LOGI(TAG,"Receive [APP_SR_START_FIELD_CHART [ %d ]] state",tts_server_state);
                if (tts_server_state) {
                    init_file_chat_start();
                    set_sr_state(APP_SR_STATE_CHARTING);
                }
                break;
            case APP_SR_END_FIELD_CHART:
                ESP_LOGI(TAG,"Receive [APP_SR_END_FIELD_CHART] state");
                //app_audio_play(SR_TTS_AUDIO_PATH);
                file_chat_stop(); //关闭通义千问
                stop_aliyun_tts_client();
                set_sr_state(APP_SR_STATE_ONCE_END);
                break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }

}
