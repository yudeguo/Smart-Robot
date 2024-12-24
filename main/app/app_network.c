#include "app_network.h"

#include <esp_check.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_wifi_default.h>
#include <string.h>
#include <esp_log.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include  "conf.h"

#define WIFI_EVENT_QUEUE_SIZE           4
#define ESP_WIFI_MAXIMUM_RETRY               3
#define WIFI_CONNECTED_BIT              BIT0
#define WIFI_FAIL_BIT                   BIT1
#define portTICK_RATE_MS                10

static QueueHandle_t wifi_event_queue = NULL;
static EventGroupHandle_t s_wifi_event_group;
static const char *TAG = "app-network";
static int s_retry_num = 0;
static bool wifi_connected = false;
static bool s_reconnect = true;
static SemaphoreHandle_t s_wifi_mux;

static void wifi_state_unlock(void)
{
    assert(s_wifi_mux && "app_network_start must be called first");
    xSemaphoreGiveRecursive(s_wifi_mux);
}

static bool wifi_state_lock(uint32_t timeout_ms)
{
    assert(s_wifi_mux && "app_network_start must be called first");
    const TickType_t timeout_ticks = (timeout_ms == 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(s_wifi_mux, timeout_ticks) == pdTRUE;
}

bool get_wifi_connected_status(void)
{
    wifi_state_lock(0);
    const bool connected = wifi_connected;
    wifi_state_unlock();
    return connected;
}

void set_wifi_connected_status(const bool connected)
{
    wifi_state_lock(0);
    wifi_connected = connected;
    wifi_state_unlock();
}

esp_err_t send_network_event(net_event_t event)
{
    net_event_t eventOut = event;
    BaseType_t ret_val = xQueueSend(wifi_event_queue, &eventOut, 0);
    if (NET_EVENT_RECONNECT == event) {
        set_wifi_connected_status(false);
    }
    ESP_RETURN_ON_FALSE(pdPASS == ret_val, ESP_ERR_INVALID_STATE, TAG, "The last event has not been processed yet");
    return ESP_OK;
}


static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        send_network_event(NET_EVENT_POWERON_SCAN);
        ESP_LOGI(TAG, "start connect to the AP");
    }else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_reconnect && ++s_retry_num < ESP_WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            ESP_LOGI(TAG, "sta disconnect, retry attempt %d...", s_retry_num);
        }else {
            ESP_LOGI(TAG, "sta disconnected");
        }
        xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        set_wifi_connected_status(false);
    }else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
        ESP_LOGI(TAG, "STA got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        set_wifi_connected_status(true);
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                ESP_EVENT_ANY_ID,
                &event_handler,
                NULL,
                &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                IP_EVENT_STA_GOT_IP,
                &event_handler,
                NULL,
                &instance_got_ip));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = {0},
            .password = {0},
        },
    };
    memcpy(wifi_config.sta.ssid, WIFI_TESTING_SSID, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, WIFI_TESTING_PASSWORD, sizeof(wifi_config.sta.password));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(TAG, "wifi_init_sta finished.%s, %s", \
             (char*)wifi_config.sta.ssid, (char*)wifi_config.sta.password);
}

static void network_task(void *args)
{
    net_event_t net_event;
    wifi_init_sta();
    while (true) {
        if (pdPASS == xQueueReceive(wifi_event_queue, &net_event, portTICK_RATE_MS / 5)) {
            switch (net_event) {
                case NET_EVENT_POWERON_SCAN:
                    ESP_LOGI(TAG, "Power on scan...");
                    esp_wifi_connect();
                    set_wifi_connected_status(false);
                break;
                default:
                    ESP_LOGE(TAG, "Unknown event %d", net_event);
                    break;
            }
        }
        vTaskDelete(NULL);
    }
}

void app_network_start()
{
    s_wifi_mux = xSemaphoreCreateMutex();
    assert(s_wifi_mux);
    wifi_event_queue = xQueueCreate(WIFI_EVENT_QUEUE_SIZE, sizeof(net_event_t));
    ESP_ERROR_CHECK_WITHOUT_ABORT((wifi_event_queue) ? ESP_OK : ESP_FAIL);
    BaseType_t ret = xTaskCreatePinnedToCore(network_task,"NetWork Task", 4 * 1024, NULL, 1, NULL, 0);
    ESP_ERROR_CHECK_WITHOUT_ABORT((pdPASS == ret) ? ESP_OK : ESP_FAIL);
}
