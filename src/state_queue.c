#include "mqtt_internal.h"
#include "state_queue.h"
#include "debug.h"

static inline void dump_expected(MQTTHandle *handle) {
    MQTTCallbackQueueItem *item = handle->queue.pending;

    DEBUG_LOG("Expected packets:")

    while (item != NULL) {
        DEBUG_LOG(" - Type: %d, packet_id: %d", item->type, item->packet_id);

        item = item->next;
    }
}

void expect_packet(MQTTHandle *handle, MQTTControlPacketType type, uint16_t packet_id, MQTTEventHandler callback, void *context) {
    MQTTCallbackQueueItem *item = calloc(1, sizeof(MQTTCallbackQueueItem));

    item->type = type;
    item->packet_id = packet_id;
    item->callback = callback;
    item->context = context;

    // insert at start
    if (handle->queue.pending != NULL) {
        item->next = handle->queue.pending;
    }

    handle->queue.pending = item;
    // dump_expected(handle);
}

static uint16_t get_packet_id(MQTTPacket *packet) {
    switch(packet->packet_type) {
        case PacketTypePublish:
            return ((PublishPayload *)packet->payload)->packet_id;
        case PacketTypePubAck:
            return ((PubAckPayload *)packet->payload)->packet_id;
        case PacketTypePubRec:
            return ((PubRecPayload *)packet->payload)->packet_id;
        case PacketTypePubRel:
            return ((PubRelPayload *)packet->payload)->packet_id;
        case PacketTypePubComp:
            return ((PubCompPayload *)packet->payload)->packet_id;
        case PacketTypeSubscribe:
            return ((SubscribePayload *)packet->payload)->packet_id;
        case PacketTypeSubAck:
            return ((SubAckPayload *)packet->payload)->packet_id;
        case PacketTypeUnsubscribe:
            return ((UnsubscribePayload *)packet->payload)->packet_id;
        case PacketTypeUnsubAck:
            return ((UnsubAckPayload *)packet->payload)->packet_id;
        default:
            return 0; // no packet id in payload
    }
}

void remove_from_queue(MQTTHandle *handle, MQTTCallbackQueueItem *remove) {
    MQTTCallbackQueueItem *item = handle->queue.pending;
    MQTTCallbackQueueItem *prev_item = NULL;

    while (item != NULL) {
        if (item == remove) {
            // remove from queue
            if (prev_item == NULL) {
                // no prev item, attach directly to queue
                handle->queue.pending = item->next;
            } else {
                // attach next item to prev item removing this one
                prev_item->next = item->next;
            }
            free(item);

            break;
        }
        prev_item = item;
        item = item->next;
    }
}

bool dispatch_packet(MQTTHandle *handle, MQTTPacket *packet) {
    MQTTCallbackQueueItem *item = handle->queue.pending;
    MQTTCallbackQueueItem *prev_item = NULL;
    uint16_t packet_id = get_packet_id(packet);

    while (item != NULL) {
        if ((item->type == packet->packet_type) && (item->packet_id == packet_id)) {
            if (item->callback) {
                item->callback(handle, item->context);
            }

            remove_from_queue(handle, item);
            return true;
        }
        prev_item = item;
        item = item->next;
    }

    // not found
    return false;
}
