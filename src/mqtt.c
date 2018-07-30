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

static inline void mqtt_free(MQTTHandle *handle) {
    release_platform(handle);
    free(handle);
}

static inline void disconnect(MQTTHandle *handle) {
    close(handle->sock);
    // FIXME: Do we have to do anything else?
}

static inline void parse_packet(MQTTHandle *handle, MQTTPacket *packet) {
    switch (packet->packet_type) {
        case PacketTypeConnAck:
        case PacketTypePubAck:
        case PacketTypePubRec:
        case PacketTypePubRel:
        case PacketTypePubComp:
        case PacketTypeSubAck:
        case PacketTypeUnsubAck:
            if (!dispatch_packet(handle, packet)) {
                DEBUG_LOG("Unexpected packet! (type: %s, packet_id: %d)", get_packet_name(packet), get_packet_id(packet));
                disconnect(handle);
            }
            break;

        case PacketTypePublish:
            dispatch_subscription(handle, packet->payload);
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
            disconnect(handle);
            break;
    }
}

static void _reader(MQTTHandle *handle) {
    ssize_t num_bytes;
    char *read_buffer = malloc(max_receive_buffer_size);
    uint8_t offset = 0;

    handle->reader_alive = true;

    while (1) {
        num_bytes = read(handle->sock, &read_buffer[offset], max_receive_buffer_size - offset);
        if (num_bytes == 0) {
            /* Socket closed, coordinated shutdown */
            DEBUG_LOG("Socket closed");
            handle->reader_alive = false;
            return;
        } else if (num_bytes < 0) {
            if ((errno == EINTR) || (errno == EAGAIN)) {
                continue;
            }

            /* Set reader task to dead */
            handle->reader_alive = false;
            return;
        }

        while (1) {
            Buffer *buffer = buffer_from_data_no_copy(read_buffer, num_bytes);
            MQTTPacket *packet = mqtt_packet_decode(buffer);
            if (packet == NULL) {
                // invalid packet
                if (num_bytes < max_receive_buffer_size) {
                    // maybe not long enough, try to fetch the rest
                    offset += num_bytes;
                    free(buffer);
                    break;
                } else {
                    // no space in buffer, bail and reconnect
                    DEBUG_LOG("Buffer overflow!");
                    disconnect(handle);
                    handle->reader_alive = false;
                    free(buffer);
                    return;
                }
            } else {
                // hexdump(buffer->data, num_bytes, 2);

                parse_packet(handle, packet);
                free_MQTTPacket(packet);

                if (!buffer_eof(buffer)) {
                    // Not complete recv buffer was consumed, so we have more than one packet in there
                    size_t remaining = max_receive_buffer_size - buffer->position;
                    memmove(read_buffer, read_buffer + buffer->position, remaining);
                    offset -= buffer->position;
                    num_bytes -= buffer->position;
                    free(buffer);
                } else {
                    // buffer consumed completely, read another chunk
                    offset = 0;
                    free(buffer);
                    break;
                }
            }
        }
    }
}

static void _mqtt_connect(MQTTHandle *handle, MQTTEventHandler callback, void *context) {
    int ret;
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    handle->sock = socket(AF_INET, SOCK_STREAM, 0);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(handle->config->port);

    char ip[40];
    if (!hostname_to_ip(handle->config->hostname, ip)) {
        bool free_handle = handle->error_handler(handle, MQTT_Error_Host_Not_Found);
        if (free_handle) {
            mqtt_free(handle);
        }
        DEBUG_LOG("Resolving hostname failed: %s", strerror(errno));
        close(handle->sock);
        return;
    }
    ret = inet_pton(AF_INET, ip, &(servaddr.sin_addr));
    if (ret == 0) {
        bool free_handle = handle->error_handler(handle, MQTT_Error_Host_Not_Found);
        if (free_handle) {
            mqtt_free(handle);
        }
        DEBUG_LOG("Converting to servaddr failed: %s", strerror(errno));
        close(handle->sock);
        return;
    }

    ret = connect(handle->sock, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (ret != 0) {
        bool free_handle = handle->error_handler(handle, MQTT_Error_Connection_Refused);
        if (free_handle) {
            mqtt_free(handle);
        }
        DEBUG_LOG("Connection failed: %s", strerror(errno));
        close(handle->sock);
        return;
    }

    run_read_task(handle, _reader);

    expect_packet(handle, PacketTypeConnAck, 0, callback, context);

    bool result = send_connect_packet(handle);
    if (result == false) {
        DEBUG_LOG("Sending connect packet failed, running error handler");
        bool free_handle = handle->error_handler(handle, MQTT_Error_Broker_Disconnected);
        if (free_handle) {
            mqtt_free(handle);
        }
        close(handle->sock);
    }
}

MQTTHandle *mqtt_connect(MQTTConfig *config, MQTTEventHandler callback, void *context, MQTTErrorHandler error_callback) {
    // sanity check
    if ((config->client_id != NULL) && (strlen(config->client_id) > 23)) {
        DEBUG_LOG("Client ID has to be shorter than 24 characters");
        return NULL;
    }

    MQTTHandle *handle = calloc(sizeof(struct _MQTTHandle), 1);
    initialize_platform(handle);

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
        close(handle->sock);
        join_read_task(handle);
    }

    // TODO: re-submit unacknowledged packages with QoS > 0
    // TODO: clear waiting packets
    // TODO: re-subscribe all subscriptions

    _mqtt_connect(handle, callback, context);

    return MQTT_STATUS_OK;
}

MQTTStatus mqtt_subscribe(MQTTHandle *handle, char *topic, MQTTQosLevel qos_level, MQTTPublishEventHandler callback) {
    if (!handle->reader_alive) {
        handle->error_handler(handle, MQTT_Error_Connection_Reset);
        return MQTT_STATUS_ERROR;
    }
    add_subscription(handle, topic, qos_level, callback);
    return (send_subscribe_packet(handle, topic, qos_level) ? MQTT_STATUS_OK : MQTT_STATUS_ERROR);
}

MQTTStatus mqtt_unsubscribe(MQTTHandle *handle, char *topic) {
    if (!handle->reader_alive) {
        handle->error_handler(handle, MQTT_Error_Connection_Reset);
        return MQTT_STATUS_ERROR;
    }
    remove_subscription(handle, topic);
    return (send_unsubscribe_packet(handle, topic) ? MQTT_STATUS_OK : MQTT_STATUS_ERROR);
}

MQTTStatus mqtt_publish(MQTTHandle *handle, char *topic, char *payload, MQTTQosLevel qos_level) {
    if (!handle->reader_alive) {
        handle->error_handler(handle, MQTT_Error_Connection_Reset);
        return MQTT_STATUS_ERROR;
    }
    return (send_publish_packet(handle, topic, payload, qos_level) ? MQTT_STATUS_OK : MQTT_STATUS_ERROR);
}

MQTTStatus mqtt_disconnect(MQTTHandle *handle, MQTTEventHandler callback, void *callback_context) {
    send_disconnect_packet(handle);
    if (close(handle->sock)) {
        return MQTT_STATUS_ERROR;
    }
    join_read_task(handle);
    mqtt_free(handle);

    if (callback) {
        callback(NULL, callback_context);
    }
    return MQTT_STATUS_OK;
}
