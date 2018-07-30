#include <assert.h>
#include "debug.h"
#include "packet.h"

/*
 * Helper functionality
 */

MQTTPacket *allocate_MQTTPacket(MQTTControlPacketType type) {
    MQTTPacket *packet = calloc(1, sizeof(MQTTPacket));
    packet->packet_type = type;

    switch (type) {
        case PacketTypeConnect:
            packet->payload = calloc(1, sizeof(ConnectPayload));
            break;
        case PacketTypeConnAck:
            packet->payload = calloc(1, sizeof(ConnAckPayload));
            break;
        case PacketTypePublish:
            packet->payload = calloc(1, sizeof(PublishPayload));
            break;
        case PacketTypePubAck:
            packet->payload = calloc(1, sizeof(PubAckPayload));
            break;
        case PacketTypePubRec:
            packet->payload = calloc(1, sizeof(PubRecPayload));
            break;
        case PacketTypePubRel:
            packet->payload = calloc(1, sizeof(PubRelPayload));
            break;
        case PacketTypePubComp:
            packet->payload = calloc(1, sizeof(PubCompPayload));
            break;
        case PacketTypeSubscribe:
            packet->payload = calloc(1, sizeof(SubscribePayload));
            break;
        case PacketTypeSubAck:
            packet->payload = calloc(1, sizeof(SubAckPayload));
            break;
        case PacketTypeUnsubscribe:
            packet->payload = calloc(1, sizeof(UnsubscribePayload));
            break;
        case PacketTypeUnsubAck:
            packet->payload = calloc(1, sizeof(UnsubAckPayload));
            break;
        case PacketTypePingReq:
        case PacketTypePingResp:
        case PacketTypeDisconnect:
            packet->payload = NULL;
            break;
    }

    return packet;
}

void free_MQTTPacket(MQTTPacket *packet) {
    free(packet->payload);
    packet->payload = NULL;
    free(packet);
}


uint16_t variable_length_int_decode(Buffer *buffer) {
    uint16_t result = buffer->data[buffer->position++] & 0x7f;
    uint16_t shift = 7;
    while (buffer->data[buffer->position - 1] & 0x80) {
        result += (buffer->data[buffer->position] & 0x7f) << shift;
        shift += 7;
        if (buffer_eof(buffer)) {
            break; // bail out, buffer exhausted
        }
        buffer->position++;
    }

    return result;
}

char *utf8_string_decode(Buffer *buffer) {
    char *result;

    if (buffer_free_space(buffer) < 2) {
        return NULL; // buffer too small
    }
    uint16_t sz = (buffer->data[buffer->position] << 8) + buffer->data[buffer->position + 1];
    if (buffer_free_space(buffer) < sz + 2) {
        return NULL; // incomplete buffer
    }
    buffer->position += 2;

    result = malloc(sz + 1);
    buffer_copy_out(buffer, result, sz);
    result[sz] = '\0';
    return result;
}

size_t variable_length_int_encode(uint16_t value, Buffer *buffer) {
    if (value == 0) {
        buffer->data[buffer->position] = 0;
        buffer->position++;
        return 1;
    }

    size_t len = 0;
    while (value > 0) {
        if (buffer->position + len > buffer->len) {
            buffer->position += len - 1;
            return len - 1; // bail out, buffer exhausted
        }
        buffer->data[buffer->position + len] = value % 128;
        value = value / 128;
        if (value > 0){
            buffer->data[buffer->position + len] |= 0x80;
        }
        len++;
    }
    buffer->position += len;
    return len;
}

size_t variable_length_int_size(uint16_t value) {
    if (value == 0) {
        return 1;
    }

    size_t len = 0;
    while (value > 0) {
        value = value / 128;
        len++;
    }
    return len;
}

size_t utf8_string_encode(char *string, Buffer *buffer) {
    size_t len = 0;

    if (string != NULL) {
        len = strlen(string);
    }

    if ((len > UINT16_MAX) || (buffer_free_space(buffer) < len + 2)) {
        return 0; // bail out, buffer too small
    }

    buffer->data[buffer->position] = (len & 0xff00) >> 8;
    buffer->data[buffer->position + 1] = (len & 0xff);
    buffer->position += 2;

    if (string != NULL) {
        (void)buffer_copy_in(string, buffer, len);
    }

    return len + 2;
}

/*
 * Decoder
 */

bool decode_connect(Buffer *buffer, ConnectPayload *payload) {
    // Validate this is actually a connect packet
    char template[] = { 0x00, 0x04, 'M', 'Q', 'T', 'T' };
    if (memcmp(buffer->data + buffer->position, template, sizeof(template)) != 0) {
        return false;
    }
    buffer->position += sizeof(template);

    payload->protocol_level = buffer->data[buffer->position++];
    uint8_t flags = buffer->data[buffer->position++];
    payload->clean_session = ((flags & 0x02) > 0);
    payload->keepalive_interval = 
        (buffer->data[buffer->position] << 8) 
        + buffer->data[buffer->position + 1];
    buffer->position += 2;
    payload->client_id = utf8_string_decode(buffer);
    
    // last will
    if (flags & 0x04) {
        payload->will_topic = utf8_string_decode(buffer);
        payload->will_message = utf8_string_decode(buffer);
    }
    payload->will_qos = (flags & 0x18) >> 3;
    payload->retain_will = (flags & 0x20) > 0;
    
    // username
    if (flags & 0x40) {
        payload->username = utf8_string_decode(buffer);
    }

    // password
    if (flags & 0x80) {
        payload->password = utf8_string_decode(buffer);
    }

    return true;
}

bool decode_connack(Buffer *buffer, ConnAckPayload *payload) {
    payload->session_present = buffer->data[buffer->position++] & 0x01;
    payload->status = buffer->data[buffer->position++];

    return true;
}

bool decode_publish(Buffer *buffer, PublishPayload *payload, size_t sz) {
    uint8_t flags = buffer->data[buffer->position - 2] & 0x0f;
    uint16_t start_pos = buffer->position;

    payload->qos = (flags & 0x06) >> 1;
    payload->retain = ((flags & 0x01) > 0);
    payload->duplicate = ((flags & 0x08) > 0);

    payload->topic = utf8_string_decode(buffer);
    payload->packet_id =
        (buffer->data[buffer->position] << 8) 
        + buffer->data[buffer->position + 1];
    buffer->position += 2;

    size_t len = sz - (buffer->position - start_pos) + 1;
    if (len > 1) {
        payload->message = malloc(len);
        memcpy(payload->message, buffer->data + buffer->position, len - 1);
        payload->message[len] = '\0';
        buffer->position += len;
    }

    return true;
}

bool decode_puback(Buffer *buffer, PubAckPayload *payload) {
    payload->packet_id = 
        (buffer->data[buffer->position] << 8) 
        + buffer->data[buffer->position + 1];
    buffer->position += 2;
    return true;
}

bool decode_pubrec(Buffer *buffer, PubRecPayload *payload) {
    payload->packet_id = 
        (buffer->data[buffer->position] << 8) 
        + buffer->data[buffer->position + 1];
    buffer->position += 2;

    return true;    
}

bool decode_pubrel(Buffer *buffer, PubRelPayload *payload) {
    payload->packet_id = 
        (buffer->data[buffer->position] << 8) 
        + buffer->data[buffer->position + 1];
    buffer->position += 2;

    return true;
}

bool decode_pubcomp(Buffer *buffer, PubCompPayload *payload) {
    payload->packet_id = 
        (buffer->data[buffer->position] << 8) 
        + buffer->data[buffer->position + 1];
    buffer->position += 2;

    return true;
}

bool decode_subscribe(Buffer *buffer, SubscribePayload *payload) {
    payload->packet_id = 
        (buffer->data[buffer->position] << 8) 
        + buffer->data[buffer->position + 1];
    buffer->position += 2;

    payload->topic = utf8_string_decode(buffer);
    payload->qos = buffer->data[buffer->position++] & 0x03;

    return true;
}

bool decode_suback(Buffer *buffer, SubAckPayload *payload) {
    payload->packet_id = 
        (buffer->data[buffer->position] << 8) 
        + buffer->data[buffer->position + 1];
    buffer->position += 2;

    payload->status = buffer->data[buffer->position++];

    return true;
}

bool decode_unsubscribe(Buffer *buffer, UnsubscribePayload *payload) {
    payload->packet_id = 
        (buffer->data[buffer->position] << 8) 
        + buffer->data[buffer->position + 1];
    buffer->position += 2;

    payload->topic = utf8_string_decode(buffer);

    return true;
}

bool decode_unsuback(Buffer *buffer, UnsubAckPayload *payload) {
    payload->packet_id = 
        (buffer->data[buffer->position] << 8) 
        + buffer->data[buffer->position + 1];
    buffer->position += 2;

    return true;
}


MQTTPacket *mqtt_packet_decode(Buffer *buffer) {
    // validate that the buffer is big enough
    MQTTControlPacketType type = (buffer->data[buffer->position] & 0xf0) >> 4;
    buffer->position++;
    size_t packet_size = variable_length_int_decode(buffer);

    if (buffer_free_space(buffer) < packet_size) {
        return NULL; // buffer incomplete
    }
    MQTTPacket *result = allocate_MQTTPacket(type);

    bool valid = false;
    switch (type) {
        case PacketTypeConnect:
            valid = decode_connect(buffer, result->payload);
            break;
        case PacketTypeConnAck:
            valid = decode_connack(buffer, result->payload);
            break;
        case PacketTypePublish:
            valid = decode_publish(buffer, result->payload, packet_size);
            break;
        case PacketTypePubAck:
            valid = decode_puback(buffer, result->payload);
            break;
        case PacketTypePubRec:
            valid = decode_pubrec(buffer, result->payload);
            break;
        case PacketTypePubRel:
            valid = decode_pubrel(buffer, result->payload);
            break;
        case PacketTypePubComp:
            valid = decode_pubcomp(buffer, result->payload);
            break;
        case PacketTypeSubscribe:
            valid = decode_subscribe(buffer, result->payload);
            break;
        case PacketTypeSubAck:
            valid = decode_suback(buffer, result->payload);
            break;
        case PacketTypeUnsubscribe:
            valid = decode_unsubscribe(buffer, result->payload);
            break;
        case PacketTypeUnsubAck:
            valid = decode_unsuback(buffer, result->payload);
            break;
        case PacketTypePingReq:
        case PacketTypePingResp:
        case PacketTypeDisconnect:
            valid = true; // there is no payload
            break;
    }

    if (!valid) {
        free_MQTTPacket(result);
        return NULL;
    }
    return result;
}

/*
 * Encoder
 */

Buffer *make_buffer_for_header(size_t sz, MQTTControlPacketType type) {
    sz += variable_length_int_size(sz); // size field
    sz += 1; // packet type and flags

    Buffer *buffer = buffer_allocate(sz);
    buffer->data[0] = type << 4;

    // MQTT Spec means we should set a bit in the flags field for some packet types
    switch (type) {
        case PacketTypePubRel:
        case PacketTypeSubscribe:
        case PacketTypeUnsubscribe:
            buffer->data[0] |= 0x02;
            break;
        default:
            break;
    }
    buffer->position += 1;
    variable_length_int_encode(sz - 2, buffer);
    
    return buffer;
}

Buffer *encode_connect(ConnectPayload *payload) {
    size_t sz = 10 /* variable header */;
    sz += strlen(payload->client_id) + 2;
    if (payload->will_topic) {
        sz += strlen(payload->will_topic) + 2;
        sz += strlen(payload->will_message) + 2;
    }
    if (payload->username) {
        sz += strlen(payload->username) + 2;
    }
    if (payload->password) {
        sz += strlen(payload->password) + 2;
    }

    Buffer *buffer = make_buffer_for_header(sz, PacketTypeConnect);

    // variable header
    utf8_string_encode("MQTT", buffer);
    char *p = buffer->data + buffer->position;

    *(p++) = payload->protocol_level;

    uint8_t flags = (
          ((payload->username) ? (1 << 7) : 0)
        + ((payload->password) ? (1 << 6) : 0)
        + ((payload->retain_will) ? (1 << 5) : 0)
        + ((payload->will_topic) ? (payload->will_qos << 3) : 0)
        + ((payload->will_topic) ? (1 << 2) : 0)
        + ((payload->clean_session) ? (1 << 1) : 0)
    );
    *(p++) = flags;
    *(p++) = (payload->keepalive_interval & 0xff00) >> 8;
    *(p++) = (payload->keepalive_interval & 0xff);

    buffer->position += 4;

    // payload
    utf8_string_encode(payload->client_id, buffer);
    if (payload->will_topic) {
        utf8_string_encode(payload->will_topic, buffer);
        utf8_string_encode(payload->will_message, buffer);
    }
    if (payload->username) {
        utf8_string_encode(payload->username, buffer);
    }
    if (payload->password) {
        utf8_string_encode(payload->password, buffer);
    }

    assert(buffer_eof(buffer));
    return buffer;
}

Buffer *encode_connack(ConnAckPayload *payload) {
    size_t sz = 2; // session flag and status

    Buffer *buffer = make_buffer_for_header(sz, PacketTypeConnAck);
    buffer->data[buffer->position++] = payload->session_present;
    buffer->data[buffer->position++] = payload->status;

    assert(buffer_eof(buffer));
    return buffer; 
}

Buffer *encode_publish(PublishPayload *payload) {
    size_t sz = 0;
    sz += strlen(payload->topic) + 2; // topic
    sz += 2; // packet id
    if (payload->message) {
        sz += strlen(payload->message);
    }

    Buffer *buffer = make_buffer_for_header(sz, PacketTypePublish);

    // Flags in header
    if (payload->retain) {
        buffer->data[buffer->position - 2] |= 1;
    }
    buffer->data[buffer->position - 2] |= (payload->qos << 1);
    if (payload->duplicate) {
        buffer->data[buffer->position - 2] |= 8;
    }

    // Variable header
    utf8_string_encode(payload->topic, buffer);
    buffer->data[buffer->position++] = (payload->packet_id & 0xff00) >> 8;
    buffer->data[buffer->position++] = (payload->packet_id & 0xff);

    // Payload
    if (payload->message) {
        buffer_copy_in(payload->message, buffer, strlen(payload->message));
    }

    assert(buffer_eof(buffer));
    return buffer; 
}

Buffer *encode_puback(PubAckPayload *payload) {
    size_t sz = 2; // packet id

    Buffer *buffer = make_buffer_for_header(sz, PacketTypePubAck);

    // Variable header
    buffer->data[buffer->position++] = (payload->packet_id & 0xff00) >> 8;
    buffer->data[buffer->position++] = (payload->packet_id & 0xff);

    assert(buffer_eof(buffer));
    return buffer; 
}

Buffer *encode_pubrec(PubRecPayload *payload) {
    size_t sz = 2; // packet id

    Buffer *buffer = make_buffer_for_header(sz, PacketTypePubRec);

    // Variable header
    buffer->data[buffer->position++] = (payload->packet_id & 0xff00) >> 8;
    buffer->data[buffer->position++] = (payload->packet_id & 0xff);

    assert(buffer_eof(buffer));
    return buffer; 
}

Buffer *encode_pubrel(PubRelPayload *payload) {
    size_t sz = 2; // packet id

    Buffer *buffer = make_buffer_for_header(sz, PacketTypePubRel);

    // Variable header
    buffer->data[buffer->position++] = (payload->packet_id & 0xff00) >> 8;
    buffer->data[buffer->position++] = (payload->packet_id & 0xff);

    assert(buffer_eof(buffer));
    return buffer; 
}

Buffer *encode_pubcomp(PubCompPayload *payload) {
    size_t sz = 2; // packet id

    Buffer *buffer = make_buffer_for_header(sz, PacketTypePubComp);

    // Variable header
    buffer->data[buffer->position++] = (payload->packet_id & 0xff00) >> 8;
    buffer->data[buffer->position++] = (payload->packet_id & 0xff);

    assert(buffer_eof(buffer));
    return buffer; 
}

Buffer *encode_subscribe(SubscribePayload *payload) {
    size_t sz = 2; // packet id
    sz += strlen(payload->topic) + 2; // topic
    sz += 1; // qos level

    Buffer *buffer = make_buffer_for_header(sz, PacketTypeSubscribe);

    // Variable header
    buffer->data[buffer->position++] = (payload->packet_id & 0xff00) >> 8;
    buffer->data[buffer->position++] = (payload->packet_id & 0xff);

    // Payload
    utf8_string_encode(payload->topic, buffer);
    buffer->data[buffer->position++] = payload->qos;

    assert(buffer_eof(buffer));
    return buffer; 
}

Buffer *encode_suback(SubAckPayload *payload) {
    size_t sz = 2; // packet id
    sz += 1; // Status code

    Buffer *buffer = make_buffer_for_header(sz, PacketTypeSubAck);

    // Variable header
    buffer->data[buffer->position++] = (payload->packet_id & 0xff00) >> 8;
    buffer->data[buffer->position++] = (payload->packet_id & 0xff);

    // Payload
    buffer->data[buffer->position++] = payload->status;

    assert(buffer_eof(buffer));
    return buffer; 
}

Buffer *encode_unsubscribe(UnsubscribePayload *payload) {
    size_t sz = 2; // packet id
    sz += strlen(payload->topic) + 2; // topic

    Buffer *buffer = make_buffer_for_header(sz, PacketTypeUnsubscribe);

    // Variable header
    buffer->data[buffer->position++] = (payload->packet_id & 0xff00) >> 8;
    buffer->data[buffer->position++] = (payload->packet_id & 0xff);

    // Payload
    utf8_string_encode(payload->topic, buffer);

    assert(buffer_eof(buffer));
    return buffer;
}

Buffer *encode_unsuback(UnsubAckPayload *payload) {
    size_t sz = 2; // packet id

    Buffer *buffer = make_buffer_for_header(sz, PacketTypeUnsubAck);

    // Variable header
    buffer->data[buffer->position++] = (payload->packet_id & 0xff00) >> 8;
    buffer->data[buffer->position++] = (payload->packet_id & 0xff);

    assert(buffer_eof(buffer));
    return buffer; 
}

Buffer *encode_pingreq() {
    Buffer *buffer = make_buffer_for_header(0, PacketTypePingReq);
    assert(buffer_eof(buffer));
    return buffer; 
}

Buffer *encode_pingresp() {
    Buffer *buffer = make_buffer_for_header(0, PacketTypePingResp);
    assert(buffer_eof(buffer));
    return buffer; 
}

Buffer *encode_disconnect() {
    Buffer *buffer = make_buffer_for_header(0, PacketTypeDisconnect);
    assert(buffer_eof(buffer));
    return buffer; 
}

Buffer *mqtt_packet_encode(MQTTPacket *packet) {
    switch (packet->packet_type) {
        case PacketTypeConnect:
            return encode_connect((ConnectPayload *)packet->payload);
        case PacketTypeConnAck:
            return encode_connack((ConnAckPayload *)packet->payload);
        case PacketTypePublish:
            return encode_publish((PublishPayload *)packet->payload);
        case PacketTypePubAck:
            return encode_puback((PubAckPayload *)packet->payload);
        case PacketTypePubRec:
            return encode_pubrec((PubRecPayload *)packet->payload);
        case PacketTypePubRel:
            return encode_pubrel((PubRelPayload *)packet->payload);
        case PacketTypePubComp:
            return encode_pubcomp((PubCompPayload *)packet->payload);
        case PacketTypeSubscribe:
            return encode_subscribe((SubscribePayload *)packet->payload);
        case PacketTypeSubAck:
            return encode_suback((SubAckPayload *)packet->payload);
        case PacketTypeUnsubscribe:
            return encode_unsubscribe((UnsubscribePayload *)packet->payload);
        case PacketTypeUnsubAck:
            return encode_unsuback((UnsubAckPayload *)packet->payload);
        case PacketTypePingReq:
            return encode_pingreq();
        case PacketTypePingResp:
            return encode_pingresp();
        case PacketTypeDisconnect:
            return encode_disconnect();
    }

    return NULL;
}
