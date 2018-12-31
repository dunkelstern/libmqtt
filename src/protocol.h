#ifndef protocol_h__included
#define protocol_h__included

#include "mqtt.h"

#if MQTT_CLIENT
MQTTErrorCode send_connect_packet(MQTTHandle *handle);
MQTTErrorCode send_subscribe_packet(MQTTHandle *handle, char *topic, MQTTQosLevel qos);
MQTTErrorCode send_unsubscribe_packet(MQTTHandle *handle, char *topic);
MQTTErrorCode send_ping_packet(MQTTHandle *handle);
MQTTErrorCode send_disconnect_packet(MQTTHandle *handle);
MQTTErrorCode send_puback_packet(MQTTHandle *handle, uint16_t packet_id);
MQTTErrorCode send_pubrec_packet(MQTTHandle *handle, uint16_t packet_id, MQTTPublishEventHandler callback, PublishPayload *publish);
#endif /* MQTT_CLIENT */

MQTTErrorCode send_publish_packet(MQTTHandle *handle, char *topic, char *message, MQTTQosLevel qos, MQTTPublishEventHandler callback);

#endif
