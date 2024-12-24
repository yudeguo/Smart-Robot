//
// Created by GS11DZ02467 on 24-12-14.
//

#ifndef APP_AUDIO_H
#define APP_AUDIO_H

void audio_play_task(void *arg);
void create_tts_audio();
void write_tts_audio(const char *data, int len);
void close_tts_audio();

#endif //APP_AUDIO_H
