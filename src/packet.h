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
} PacketIDPayload;

#define PubAckPayload PacketIDPayload
#define PubRecPayload PacketIDPayload
#define PubRelPayload PacketIDPayload
#define PubCompPayload PacketIDPayload

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

#define UnsubAckPayload PacketIDPayload

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
 * Utility
 */

uint16_t get_packet_id(MQTTPacket *packet);
char *get_packet_name(MQTTPacket *packet);

#endif /* packet_h__included */
