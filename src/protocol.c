#include <unistd.h>
#include <stdio.h>

#include "mqtt_internal.h"
#include "packet.h"
#include "buffer.h"

#include "debug.h"

typedef struct {
    PublishPayload *payload;
    MQTTPublishEventHandler callback;
} PublishCallback;

/*
 * Utility
 */

bool send_buffer(MQTTHandle *handle, Buffer *buffer) {
    while (!buffer_eof(buffer)) {
        ssize_t bytes = write(handle->sock, buffer->data + buffer->position, buffer_free_space(buffer));
        if (bytes <= 0) {
            buffer_release(buffer);
            return false;
        }
        buffer->position += bytes;
    }
    buffer_release(buffer);
    return true;
}

/*
 * QoS event handlers
 */

void handle_puback_pubcomp(MQTTHandle *handle, void *context) {
    PublishCallback *ctx = (PublishCallback *)context;

    if (ctx->callback) {
        ctx->callback(handle, ctx->payload->topic, ctx->payload->message);
    }

    free(ctx->payload);
    free(ctx);
}

void handle_pubrec(MQTTHandle *handle, void *context) {
    PublishCallback *ctx = (PublishCallback *)context;

    PubRelPayload newPayload = {
        .packet_id = ctx->payload->packet_id
    };

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypePubRel, &newPayload });
    expect_packet(handle, PacketTypePubComp, ctx->payload->packet_id, handle_puback_pubcomp, context);

    encoded->position = 0;
    send_buffer(handle, encoded);
}

/*
 * packet constructors
 */

#if MQTT_CLIENT
bool send_connect_packet(MQTTHandle *handle) {
    ConnectPayload *payload = calloc(1, sizeof(ConnectPayload));

    payload->client_id = handle->config->client_id;
    payload->protocol_level = 4;
    payload->keepalive_interval = 60;
    payload->clean_session = handle->config->clean_session;

    payload->will_topic = handle->config->last_will_topic;
    payload->will_message = handle->config->last_will_message;
    payload->will_qos = MQTT_QOS_0;
    payload->retain_will = handle->config->last_will_retain;

    payload->username = handle->config->username;
    payload->password = handle->config->password;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeConnect, payload });
    free(payload);

    // ConnAck waiting packet added to queue from _mqtt_connect

    encoded->position = 0;
    return send_buffer(handle, encoded);
}
#endif /* MQTT_CLIENT */

void remove_pending(MQTTHandle *handle, void *context) {
    SubscribePayload *payload = (SubscribePayload *)context;

    subscription_set_pending(handle, payload->topic, false);

    free(payload->topic);
    free(payload);
}

#if MQTT_CLIENT
bool send_subscribe_packet(MQTTHandle *handle, char *topic, MQTTQosLevel qos) {
    SubscribePayload *payload = calloc(1, sizeof(SubscribePayload));

    payload->packet_id = (++handle->packet_id_counter > 0) ? handle->packet_id_counter : ++handle->packet_id_counter;
    payload->topic = strdup(topic);
    payload->qos = qos;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeSubscribe, payload });

    // add waiting for SubAck to queue
    expect_packet(handle, PacketTypeSubAck, payload->packet_id, remove_pending, payload);

    encoded->position = 0;
    return send_buffer(handle, encoded);
}
#endif /* MQTT_CLIENT */

#if MQTT_CLIENT
bool send_unsubscribe_packet(MQTTHandle *handle, char *topic) {
    UnsubscribePayload *payload = calloc(1, sizeof(UnsubscribePayload));

    payload->packet_id = (++handle->packet_id_counter > 0) ? handle->packet_id_counter : ++handle->packet_id_counter;
    payload->topic = topic;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeUnsubscribe, payload });

    // add waiting for UnsubAck to queue
    expect_packet(handle, PacketTypeUnsubAck, payload->packet_id, NULL, NULL);
    free(payload);

    encoded->position = 0;
    return send_buffer(handle, encoded);
}
#endif /* MQTT_CLIENT */

bool send_publish_packet(MQTTHandle *handle, char *topic, char *message, MQTTQosLevel qos, MQTTPublishEventHandler callback) {
    PublishPayload *payload = calloc(1, sizeof(PublishPayload));

    payload->qos = qos;
    payload->retain = true;
    payload->topic = topic;
    payload->packet_id = (++handle->packet_id_counter > 0) ? handle->packet_id_counter : ++handle->packet_id_counter;
    payload->message = message;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypePublish, payload });
    encoded->position = 0;
    bool result = send_buffer(handle, encoded);
    if (!result) {
        free(payload);
        return false;
    }

    // Handle QoS and add waiting packets to queue
    switch(payload->qos) {
        case MQTT_QOS_0:
            // fire and forget
            if (callback) {
                callback(handle, payload->topic, payload->message);
            }
            free(payload);
            break;
        case MQTT_QOS_1: {
            PublishCallback *ctx = malloc(sizeof(PublishCallback));
            ctx->payload = payload;
            ctx->callback = callback;
            expect_packet(handle, PacketTypePubAck, payload->packet_id, handle_puback_pubcomp, ctx);
            break;
        }
        case MQTT_QOS_2: {
            PublishCallback *ctx = malloc(sizeof(PublishCallback));
            ctx->payload = payload;
            ctx->callback = callback;
            expect_packet(handle, PacketTypePubRec, payload->packet_id, handle_pubrec, ctx);
            break;
        }
    }

    return true;
}

#if MQTT_CLIENT
bool send_disconnect_packet(MQTTHandle *handle) {
    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeDisconnect, NULL });
    encoded->position = 0;
    return send_buffer(handle, encoded);
}
#endif /* MQTT_CLIENT */
