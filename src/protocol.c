#include <unistd.h>
#include <stdio.h>

#include "mqtt_internal.h"
#include "packet.h"
#include "buffer.h"

#include "debug.h"

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

bool send_connect_packet(MQTTHandle *handle) {
    ConnectPayload *payload = calloc(1, sizeof(ConnectPayload));

    payload->client_id = handle->config->client_id;
    payload->protocol_level = 4;
    payload->keepalive_interval = 60;

    // TODO: support last will
    // payload->will_topic = "test/lastwill";
    // payload->will_message = "disconnected";
    // payload->will_qos = MQTT_QOS_1;
    // payload->retain_will = true;

    payload->username = handle->config->username;
    payload->password = handle->config->password;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeConnect, payload });
    free(payload);

    // ConnAck waiting packet added to queue from _mqtt_connect

    encoded->position = 0;
    return send_buffer(handle, encoded);
}

void remove_pending(MQTTHandle *handle, void *context) {
    SubscribePayload *payload = (SubscribePayload *)context;

    subscription_set_pending(handle, payload->topic, false);

    free(payload->topic);
    free(payload);
}

bool send_subscribe_packet(MQTTHandle *handle, char *topic, MQTTQosLevel qos) {
    SubscribePayload *payload = calloc(1, sizeof(SubscribePayload));

    payload->packet_id = handle->packet_id_counter++;
    payload->topic = strdup(topic);
    payload->qos = qos;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeSubscribe, payload });

    // add waiting for SubAck to queue
    expect_packet(handle, PacketTypeSubAck, payload->packet_id, remove_pending, payload);

    encoded->position = 0;
    return send_buffer(handle, encoded);
}

bool send_unsubscribe_packet(MQTTHandle *handle, char *topic) {
    UnsubscribePayload *payload = calloc(1, sizeof(UnsubscribePayload));

    payload->packet_id = 10;
    payload->topic = "test/topic";

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeUnsubscribe, payload });

    // add waiting for UnsubAck to queue
    expect_packet(handle, PacketTypeUnsubAck, payload->packet_id, NULL, NULL);
    free(payload);

    encoded->position = 0;
    return send_buffer(handle, encoded);
}

void handle_pubrec(MQTTHandle *handle, void *context) {
    PublishPayload *payload = (PublishPayload *)context;

    PubRelPayload newPayload = {
        .packet_id = payload->packet_id
    };

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypePubRel, &newPayload });

    expect_packet(handle, PacketTypePubComp, payload->packet_id, NULL, NULL);
    free(payload);

    encoded->position = 0;
    send_buffer(handle, encoded);
}

bool send_publish_packet(MQTTHandle *handle, char *topic, char *message, MQTTQosLevel qos) {
    PublishPayload *payload = calloc(1, sizeof(PublishPayload));

    payload->qos = qos;
    payload->retain = true;
    payload->topic = topic;
    payload->packet_id = handle->packet_id_counter++;
    payload->message = message;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypePublish, payload });

    // Handle QoS and add waiting packets to queue
    switch(payload->qos) {
        case MQTT_QOS_0:
            // fire and forget
            free(payload);
            break;
        case MQTT_QOS_1:
            expect_packet(handle, PacketTypePubAck, payload->packet_id, NULL, NULL);
            free(payload);
            break;
        case MQTT_QOS_2:
            expect_packet(handle, PacketTypePubRec, payload->packet_id, handle_pubrec, payload);
            break;
    }

    encoded->position = 0;
    return send_buffer(handle, encoded);
}

bool send_disconnect_packet(MQTTHandle *handle) {
    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeDisconnect, NULL });
    encoded->position = 0;
    return send_buffer(handle, encoded);
}
