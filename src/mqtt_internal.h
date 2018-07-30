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

    int sock;
    bool reader_alive;

    uint16_t packet_id_counter;

    MQTTCallbackQueue queue;
    PlatformData *platform;
};

#endif /* mqtt_internal_h__included */
