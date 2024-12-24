#ifndef APP_TTS_H
#define APP_TTS_H

void start_aliyun_tts_client();
int get_tts_state(void);
void tts_text_trans(const char *text);
void tts_text_finish();
void stop_aliyun_tts_client();
void app_tts_start();
#endif //APP_TTS_H
