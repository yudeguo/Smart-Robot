#include  "app_chat.h"

#include <app_network.h>
#include <app_sr.h>
#include <app_tts.h>
#include <data_handle.h>
#include <esp_crt_bundle.h>
#include <esp_log.h>
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "esp_websocket_client.h"
#include "freertos/task.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#include "esp_http_client.h"
#include "mbedtls/md.h"
#include "mbedtls/base64.h"
#include "esp_tls.h"
#include "conf.h"

struct app_chat_t
{
    uint8_t send_status;
    SemaphoreHandle_t send_mux;
};

#define HTTP_RSP_BUF_LEN              2048
#define VOICE_TO_FILED_STR_LEN        1024

#define ALIYUN_WSS_SERVER_ADDR        "wss://dashscope.aliyuncs.com/api-ws/v1/inference/"
#define ALIYUN_WSS_UUID_SIZE           64
#define DEFAULT_UUID_CHARSET          "0123456789abcdef"
#define ALIYUN_CHAT_SERVER_ADDR       "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"

static char * TAG = "app_chat";
static esp_websocket_client_handle_t s_wss_client;
static struct app_chat_t s_chat_handle;
static char s_wss_task_id[64];
static esp_http_client_handle_t s_http_client;
static char *s_http_rsp_buf = NULL;
static char *s_voice_to_filed_str = NULL;

static void chat_state_unlock(void)
{
    assert(s_chat_handle.send_mux && "app_chat_start must be called first");
    xSemaphoreGiveRecursive(s_chat_handle.send_mux);
}

static bool chat_state_lock(uint32_t timeout_ms)
{
    assert(s_chat_handle.send_mux && "app_chat_start must be called first");
    const TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_chat_handle.send_mux, timeout_ticks) == pdTRUE;
}

static void set_chat_send_status(const uint8_t status)
{
    chat_state_lock(0);
    s_chat_handle.send_status = status;
    chat_state_unlock();
}

int get_chat_send_status(void)
{
    chat_state_lock(0);
    uint8_t status = s_chat_handle.send_status;
    chat_state_unlock();
    return status;
}

static void generate_random_char(char *buffer, size_t length)
{
    size_t charset_size = strlen(DEFAULT_UUID_CHARSET);  // 字符集大小
    for (size_t i = 0; i < length; i++) {
        buffer[i] = DEFAULT_UUID_CHARSET[rand() % charset_size];  // 从字符集中随机选择字符
    }
    buffer[length] = '\0';
}

void generate_id(char *id)
{
    char part1[9], part2[5], part3[5], part4[5], part5[13];
    // 初始化随机数种子
    srand((unsigned int)time(NULL));
    // 生成各个部分
    generate_random_char(part1, 8);  // A 部分
    generate_random_char(part2, 4);  // B 部分
    generate_random_char(part3, 4);  // C 部分
    generate_random_char(part4, 4);  // D 部分
    generate_random_char(part5, 12); // E 部分
    // 将生成的部分拼接成UUID格式
    snprintf(id, ALIYUN_WSS_UUID_SIZE, "%s%s%s%s%s", part1, part2, part3, part4, part5);
}

static void send_start_platform_trans_msg()
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
    generate_id(s_wss_task_id);
    cJSON_AddStringToObject(head_obj,"task_id",s_wss_task_id);
    cJSON_AddStringToObject(head_obj,"action","run-task");
    cJSON_AddStringToObject(head_obj,"streaming","duplex");
    cJSON_AddItemToObject(root_obj,"header",head_obj);

    cJSON_AddStringToObject(payload_obj,"task_group","audio");
    cJSON_AddStringToObject(payload_obj,"task","asr");
    cJSON_AddStringToObject(payload_obj,"function","recognition");
    cJSON_AddStringToObject(payload_obj,"model","paraformer-realtime-v2");
    cJSON *param_obj = cJSON_CreateObject();
    if (NULL != param_obj) {
        cJSON_AddStringToObject(param_obj,"format","pcm");
        cJSON_AddNumberToObject(param_obj,"sample_rate",16000);
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
        esp_websocket_client_send_text(s_wss_client, content, (int)strlen(content), portMAX_DELAY);
        free(content);
    }
    cJSON_Delete(root_obj);
}

void send_wss_task_finish_msg(void)
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
    cJSON_AddStringToObject(head_obj,"task_id",s_wss_task_id);
    cJSON_AddStringToObject(head_obj,"action","finish-task");
    cJSON_AddStringToObject(head_obj,"streaming","duplex");
    cJSON_AddItemToObject(root_obj,"header",head_obj);
    cJSON *input_obj = cJSON_CreateObject();
    if (NULL != input_obj) {
        cJSON_AddItemToObject(payload_obj,"input",input_obj);
    }
    cJSON_AddItemToObject(root_obj,"payload",payload_obj);
    char *content = cJSON_PrintUnformatted(root_obj);
    if (NULL != content) {
        printf("Send wss finish message:%s\n",content);
        esp_websocket_client_send_text(s_wss_client, content, (int)strlen(content), portMAX_DELAY);
        free(content);
    }
    cJSON_Delete(root_obj);
}

static void phrase_wss_event_msg(const char *msg)
{
    char ws_event[32]={0};
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
        strncpy(ws_event,filed_obj->valuestring,sizeof(ws_event)-1);
    }
    if (0 == strcmp(ws_event,"task-started")) {
        set_chat_send_status(1);
    }else if(0 == strcmp(ws_event,"result-generated")){
        cJSON* payload_obj = cJSON_GetObjectItem(obj,"payload");
        if (NULL == payload_obj) {
            goto end;
        }
        cJSON *output_obj = cJSON_GetObjectItem(payload_obj,"output");
        if (NULL == output_obj) {
            goto end;
        }
        cJSON *sentence_obj = cJSON_GetObjectItem(output_obj,"sentence");
        if (NULL == sentence_obj) {
            goto end;
        }
        filed_obj = cJSON_GetObjectItem(sentence_obj,"sentence_end");
        if (NULL != filed_obj) {
            if (filed_obj->valueint == true) {
                cJSON *txt_obj = cJSON_GetObjectItem(sentence_obj,"text");
                if (NULL != txt_obj) {
                    ESP_LOGI(TAG, "=====Sentence: [%s]=======", txt_obj->valuestring);
                    strncpy(s_voice_to_filed_str,txt_obj->valuestring,VOICE_TO_FILED_STR_LEN-1);
                    set_sr_state(APP_SR_TTS_SERVER_START);
                }
            }
        }
    }
end:
    cJSON_Delete(obj);
}

static void wss_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    char *rcv_buf = NULL;
    switch (event_id) {
        case WEBSOCKET_EVENT_BEGIN:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_BEGIN");
        break;
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
            send_start_platform_trans_msg();
        break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        break;
        case WEBSOCKET_EVENT_DATA:
            ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA");
            ESP_LOGI(TAG, "Received=%.*s", data->data_len, (char *)data->data_ptr);
            if (data->op_code == 0x08 && data->data_len == 2) {
                ESP_LOGW(TAG, "Received closed message with code=%d", 256 * data->data_ptr[0] + data->data_ptr[1]);
            }else {
                rcv_buf = (char *)malloc(data->data_len+1);
                if (NULL != rcv_buf) {
                    memset(rcv_buf,0,data->data_len+1);
                    memcpy(rcv_buf, data->data_ptr, data->data_len);
                    phrase_wss_event_msg(rcv_buf);
                    free(rcv_buf);
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

void start_aliyun_ws_client()
{
    char api_key[64]={0};
    esp_websocket_client_config_t websocket_cfg = {};
    websocket_cfg.uri = ALIYUN_WSS_SERVER_ADDR;
    websocket_cfg.crt_bundle_attach = esp_crt_bundle_attach;
    websocket_cfg.use_global_ca_store = true;
    s_wss_client = esp_websocket_client_init(&websocket_cfg);
    sniprintf(api_key,sizeof(api_key),"bearer %s", ALIYUN_API_KEY);
    esp_websocket_client_append_header(s_wss_client, "Authorization", api_key);
    esp_websocket_client_append_header(s_wss_client, "X-DashScope-DataInspection", "enable");
    esp_websocket_register_events(s_wss_client, WEBSOCKET_EVENT_ANY, wss_event_handler, (void *)s_wss_client);
    esp_websocket_client_start(s_wss_client);
}

void stop_aliyun_ws_client()
{
    esp_websocket_client_stop(s_wss_client);
    esp_websocket_client_destroy(s_wss_client);
    s_wss_client = NULL;
    set_chat_send_status(0);
}

static void audio_send_task(void *args)
{
    audio_data_t *data = NULL;
    while (true) {
        pop_audio_data_queue((void **)&data);
        if (data != NULL) {
            int status = get_chat_send_status();
            if (s_wss_client != NULL &&  status) {
                esp_websocket_client_send_bin(s_wss_client, (char*)data->data,data->data_len,portMAX_DELAY);
            }
            //free(data);
            heap_caps_free(data);
            data = NULL;
        }
    }
}

static void  phrase_filed_chat_rsp(const char *content)
{
    cJSON *obj = cJSON_Parse(content);
    if (obj == NULL) {
        return;
    }
    cJSON *choices_arr = cJSON_GetObjectItem(obj, "choices");
    if (NULL == choices_arr) {
        cJSON_Delete(obj);
        return;
    }
    int index = cJSON_GetArraySize(choices_arr);
    for (int i = 0; i < index; i++) {
        cJSON *arr_filed = cJSON_GetArrayItem(choices_arr, i);
        if (NULL == arr_filed) {
            continue;
        }
        cJSON * filed_obj = cJSON_GetObjectItem(arr_filed, "finish_reason");
        if (cJSON_IsNull(filed_obj)) {
            cJSON *delta_obj = cJSON_GetObjectItem(arr_filed, "delta");
            if (NULL != delta_obj) {
                cJSON* content_obj = cJSON_GetObjectItem(delta_obj,"content");
                if (NULL != content_obj) {
                    ESP_LOGI(TAG, "[%s]",content_obj->valuestring);
                    tts_text_trans(content_obj->valuestring);
                }
            }
        }else {
            ESP_LOGI(TAG, "Send tts finish message");
            tts_text_finish();
        }

    }
    cJSON_Delete(obj);
}

esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
        break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
        break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT, header sent");
        break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
        break;
        case HTTP_EVENT_ON_DATA:
            if (evt->data) {
                //ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
                //ESP_LOGI(TAG, "Response: %.*s", evt->data_len, (char *)evt->data);
                memset(evt->user_data,0,HTTP_RSP_BUF_LEN);
                memcpy(evt->user_data, evt->data+strlen("data: "), evt->data_len-strlen("data: "));
                phrase_filed_chat_rsp(evt->user_data);
            }
        break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_FINISHED");
        break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
        break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
        break;
    }
    return ESP_OK;
}

static void build_http_filed_chat(cJSON *root,const char *content)
{
    cJSON_AddStringToObject(root,"model","qwen-plus");
    cJSON *msg_array = cJSON_CreateArray();
    if (NULL == msg_array) {
        return;
    }
    cJSON *filed_obj = cJSON_CreateObject();
    if (NULL == filed_obj) {
        cJSON_Delete(msg_array);
        return;
    }
    cJSON_AddStringToObject(filed_obj,"role","user");
    cJSON_AddStringToObject(filed_obj,"content",content);
    cJSON_AddItemToArray(msg_array,filed_obj);
    cJSON_AddBoolToObject(root,"stream", true);
    cJSON_AddItemToObject(root,"messages",msg_array);
}

void init_file_chat_start(void)
{
    char *send_str = NULL;
    esp_http_client_config_t config ={
        .url = ALIYUN_CHAT_SERVER_ADDR,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .user_data = s_http_rsp_buf,
        .timeout_ms = 10000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .use_global_ca_store = true,
    };
    s_http_client = esp_http_client_init(&config);
    esp_http_client_set_header(s_http_client, "Authorization", "Bearer "ALIYUN_API_KEY);
    esp_http_client_set_header(s_http_client, "Content-Type", "application/json");
    cJSON *root = cJSON_CreateObject();
    if (NULL != root) {
        build_http_filed_chat(root,s_voice_to_filed_str);
        send_str = cJSON_PrintUnformatted(root);
        ESP_LOGI(TAG,"http send content: %s",send_str);
        esp_http_client_set_post_field(s_http_client, send_str, (int)strlen(send_str));
    }
    esp_err_t err = esp_http_client_perform(s_http_client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status Code: %d", esp_http_client_get_status_code(s_http_client));
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }
    if (NULL != send_str) {
        free(send_str);
        send_str = NULL;
    }
}

void file_chat_stop()
{
    esp_http_client_cleanup(s_http_client);
}

void func_test_filed_char()
{
    while (true) {
        int wifi_status = get_wifi_connected_status();
        if (wifi_status) {
            ESP_LOGI(TAG, "WiFi connected start chat");
            break;
        }
    }
    //init_file_chat_start("讲一个哪吒闹海的故事");
}

void app_chat_start()
{
    s_chat_handle.send_mux = xSemaphoreCreateMutex();
    assert(s_chat_handle.send_mux);
    s_http_rsp_buf = (char *)malloc(HTTP_RSP_BUF_LEN);
    assert(s_http_rsp_buf);
    memset(s_http_rsp_buf,0,HTTP_RSP_BUF_LEN);
    s_voice_to_filed_str = (char*)malloc(VOICE_TO_FILED_STR_LEN);
    assert(s_voice_to_filed_str);
    BaseType_t ret = xTaskCreatePinnedToCore(audio_send_task,"NetWork Task", 4 * 1024, NULL, 1, NULL, 0);
    ESP_ERROR_CHECK_WITHOUT_ABORT((pdPASS == ret) ? ESP_OK : ESP_FAIL);
}




