#ifndef APP_CHAT_H
#define APP_CHAT_H

void start_aliyun_ws_client();
void stop_aliyun_ws_client();
void app_chat_start();
int get_chat_send_status(void);
void func_test_filed_char();
void send_wss_task_finish_msg(void);
void generate_id(char *id);
void init_file_chat_start(void);
void file_chat_stop();
#endif //APP_CHAT_H
