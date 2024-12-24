#ifndef APP_SR_H
#define APP_SR_H
#include "freertos/FreeRTOS.h"
#include <esp_mn_iface.h>
#include <esp_wn_iface.h>
#include <stdbool.h>
#include <stdio.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "esp_afe_sr_models.h"
#include "esp_mn_models.h"

#define MAX_AUDIO_DATA_LEN       512

typedef enum
{
    APP_SR_STATE_IDLE,
    APP_SR_STATE_WAKED,// 语音唤醒成功
    APP_SR_AUDIO_SERVER_START, //启动初始化语音转文字wss
    APP_SR_AUDIO_START_COLLECT, //开始采集音频启动转换
    APP_SR_AUDIO_END_COLLECT,  //结束采集音频
    APP_SR_TTS_SERVER_START, //启动文字转语音server
    APP_SR_START_FIELD_CHART, //开始通义千问
    APP_SR_STATE_CHARTING,//开始处理语音聊天内容
    APP_SR_END_FIELD_CHART,
    APP_SR_STATE_ONCE_END,
}NUM_SR_STATE;

typedef struct {
    uint8_t state;

    SemaphoreHandle_t state_mux;
}sr_handle_t;

typedef struct {
    wakenet_state_t wakenet_mode;
    esp_mn_state_t state;
    int command_id;
} sr_result_t;

typedef struct {
    const esp_afe_sr_iface_t *afe_handle;
    esp_afe_sr_data_t *afe_data;
    int16_t *afe_in_buffer;
    int16_t *afe_out_buffer;
    TaskHandle_t feed_task;
    TaskHandle_t detect_task;
    TaskHandle_t audio_handle_task;
} sr_data_t;

typedef struct
{
    int data_len;
    uint8_t data[MAX_AUDIO_DATA_LEN];
}audio_data_t;

typedef struct
{
    int len;
    uint8_t *data;
}tts_data_t;


esp_err_t app_sr_start(void);
int get_sr_state(void);
void set_sr_state(int new_state);

#endif //APP_SR_H
