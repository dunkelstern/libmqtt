#ifndef packet_h__included
#define packet_h__included

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#include "mqtt.h"
#include "buffer.h"

typedef enum {
    PacketTypeConnect      = 1,
    PacketTypeConnAck      = 2,
    PacketTypePublish      = 3,
    PacketTypePubAck       = 4,
    PacketTypePubRec       = 5,
    PacketTypePubRel       = 6,
    PacketTypePubComp      = 7,
    PacketTypeSubscribe    = 8,
    PacketTypeSubAck       = 9,
    PacketTypeUnsubscribe  = 10,
    PacketTypeUnsubAck     = 11,
    PacketTypePingReq      = 12,
    PacketTypePingResp     = 13,
    PacketTypeDisconnect   = 14
} MQTTControlPacketType;

typedef struct {
    // required:
    char *client_id;
    int protocol_level;
    uint16_t keepalive_interval;

    // optional
    char *username;
    char *password;
    char *will_topic;
    char *will_message;
    MQTTQosLevel will_qos;
    bool retain_will;
    bool clean_session;
} ConnectPayload;

typedef enum {
    ConnAckStatusAccepted             = 0, /**< Connection accepted */
    ConnAckStatusInvalidProtocolLevel = 1, /**< Protocol level not supported */
    ConnAckStatusInvalidIdentifier    = 2, /**< Client ID not accepted */
    ConnAckStatusServerUnavailable    = 3, /**< Server restarting or too many clients */
    ConnAckStatusAuthenticationError  = 4, /**< missing username/password */
    ConnAckStatusNotAuthorized        = 5  /**< not authorized */ 
} ConnAckStatus;

typedef struct {
    bool session_present;
    ConnAckStatus status;
} ConnAckPayload;

typedef struct {
    bool duplicate;
    MQTTQosLevel qos;
    bool retain;

    char *topic;
    uint16_t packet_id;

    char *message;
} PublishPayload;

typedef struct {
    uint16_t packet_id;
} PubAckPayload;

typedef struct {
    uint16_t packet_id;
} PubRecPayload;

typedef struct {
    uint16_t packet_id;
} PubRelPayload;

typedef struct {
    uint16_t packet_id;
} PubCompPayload;

typedef struct {
    uint16_t packet_id;
    char *topic;
    MQTTQosLevel qos;
} SubscribePayload;

typedef enum {
    SubAckStatusQoS0 = 0,
    SubAckStatusQoS1 = 1,
    SubAckStatusQoS2 = 2,
    SubAckFailure    = 0x80
} SubAckStatus;

typedef struct {
    uint16_t packet_id;
    SubAckStatus status;
} SubAckPayload;

typedef struct {
    uint16_t packet_id;
    char *topic;
} UnsubscribePayload;

typedef struct {
    uint16_t packet_id;
} UnsubAckPayload;

typedef struct {
    MQTTControlPacketType packet_type;
    void *payload;
} MQTTPacket;

/*
 * Decoder
 */
MQTTPacket *mqtt_packet_decode(Buffer *buffer);
void free_MQTTPacket(MQTTPacket *packet);

/*
 * Encoder
 */

Buffer *mqtt_packet_encode(MQTTPacket *packet);
MQTTPacket *allocate_MQTTPacket(MQTTControlPacketType type);


/*
 * Internal
 */

size_t variable_length_int_encode(uint16_t value, Buffer *buffer);
size_t variable_length_int_size(uint16_t value);
size_t utf8_string_encode(char *string, Buffer *buffer);

Buffer *make_buffer_for_header(size_t sz, MQTTControlPacketType type);
Buffer *encode_connect(ConnectPayload *payload);
Buffer *encode_connack(ConnAckPayload *payload);
Buffer *encode_publish(PublishPayload *payload);
Buffer *encode_puback(PubAckPayload *payload);
Buffer *encode_pubrec(PubRecPayload *payload);
Buffer *encode_pubrel(PubRelPayload *payload);
Buffer *encode_pubcomp(PubCompPayload *payload);
Buffer *encode_subscribe(SubscribePayload *payload);
Buffer *encode_suback(SubAckPayload *payload);
Buffer *encode_unsubscribe(UnsubscribePayload *payload);
Buffer *encode_unsuback(UnsubAckPayload *payload);
Buffer *encode_pingreq();
Buffer *encode_pingresp();
Buffer *encode_disconnect();


uint16_t variable_length_int_decode(Buffer *buffer);
char *utf8_string_decode(Buffer *buffer);

#endif /* packet_h__included */
