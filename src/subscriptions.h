#ifndef subscriptions_h__included
#define subscriptions_h__included

#include "mqtt.h"

typedef struct _SubscriptionItem {
    struct _SubscriptionItem *next;

    char *topic;
    MQTTPublishEventHandler handler;
    bool pending;
} SubscriptionItem;

typedef struct {
    SubscriptionItem *items;
} Subscriptions;

void add_subscription(MQTTHandle *handle, char *topic, MQTTPublishEventHandler callback);
void remove_subscription(MQTTHandle *handle, char *topic);
void subscription_set_pending(MQTTHandle *handle, char *topic, bool pending);

void dispatch_subscription(MQTTHandle *handle, PublishPayload *payload);

#endif /* subscription_h__included */
