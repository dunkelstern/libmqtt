#ifndef subscriptions_h__included
#define subscriptions_h__included

#include "mqtt.h"

typedef struct _SubscriptionItem {
    struct _SubscriptionItem *next;

    char *topic;
    MQTTQosLevel qos;
    MQTTPublishEventHandler handler;
    bool pending;
} SubscriptionItem;

typedef struct {
    SubscriptionItem *items;
} Subscriptions;

void add_subscription(MQTTHandle *handle, char *topic, MQTTQosLevel qos, MQTTPublishEventHandler callback);
void remove_subscription(MQTTHandle *handle, char *topic);
void remove_all_subscriptions(MQTTHandle *handle);
void subscription_set_pending(MQTTHandle *handle, char *topic, bool pending);

void dispatch_subscription(MQTTHandle *handle, PublishPayload *payload);
void dispatch_subscription_direct(MQTTHandle *handle, char *topic, char *message);

#endif /* subscription_h__included */
