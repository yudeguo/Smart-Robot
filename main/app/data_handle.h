#ifndef DATA_HANDLE_H
#define DATA_HANDLE_H
#include <stdbool.h>

void init_audio_data_queue(int len);
bool push_audio_data_queue(void * data);
bool pop_audio_data_queue(void **data);
void init_tts_data_queue(int len);
bool push_tts_data_queue(void *data);
bool pop_tts_data_queue(void **data);

#endif //DATA_HANDLE_H
