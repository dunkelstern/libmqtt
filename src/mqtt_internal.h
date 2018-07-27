#ifndef mqtt_internal_h__included
#define mqtt_internal_h__included

#include "mqtt.h"

typedef struct _PlatformData PlatformData;

typedef struct {
    char *topic;
    MQTTEventHandler *handler;
    bool pending;
} Subscriptions;

struct _MQTTHandle {
    MQTTErrorHandler *errorHandler;
    Subscriptions *subscriptions;
    int num_subscriptions;

    int sock;

    // TODO: status queue (Waiting for ACK)
    PlatformData *platform;
};

#endif /* mqtt_internal_h__included */
