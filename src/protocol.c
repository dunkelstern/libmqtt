#include <unistd.h>
#include <stdio.h>

#include "mqtt_internal.h"
#include "packet.h"
#include "buffer.h"

#include "debug.h"

static bool send_buffer(MQTTHandle *handle, Buffer *buffer) {
    DEBUG_LOG("Attempting to send:");
    buffer_hexdump(buffer, 2);

    while (buffer_free_space(buffer) > 0) {
        ssize_t bytes = write(handle->sock, buffer->data + buffer->position, buffer_free_space(buffer));
        if (bytes <= 0) {
            DEBUG_LOG("write error, %s", strerror(errno));
            buffer_release(buffer);
            return false;
        }
        DEBUG_LOG("Wrote %zu bytes...", bytes);
        buffer->position += bytes;
    }
    DEBUG_LOG("Buffer written...");
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

    // TODO: add waiting ConnAck to queue

    encoded->position = 0;
    return send_buffer(handle, encoded);
}

bool send_subscribe_packet(MQTTHandle *handle, char *topic, MQTTQosLevel qos) {
    SubscribePayload *payload = calloc(1, sizeof(SubscribePayload));

    payload->packet_id = handle->packet_id_counter++;
    payload->topic = topic;
    payload->qos = qos;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeSubscribe, payload });
    free(payload);

    // TODO: add waiting SubAck to queue

    encoded->position = 0;
    return send_buffer(handle, encoded);
}

bool send_unsubscribe_packet(MQTTHandle *handle, char *topic) {
    UnsubscribePayload *payload = calloc(1, sizeof(UnsubscribePayload));

    payload->packet_id = 10;
    payload->topic = "test/topic";

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypeUnsubscribe, payload });
    free(payload);

    // TODO: add waiting UnsubAck to queue
    encoded->position = 0;
    return send_buffer(handle, encoded);
}

bool send_publish_packet(MQTTHandle *handle, char *topic, char *message, MQTTQosLevel qos) {
    PublishPayload *payload = calloc(1, sizeof(PublishPayload));

    payload->qos = qos;
    payload->retain = true;
    payload->topic = topic;
    payload->packet_id = handle->packet_id_counter++;
    payload->message = message;

    Buffer *encoded = mqtt_packet_encode(&(MQTTPacket){ PacketTypePublish, payload });
    free(payload);

    // TODO: Handle QoS and add waiting packets to queue

    encoded->position = 0;
    return send_buffer(handle, encoded);
}
