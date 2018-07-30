#ifndef state_queue_h__included
#define state_queue_h__included

#include <stdint.h>
#include "mqtt.h"
#include "packet.h"

typedef struct _MQTTCallbackQueueItem {
    struct _MQTTCallbackQueueItem *next;

    MQTTControlPacketType type;
    uint16_t packet_id;
    void *context;
    MQTTEventHandler callback;
} MQTTCallbackQueueItem;

typedef struct {
    MQTTCallbackQueueItem *pending;
} MQTTCallbackQueue;

void expect_packet(MQTTHandle *handle, MQTTControlPacketType type, uint16_t packet_id, MQTTEventHandler callback, void *context);
bool dispatch_packet(MQTTHandle *handle, MQTTPacket *packet);

#endif /* state_queue_h__included */
