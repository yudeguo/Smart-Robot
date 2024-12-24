#include "data_handle.h"
#include "freertos/FreeRTOS.h"
#include <freertos/queue.h>

static char * TAG = "data_handle";
static QueueHandle_t s_audio_queue;
static QueueHandle_t s_tts_queue;


void init_audio_data_queue(int len)
{
    s_audio_queue = xQueueCreate(len ,sizeof(void *));
    assert(s_audio_queue);
}

bool push_audio_data_queue(void *data)
{
    return xQueueSend(s_audio_queue, &data, portMAX_DELAY);
}

bool pop_audio_data_queue(void **data)
{
    return xQueueReceive(s_audio_queue, data, portMAX_DELAY);
}

void init_tts_data_queue(int len)
{
    s_tts_queue = xQueueCreate(len ,sizeof(void *));
    assert(s_tts_queue);
}

bool push_tts_data_queue(void *data)
{
    return xQueueSend(s_tts_queue, &data, portMAX_DELAY);
}

bool pop_tts_data_queue(void **data)
{
    return xQueueReceive(s_tts_queue, data, portMAX_DELAY);
}
