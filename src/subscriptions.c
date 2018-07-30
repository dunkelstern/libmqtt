#include "mqtt_internal.h"
#include "subscriptions.h"

void add_subscription(MQTTHandle *handle, char *topic, MQTTQosLevel qos, MQTTPublishEventHandler callback) {
    SubscriptionItem *item = calloc(1, sizeof(SubscriptionItem));

    item->topic = topic;
    item->qos = qos;
    item->handler = callback;
    item->pending = true;

    // insert at start
    if (handle->subscriptions.items != NULL) {
        item->next = handle->subscriptions.items;
    }

    handle->subscriptions.items = item;
}

void remove_subscription(MQTTHandle *handle, char *topic) {
    SubscriptionItem *item = handle->subscriptions.items;
    SubscriptionItem *prev = NULL;

    while (item != NULL) {
        if (strcmp(topic, item->topic) == 0) {
            if (prev == NULL) {
                handle->subscriptions.items = item->next;
            } else {
                prev->next = item->next;
            }

            free(item);
            break;
        }

        prev = item;
        item = item->next;
    }
}

void subscription_set_pending(MQTTHandle *handle, char *topic, bool pending) {
    SubscriptionItem *item = handle->subscriptions.items;

    while (item != NULL) {
        if (strcmp(topic, item->topic) == 0) {
            item->pending = pending;
            break;
        }

        item = item->next;
    }
}

void dispatch_subscription(MQTTHandle *handle, PublishPayload *payload) {
    SubscriptionItem *item = handle->subscriptions.items;

    // TODO: Handle server Qos

    while (item != NULL) {
        if ((item->pending == false) && (strcmp(payload->topic, item->topic) == 0)) {
            if (item->handler) {
                item->handler(handle, payload->topic, payload->message);
            }
            break;
        }

        item = item->next;
    }
}
