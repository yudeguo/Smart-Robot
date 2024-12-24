#include "app_tts.h"

#include <app_audio.h>
#include <app_chat.h>
#include <app_network.h>
#include <app_sr.h>
#include <cJSON.h>
#include <data_handle.h>
#include <esp32s3_bsp_board.h>
#include <esp_crt_bundle.h>
#include <esp_http_client.h>
#include <esp_log.h>
#include <esp_websocket_client.h>
#include <func_testing.h>
#include "conf.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
static const char *TAG = "app_tts";
static esp_websocket_client_handle_t s_tts_client;
static char g_tts_task_id[64];
static uint8_t g_tts_start_trans_flag = 0;
static SemaphoreHandle_t s_tts_mux;;

#define ALIYUN_TTS_SERVER_ADDR "wss://dashscope.aliyuncs.com/api-ws/v1/inference"


static void tts_state_unlock(void)
{
    assert(s_tts_mux && "app_tts_start must be called first");
    xSemaphoreGiveRecursive(s_tts_mux);
}

static bool tts_state_lock(uint32_t timeout_ms)
{
    assert(s_tts_mux && "bsp_display_start must be called first");
    const TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_tts_mux, timeout_ticks) == pdTRUE;
}

int get_tts_state(void)
{
    tts_state_lock(0);
    const uint8_t state = g_tts_start_trans_flag;
    tts_state_unlock();
    return state;
}

void set_tts_state(int new_state)
{
    tts_state_lock(0);
    g_tts_start_trans_flag = new_state;
    tts_state_unlock();
}


static void send_tts_run_task_msg(void)
{
    cJSON *root_obj = cJSON_CreateObject();
    if (NULL == root_obj) {
        return;
    }
    cJSON *head_obj = cJSON_CreateObject();
    if (NULL == head_obj) {
        cJSON_Delete(root_obj);
        return;
    }
    cJSON *payload_obj = cJSON_CreateObject();
    if (NULL == payload_obj) {
        cJSON_Delete(root_obj);
        cJSON_Delete(head_obj);
        return;
    }
    generate_id(g_tts_task_id);
    cJSON_AddStringToObject(head_obj,"action","run-task");
    cJSON_AddStringToObject(head_obj,"task_id",g_tts_task_id);
    cJSON_AddStringToObject(head_obj,"streaming","duplex");
    cJSON_AddItemToObject(root_obj,"header",head_obj);
    cJSON_AddStringToObject(payload_obj,"task_group","audio");
    cJSON_AddStringToObject(payload_obj,"task","tts");
    cJSON_AddStringToObject(payload_obj,"function","SpeechSynthesizer");
    cJSON_AddStringToObject(payload_obj,"model","cosyvoice-v1");
    cJSON *param_obj = cJSON_CreateObject();
    if (NULL != param_obj) {
        cJSON_AddStringToObject(param_obj,"text_type","PlainText");
        cJSON_AddStringToObject(param_obj,"voice","longwan");
        cJSON_AddStringToObject(param_obj,"format","pcm");
        cJSON_AddNumberToObject(param_obj,"sample_rate",16000);
        cJSON_AddNumberToObject(param_obj,"volume",60);
        cJSON_AddNumberToObject(param_obj,"rate",1);
        cJSON_AddNumberToObject(param_obj,"pitch",1);
        cJSON_AddItemToObject(payload_obj,"parameters",param_obj);
    }
    cJSON *input_obj = cJSON_CreateObject();
    if (NULL != input_obj) {
        cJSON_AddItemToObject(payload_obj,"input",input_obj);
    }
    cJSON_AddItemToObject(root_obj,"payload",payload_obj);
    char *content = cJSON_PrintUnformatted(root_obj);
    if (NULL != content) {
        printf("Send web socket start:%s\n",content);
        esp_websocket_client_send_text(s_tts_client, content, (int)strlen(content), portMAX_DELAY);
        free(content);
    }
    cJSON_Delete(root_obj);
}

void tts_text_trans(const char *text)
{
    cJSON *root_obj = cJSON_CreateObject();
    if (NULL == root_obj) {
        return;
    }
    cJSON *head_obj = cJSON_CreateObject();
    if (NULL == head_obj) {
        cJSON_Delete(root_obj);
        return;
    }
    cJSON *payload_obj = cJSON_CreateObject();
    if (NULL == payload_obj) {
        cJSON_Delete(root_obj);
        cJSON_Delete(head_obj);
        return;
    }
    cJSON_AddStringToObject(head_obj,"action","continue-task");
    cJSON_AddStringToObject(head_obj,"task_id",g_tts_task_id);
    cJSON_AddStringToObject(head_obj,"streaming","duplex");
    cJSON_AddItemToObject(root_obj,"header",head_obj);
    cJSON *input_obj = cJSON_CreateObject();
    if (NULL != input_obj) {
        cJSON_AddStringToObject(input_obj,"text",text);
        cJSON_AddItemToObject(payload_obj,"input",input_obj);
    }
    cJSON_AddItemToObject(root_obj,"payload",payload_obj);
    char *content = cJSON_PrintUnformatted(root_obj);
    if (NULL != content) {
        printf("Send web socket start:%s\n",content);
        esp_websocket_client_send_text(s_tts_client, content, (int)strlen(content), portMAX_DELAY);
        free(content);
    }
    cJSON_Delete(root_obj);
}


void tts_text_finish(void)
{
    cJSON *root_obj = cJSON_CreateObject();
    if (NULL == root_obj) {
        return;
    }
    cJSON *head_obj = cJSON_CreateObject();
    if (NULL == head_obj) {
        cJSON_Delete(root_obj);
        return;
    }
    cJSON *payload_obj = cJSON_CreateObject();
    if (NULL == payload_obj) {
        cJSON_Delete(root_obj);
        cJSON_Delete(head_obj);
        return;
    }
    cJSON_AddStringToObject(head_obj,"action","finish-task");
    cJSON_AddStringToObject(head_obj,"task_id",g_tts_task_id);
    cJSON_AddStringToObject(head_obj,"streaming","duplex");
    cJSON_AddItemToObject(root_obj,"header",head_obj);
    cJSON *input_obj = cJSON_CreateObject();
    if (NULL != input_obj) {
        cJSON_AddItemToObject(payload_obj,"input",input_obj);
    }
    cJSON_AddItemToObject(root_obj,"payload",payload_obj);
    char *content = cJSON_PrintUnformatted(root_obj);
    if (NULL != content) {
        printf("Send web socket start:%s\n",content);
        esp_websocket_client_send_text(s_tts_client, content, (int)strlen(content), portMAX_DELAY);
        free(content);
    }
    cJSON_Delete(root_obj);
}

static void phrase_tts_event_msg(const char *msg)
{
    cJSON *obj = cJSON_Parse(msg);
    if (NULL == obj) {
        return;
    }
    cJSON *head_obj = cJSON_GetObjectItem(obj,"header");
    if (NULL == head_obj) {
        cJSON_Delete(obj);
        return;
    }
    cJSON *filed_obj = cJSON_GetObjectItem(head_obj,"event");
    if (NULL != filed_obj) {
        if (0 == strcmp(filed_obj->valuestring,"task-started")) {
            g_tts_start_trans_flag = 1;
            create_tts_audio();
            ESP_LOGI(TAG,"Receive server start task....");
        }
        if (0 == strcmp(filed_obj->valuestring,"task-finished")) {
            ESP_LOGI(TAG,"Receive server end task....");
            close_tts_audio();
            set_sr_state(APP_SR_END_FIELD_CHART);
        }
    }
    cJSON_Delete(obj);
}

static void wss_tts_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    char *rcv_buf = NULL;
    switch (event_id) {
        case WEBSOCKET_EVENT_BEGIN:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_BEGIN");
        break;
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
            send_tts_run_task_msg();
        break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
            esp_websocket_client_start(s_tts_client);
        break;
        case WEBSOCKET_EVENT_DATA:
           // ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA [%d]",data->op_code);
        if (data->op_code == 0x08 && data->data_len == 2) {
            ESP_LOGW(TAG, "Received closed message with code=%d", 256 * data->data_ptr[0] + data->data_ptr[1]);
        }else {
                if (data->op_code == 0x1) {
                    ESP_LOGI(TAG, "Received=%.*s", data->data_len, (char *)data->data_ptr);
                    rcv_buf = (char *)malloc(data->data_len+1);
                    if (NULL != rcv_buf) {
                        memset(rcv_buf,0,data->data_len+1);
                        memcpy(rcv_buf, data->data_ptr, data->data_len);
                        phrase_tts_event_msg(rcv_buf);
                        free(rcv_buf);
                    }
                }
                if (data->op_code == 0x2) {
                   // size_t write_bytes;
                    //esp32s3_bsp_i2s_write((void*)data->data_ptr,data->data_len,&write_bytes,portMAX_DELAY);
                    //write_tts_audio(data->data_ptr, data->data_len);
                    //tts_data_t *tts_data= (tts_data_t *)malloc(sizeof(tts_data_t));
                    tts_data_t *tts_data = heap_caps_calloc(1, sizeof(tts_data_t), MALLOC_CAP_SPIRAM);
                    if (NULL != tts_data) {
                        //tts_data->data = (uint8_t *)malloc(data->data_len+1);
                        tts_data->data = heap_caps_calloc(1, data->data_len+1, MALLOC_CAP_SPIRAM);
                        if (NULL != tts_data->data) {
                            memset(tts_data->data,0,data->data_len+1);
                            memcpy(tts_data->data, data->data_ptr, data->data_len);
                            tts_data->len = data->data_len;
                            push_tts_data_queue(tts_data);
                        }else {
                            free(tts_data);
                        }
                    }

                    ESP_LOGI(TAG,"I2S write %d success",data->data_len);
                }
        }
        break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
        break;
        case WEBSOCKET_EVENT_FINISH:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_FINISH");
        break;
        case  WEBSOCKET_EVENT_CLOSED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_CLOSED");
        break;
    }
}

void start_aliyun_tts_client()
{
    char api_key[64]={0};
    esp_websocket_client_config_t websocket_cfg = {};
    websocket_cfg.uri = ALIYUN_TTS_SERVER_ADDR;
    websocket_cfg.crt_bundle_attach = esp_crt_bundle_attach;
    websocket_cfg.use_global_ca_store = true;
    websocket_cfg.reconnect_timeout_ms = 5000;
    s_tts_client = esp_websocket_client_init(&websocket_cfg);
    sniprintf(api_key,sizeof(api_key),"bearer %s",ALIYUN_API_KEY);
    esp_websocket_client_append_header(s_tts_client, "Authorization", api_key);
    esp_websocket_client_append_header(s_tts_client, "X-DashScope-DataInspection", "enable");
    esp_websocket_register_events(s_tts_client, WEBSOCKET_EVENT_ANY, wss_tts_event_handler, (void *)s_tts_client);
    esp_websocket_client_start(s_tts_client);
}

void stop_aliyun_tts_client()
{
    esp_websocket_client_stop(s_tts_client);
    esp_websocket_client_destroy(s_tts_client);
    s_tts_client = NULL;
    set_tts_state(0);
}

static void tts_audio_play(void *args)
{
    tts_data_t *data = NULL;
    while (true) {
        pop_tts_data_queue((void **)&data);
        if (data != NULL) {
            size_t write_bytes = 0;
            esp32s3_bsp_i2s_write((void*)data->data,data->len,&write_bytes,portMAX_DELAY);
            //free(data->data);
            heap_caps_free(data->data);
            //free(data);
            heap_caps_free(data);
        }
    }
}

void app_tts_start()
{
    s_tts_mux = xSemaphoreCreateMutex();
    assert(s_tts_mux);
    init_tts_data_queue(100);
    BaseType_t ret = xTaskCreatePinnedToCore(tts_audio_play,"NetWork Task", 4 * 1024, NULL, 1, NULL, 0);
    ESP_ERROR_CHECK_WITHOUT_ABORT((pdPASS == ret) ? ESP_OK : ESP_FAIL);
}


void func_test_tts(void)
{
    while (true) {
        if (get_wifi_connected_status()) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    start_aliyun_tts_client();
    while (true) {
        if (g_tts_start_trans_flag == 1) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    tts_text_trans("小冰退下了");
    tts_text_finish();
}