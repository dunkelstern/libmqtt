#ifndef mqtt_internal_h__included
#define mqtt_internal_h__included

#include "mqtt.h"
#include "packet.h"

typedef struct _PlatformData PlatformData;

typedef struct {
    char *topic;
    MQTTEventHandler *handler;
    bool pending;
} SubscriptionItem;

typedef struct {
    SubscriptionItem *items;
    uint8_t num_items;
} Subscriptions;

typedef void (*MQTTCallback)(MQTTHandle *handle, MQTTPacket *packet, void *context);

typedef struct {
    MQTTControlPacketType type;
    uint16_t packet_id;
    void *context;
    MQTTCallback callback;
} MQTTCallbackQueueItem;

typedef struct {
    MQTTCallbackQueueItem *pending;
    uint8_t num_items;
} MQTTCallbackQueue;

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
