#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include "mqtt.h"
#include "mqtt_internal.h"
#include "packet.h"
#include "platform.h"
#include "protocol.h"
#include "debug.h"

void mqtt_free(MQTTHandle *handle) {
    platform_release(handle);
    free(handle);
}

static void _keepalive_callback(MQTTHandle *handle, int timer_handle) {
    bool result = send_ping_packet(handle);
    if (!result) {
        DEBUG_LOG("Sending PINGREQ packet failed!");
    }
}

static inline void parse_packet(MQTTHandle *handle, MQTTPacket *packet) {
    switch (packet->packet_type) {
        case PacketTypeConnAck:
            if (!dispatch_packet(handle, packet)) {
                DEBUG_LOG("Unexpected packet! (type: CONNACK)");
                (void)platform_disconnect(handle);
            } else {
                if (platform_create_timer(handle, KEEPALIVE_INTERVAL, &handle->keepalive_timer, _keepalive_callback) != PlatformStatusOk) {
                    DEBUG_LOG("Could not create keepalive timer!");
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
                (void)platform_disconnect(handle);
            }
            break;

        case PacketTypePublish:
            dispatch_subscription(handle, (PublishPayload *)packet->payload);
            // TODO: Handle QoS
            break;

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
            (void)platform_disconnect(handle);
            break;
    }
}

static void *_reader(MQTTHandle *handle) {
    Buffer *buffer = buffer_allocate(max_receive_buffer_size);

    handle->reader_alive = true;

    while (1) {
        PlatformStatusCode ret = platform_read(handle, buffer);
        if (ret == PlatformStatusError) {
            handle->reader_alive = false;
            return NULL;
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
                    platform_disconnect(handle);
                    handle->reader_alive = false;
                    buffer_release(buffer);
                    return NULL;
                }
            } else {
                hexdump(buffer->data, num_bytes, 2);
                parse_packet(handle, packet);
                free_MQTTPacket(packet);

                if (!buffer_eof(buffer)) {
                    buffer->position = buffer->len;
                    buffer->len = max_receive_buffer_size;

                    // Not complete recv buffer was consumed, so we have more than one packet in there
                    size_t remaining = max_receive_buffer_size - buffer->position;
                    memmove(buffer->data, buffer->data + buffer->position, remaining);
                    buffer->position = 0;
                    break;
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

static void _mqtt_connect(MQTTHandle *handle, MQTTEventHandler callback, void *context) {
    PlatformStatusCode ret = platform_connect(handle);

    if (ret == PlatformStatusError) {
        DEBUG_LOG("Could not connect");
        return;
    }

    ret = platform_run_task(handle, &handle->read_task_handle, _reader);
    if (ret == PlatformStatusError) {
        DEBUG_LOG("Could not start read task");
        return;
    }

    expect_packet(handle, PacketTypeConnAck, 0, callback, context);

    bool result = send_connect_packet(handle);
    if (result == false) {
        DEBUG_LOG("Sending connect packet failed, running error handler");
        bool free_handle = handle->error_handler(handle, handle->config, MQTT_Error_Broker_Disconnected);
        platform_disconnect(handle);
        if (free_handle) {
            platform_cleanup_task(handle, handle->read_task_handle);
            mqtt_free(handle);
        }
    }
}

MQTTHandle *mqtt_connect(MQTTConfig *config, MQTTEventHandler callback, void *context, MQTTErrorHandler error_callback) {
    // sanity check
    if ((config->client_id != NULL) && (strlen(config->client_id) > 23)) {
        DEBUG_LOG("Client ID has to be shorter than 24 characters");
        return NULL;
    }

    MQTTHandle *handle = (MQTTHandle *)calloc(sizeof(struct _MQTTHandle), 1);
    PlatformStatusCode ret = platform_init(handle);
    if (ret == PlatformStatusError) {
        free(handle);
        return NULL;
    }

    if (config->port == 0) {
        config->port = 1883;
    }

    handle->config = config;
    handle->error_handler = error_callback;

    _mqtt_connect(handle, callback, context);

    return handle;
}


MQTTStatus mqtt_reconnect(MQTTHandle *handle, MQTTEventHandler callback, void *context) {
    if (handle->reader_alive) {
        DEBUG_LOG("Closing old connection");
        platform_disconnect(handle);
        platform_cleanup_task(handle, handle->read_task_handle);
    }

    // TODO: re-submit unacknowledged packages with QoS > 0
    // TODO: clear waiting packets
    // TODO: re-subscribe all subscriptions

    _mqtt_connect(handle, callback, context);

    return MQTT_STATUS_OK;
}

MQTTStatus mqtt_subscribe(MQTTHandle *handle, char *topic, MQTTQosLevel qos_level, MQTTPublishEventHandler callback) {
    if (!handle->reader_alive) {
        handle->error_handler(handle, handle->config, MQTT_Error_Connection_Reset);
        return MQTT_STATUS_ERROR;
    }
    add_subscription(handle, topic, qos_level, callback);
    return (send_subscribe_packet(handle, topic, qos_level) ? MQTT_STATUS_OK : MQTT_STATUS_ERROR);
}

MQTTStatus mqtt_unsubscribe(MQTTHandle *handle, char *topic) {
    if (!handle->reader_alive) {
        handle->error_handler(handle, handle->config, MQTT_Error_Connection_Reset);
        return MQTT_STATUS_ERROR;
    }
    remove_subscription(handle, topic);
    return (send_unsubscribe_packet(handle, topic) ? MQTT_STATUS_OK : MQTT_STATUS_ERROR);
}

MQTTStatus mqtt_publish(MQTTHandle *handle, char *topic, char *payload, MQTTQosLevel qos_level, MQTTPublishEventHandler callback) {
    if (!handle->reader_alive) {
        handle->error_handler(handle, handle->config, MQTT_Error_Connection_Reset);
        return MQTT_STATUS_ERROR;
    }
    return (send_publish_packet(handle, topic, payload, qos_level, callback) ? MQTT_STATUS_OK : MQTT_STATUS_ERROR);
}

MQTTStatus mqtt_disconnect(MQTTHandle *handle, MQTTEventHandler callback, void *callback_context) {
    send_disconnect_packet(handle);
    platform_disconnect(handle);
    platform_cleanup_task(handle, handle->read_task_handle);
    mqtt_free(handle);

    if (callback) {
        callback(NULL, callback_context);
    }
    return MQTT_STATUS_OK;
}
