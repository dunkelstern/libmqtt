#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include "mqtt.h"
#include "mqtt_internal.h"
#include "debug.h"
#include "packet.h"
#include "platform.h"
#include "protocol.h"

void mqtt_free(MQTTHandle *handle) {
    platform_release(handle);
    remove_all_subscriptions(handle);
    free(handle);
}

/*
 * State handling
 */

 void cleanup_session(MQTTHandle *handle) {
    // Remove all waiting packets
    clear_packet_queue(handle);
}

static bool resubscribe(MQTTHandle *handle) {
    // re-subscribe to all topics
    SubscriptionItem *item = handle->subscriptions.items;
    while (item != NULL) {
        if (!send_subscribe_packet(handle, item->topic, item->qos)) {
            DEBUG_LOG("Error sending subscribe packet");
            return false;
        }
        item = item->next;
    }

    return true;
}

/*
 * Keepalive
 */

static void _keepalive_callback(MQTTHandle *handle, int timer_handle) {
    MQTTErrorCode result = send_ping_packet(handle);
    if (result != MQTT_Error_Ok) {
        bool reconnect = handle->error_handler(handle, handle->config, result);
        if (reconnect) {
            // FIXME: This will lead to stack overflow
            mqtt_reconnect(handle, NULL, NULL);
        } else {
            (void)platform_destroy_timer(handle, handle->keepalive_timer);
            handle->keepalive_timer = -1;
            (void)platform_disconnect(handle);
            (void)platform_cleanup_task(handle, handle->read_task_handle);
            handle->read_task_handle = -1;
            mqtt_free(handle);
        }
        DEBUG_LOG("Sending PINGREQ packet failed!");
    }
}

/*
 * Packet parser
 */

static inline void parse_packet(MQTTHandle *handle, MQTTPacket *packet) {
    // DEBUG_LOG("Packet, type: %s, packet_id: %d", get_packet_name(packet), get_packet_id(packet));

    switch (packet->packet_type) {
        case PacketTypeConnAck:
            if (!dispatch_packet(handle, packet)) {
                DEBUG_LOG("Unexpected packet! (type: CONNACK)");
                (void)platform_destroy_timer(handle, handle->keepalive_timer);
                handle->keepalive_timer = -1;
                (void)platform_disconnect(handle);
            } else {
                ConnAckPayload *payload = (ConnAckPayload *)packet->payload;
                if ((!payload->session_present) && (handle->reconnecting)) {
                    cleanup_session(handle);
                    if (resubscribe(handle) != true) {
                        DEBUG_LOG("Could not re-subscribe to all topics!");
                        (void)platform_destroy_timer(handle, handle->keepalive_timer);
                        handle->keepalive_timer = -1;
                        (void)platform_disconnect(handle);
                    }
                }
                if (platform_create_timer(handle, KEEPALIVE_INTERVAL, &handle->keepalive_timer, _keepalive_callback) != MQTT_Error_Ok) {
                    DEBUG_LOG("Could not create keepalive timer!");
                    (void)platform_destroy_timer(handle, handle->keepalive_timer);
                    handle->keepalive_timer = -1;
                    (void)platform_disconnect(handle);
                }
            }
            break;

        case PacketTypePubAck:
        case PacketTypePubRec:
        case PacketTypePubRel:
        case PacketTypePubComp:
        case PacketTypeSubAck:
        case PacketTypeUnsubAck:
            if (!dispatch_packet(handle, packet)) {
                DEBUG_LOG("Unexpected packet! (type: %s, packet_id: %d)", get_packet_name(packet), get_packet_id(packet));
                (void)platform_destroy_timer(handle, handle->keepalive_timer);
                handle->keepalive_timer = -1;
                (void)platform_disconnect(handle);
            }
            break;

        case PacketTypePublish: {
            // DEBUG_LOG("Publish on %s -> %s", ((PublishPayload *)packet->payload)->topic, ((PublishPayload *)packet->payload)->message);
            PublishPayload *payload = (PublishPayload *)packet->payload;
            switch (payload->qos) {
                case MQTT_QOS_0:
                    dispatch_subscription(handle, payload);
                    break;
                case MQTT_QOS_1:
                    if (send_puback_packet(handle, payload->packet_id)) {
                        dispatch_subscription(handle, payload);
                    }
                    break;
                case MQTT_QOS_2:
                    send_pubrec_packet(handle, payload->packet_id, dispatch_subscription_direct, payload);
                    break;
                default:
                    DEBUG_LOG("Invalid QoS! (packet_id: %d)", payload->packet_id);
                    (void)platform_destroy_timer(handle, handle->keepalive_timer);
                    handle->keepalive_timer = -1;
                    (void)platform_disconnect(handle);
                    break;
            }
            break;
        }

        // just for keepalive, do not handle
        case PacketTypePingResp:
            break;

        // client -> server, will not be handled in client
        case PacketTypeConnect:
        case PacketTypeSubscribe:
        case PacketTypeUnsubscribe:
        case PacketTypePingReq:
        case PacketTypeDisconnect:
            DEBUG_LOG("Server packet on client connection? What's up with the broker?");
            (void)platform_destroy_timer(handle, handle->keepalive_timer);
            handle->keepalive_timer = -1;
            (void)platform_disconnect(handle);
            break;
    }
}

/*
 * Reading loop
 */

PlatformTaskFunc(_reader) {
    MQTTHandle *handle = (MQTTHandle *)context;
    Buffer *buffer = buffer_allocate(max_receive_buffer_size);

    handle->reader_alive = true;

    while (1) {
        MQTTErrorCode ret = platform_read(handle, buffer);
        if (ret != MQTT_Error_Ok) {
            handle->reader_alive = false;
            buffer_release(buffer);
            #if PLATFORM_TASK_MAY_EXIT
                return 0;
            #else
                platform_suspend_self();
            #endif
        }

        while (1) {
            buffer->len = buffer->position;
            buffer->position = 0;

            MQTTPacket *packet = mqtt_packet_decode(buffer);
            if (packet == NULL) {
                // invalid packet
                if (buffer_free_space(buffer) > 0) {
                    // half packet, fetch more
                    buffer->position = buffer->len;
                    buffer->len = max_receive_buffer_size;
                    break;
                } else {
                    // no space in buffer, bail and reconnect
                    DEBUG_LOG("Buffer overflow!");
                    (void)platform_destroy_timer(handle, handle->keepalive_timer);
                    handle->keepalive_timer = -1;
                    (void)platform_disconnect(handle);
                    handle->reader_alive = false;
                    buffer_release(buffer);
                    #if PLATFORM_TASK_MAY_EXIT
                        return 0;
                    #else
                        platform_suspend_self();
                    #endif
                }
            } else {
                hexdump(buffer->data, num_bytes, 2);
                parse_packet(handle, packet);
                free_MQTTPacket(packet);

                if (!buffer_eof(buffer)) {
                    buffer->len = max_receive_buffer_size;

                    // Not complete recv buffer was consumed, so we have more than one packet in there
                    size_t remaining = max_receive_buffer_size - buffer->position;
                    memmove(buffer->data, buffer->data + buffer->position, remaining);
                    buffer->position = 0;
                } else {
                    // buffer consumed completely, read another chunk
                    buffer->position = 0;
                    buffer->len = max_receive_buffer_size;
                    break;
                }
            }
        }
    }
}

static MQTTErrorCode _mqtt_connect(MQTTHandle *handle, MQTTEventHandler callback, void *context) {
    MQTTErrorCode ret = platform_connect(handle);

    if (ret != MQTT_Error_Ok) {
        DEBUG_LOG("Could not connect");
        return ret;
    }

    expect_packet(handle, PacketTypeConnAck, 0, callback, context);

    if (!handle->reader_alive) {
        if (handle->read_task_handle >= 0) {
            platform_cleanup_task(handle, handle->read_task_handle);
            handle->read_task_handle = -1;
        }
        ret = platform_run_task(handle, &handle->read_task_handle, _reader);
        platform_sleep(50); // FIXME: Simple yield should be enough
        if (ret != MQTT_Error_Ok) {
            DEBUG_LOG("Could not start read task");
            return ret;
        }
    }

    ret = send_connect_packet(handle);
    if (ret != MQTT_Error_Ok) {
        DEBUG_LOG("Sending connect packet failed, running error handler");
        platform_disconnect(handle);
        return ret;
    }

    return MQTT_Error_Ok;
}

/*
 * API
 */

MQTTHandle *mqtt_connect(MQTTConfig *config, MQTTEventHandler callback, void *context, MQTTErrorHandler error_callback) {
    // sanity check
    if ((config->client_id != NULL) && (strlen(config->client_id) > 23)) {
        DEBUG_LOG("Client ID has to be shorter than 24 characters");
        return NULL;
    }

    MQTTHandle *handle = (MQTTHandle *)calloc(sizeof(struct _MQTTHandle), 1);
    MQTTErrorCode ret = platform_init(handle);
    if (ret != MQTT_Error_Ok) {
        free(handle);
        return NULL;
    }

    if (config->port == 0) {
        config->port = 1883;
    }

    handle->config = config;
    handle->error_handler = error_callback;
    handle->read_task_handle = -1;
    handle->keepalive_timer = -1;

    DEBUG_DUMP_HANDLE(handle);
    _mqtt_connect(handle, callback, context);

    return handle;
}


MQTTErrorCode mqtt_reconnect(MQTTHandle *handle, MQTTEventHandler callback, void *context) {
    if (handle->reader_alive) {
        (void)platform_destroy_timer(handle, handle->keepalive_timer);
        handle->keepalive_timer = -1;
        (void)platform_disconnect(handle);
        // DEBUG_LOG("Waiting for reader to exit");
        #if PLATFORM_TASK_MAY_EXIT == 0
            platform_cleanup_task(handle, handle->read_task_handle);
        #endif
    }

    handle->config->clean_session = false;
    handle->reconnecting = true;
    return _mqtt_connect(handle, callback, context);
}

MQTTErrorCode mqtt_subscribe(MQTTHandle *handle, char *topic, MQTTQosLevel qos_level, MQTTPublishEventHandler callback) {
    if (!handle->reader_alive) {
        return MQTT_Error_Connection_Reset;
    }
    add_subscription(handle, topic, qos_level, callback);
    return (send_subscribe_packet(handle, topic, qos_level) ? MQTT_Error_Ok : MQTT_Error_Internal);
}

MQTTErrorCode mqtt_unsubscribe(MQTTHandle *handle, char *topic) {
    if (!handle->reader_alive) {
        return MQTT_Error_Connection_Reset;
    }
    remove_subscription(handle, topic);
    return (send_unsubscribe_packet(handle, topic) ? MQTT_Error_Ok : MQTT_Error_Internal);
}

MQTTErrorCode mqtt_publish(MQTTHandle *handle, char *topic, char *payload, MQTTQosLevel qos_level, MQTTPublishEventHandler callback) {
    if (!handle->reader_alive) {
        return MQTT_Error_Connection_Reset;
    }
    return (send_publish_packet(handle, topic, payload, qos_level, callback) ? MQTT_Error_Ok : MQTT_Error_Internal);
}

MQTTErrorCode mqtt_disconnect(MQTTHandle *handle, MQTTEventHandler callback, void *callback_context) {
    (void)send_disconnect_packet(handle);
    (void)platform_destroy_timer(handle, handle->keepalive_timer);
    handle->keepalive_timer = -1;
    (void)platform_disconnect(handle);
    (void)platform_cleanup_task(handle, handle->read_task_handle);
    handle->read_task_handle = -1;
    mqtt_free(handle);

    if (callback) {
        callback(NULL, callback_context);
    }
    return MQTT_Error_Ok;
}
