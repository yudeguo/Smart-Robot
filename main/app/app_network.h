#ifndef APP_NETWORK_H
#define APP_NETWORK_H
#include <stdbool.h>

typedef enum {
    NET_EVENT_NONE = 0,
    NET_EVENT_RECONNECT,
    NET_EVENT_SCAN,
    NET_EVENT_NTP,
    NET_EVENT_WEATHER,
    NET_EVENT_POWERON_SCAN,
    NET_EVENT_MAX,
} net_event_t;

void app_network_start(void);
bool get_wifi_connected_status(void);

#endif //APP_NETWORK_H
