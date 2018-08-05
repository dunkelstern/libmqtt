#ifndef protocol_h__included
#define protocol_h__included

#include "mqtt.h"

#if MQTT_CLIENT
bool send_connect_packet(MQTTHandle *handle);
bool send_subscribe_packet(MQTTHandle *handle, char *topic, MQTTQosLevel qos);
bool send_unsubscribe_packet(MQTTHandle *handle, char *topic);
bool send_ping_packet(MQTTHandle *handle);
bool send_disconnect_packet(MQTTHandle *handle);
bool send_puback_packet(MQTTHandle *handle, uint16_t packet_id);
bool send_pubrec_packet(MQTTHandle *handle, uint16_t packet_id, MQTTPublishEventHandler callback, PublishPayload *publish);
#endif /* MQTT_CLIENT */

bool send_publish_packet(MQTTHandle *handle, char *topic, char *message, MQTTQosLevel qos, MQTTPublishEventHandler callback);

#endif
