#ifndef protocol_h__included
#define protocol_h__included

#include "mqtt.h"

bool send_connect_packet(MQTTHandle *handle);
bool send_subscribe_packet(MQTTHandle *handle, char *topic);
bool send_unsubscribe_packet(MQTTHandle *handle, char *topic);
bool send_publish_packet(MQTTHandle *handle, char *topic, char *message, MQTTQosLevel qos);

#endif
