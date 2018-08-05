#ifndef mqtt_internal_h__included
#define mqtt_internal_h__included

#include "mqtt.h"
#include "packet.h"
#include "subscriptions.h"
#include "state_queue.h"

typedef struct _PlatformData PlatformData;

struct _MQTTHandle {
    MQTTConfig *config;

    MQTTErrorHandler error_handler;
    Subscriptions subscriptions;

    bool reader_alive;
    int read_task_handle;

    uint16_t packet_id_counter;

    MQTTCallbackQueue queue;
    PlatformData *platform;

    bool reconnecting;
    int keepalive_timer;
};

void mqtt_free(MQTTHandle *handle);

#ifndef KEEPALIVE_INTERVAL
#define KEEPALIVE_INTERVAL 60
#endif

#endif /* mqtt_internal_h__included */
