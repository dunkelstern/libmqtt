#include "mqtt_internal.h"
#include "state_queue.h"
#include "debug.h"

#if 0
static inline void dump_expected(MQTTHandle *handle) {
    MQTTCallbackQueueItem *item = handle->queue.pending;

    DEBUG_LOG("Expected packets:")

    while (item != NULL) {
        DEBUG_LOG(" - Type: %d, packet_id: %d", item->type, item->packet_id);

        item = item->next;
    }
}
#endif

void expect_packet(MQTTHandle *handle, MQTTControlPacketType type, uint16_t packet_id, MQTTEventHandler callback, void *context) {
    MQTTCallbackQueueItem *item = (MQTTCallbackQueueItem *)calloc(1, sizeof(MQTTCallbackQueueItem));

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

            break;
        }
        prev_item = item;
        item = item->next;
    }
}

bool dispatch_packet(MQTTHandle *handle, MQTTPacket *packet) {
    MQTTCallbackQueueItem *item = handle->queue.pending;
    uint16_t packet_id = get_packet_id(packet);

    while (item != NULL) {
        if ((item->type == packet->packet_type) && (item->packet_id == packet_id)) {
            remove_from_queue(handle, item);
            if (item->callback) {
                item->callback(handle, item->context);
            }
            free(item);

            return true;
        }
        item = item->next;
    }

    // not found
    return false;
}
