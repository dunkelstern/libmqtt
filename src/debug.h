#ifndef debug_h__included
#define debug_h__included

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>

#include "platform.h"

#if DEBUG_HEXDUMP
static inline void hexdump(char *data, size_t len, int indent) {
    for (int i = 0; i < len;) {

        // indent
        for (int col = 0; col < indent; col++) {
            platform_printf(" ");
        }

        // address
        platform_printf("0x%04x: ", i);

        // hex field
        for (int col = 0; col < 16; col++) {
            if (i + col < len) {
                platform_printf("%02x ", (uint8_t)data[i + col]);
            } else {
                platform_printf("   ");
            }
        }

        // separator
        platform_printf("  |  ");

        // ascii field
        for (int col = 0; col < 16; col++) {
            if (i + col < len) {
                char c = data[i + col];
                if ((c > 127) || (c < 32)) c = '.';
                platform_printf("%c", (uint8_t)c);
            } else {
                platform_printf(" ");
            }
        }

        platform_printf("\n");
        i += 16;
    }
}
#else
#define hexdump(_data, _len, _indent) /* */
#endif /* DEBUG_HEXDUMP */

#if DEBUG
# define DEBUG_LOG(fmt, ...) platform_printf("%s:%d: " fmt, __FILE__, __LINE__, ## __VA_ARGS__);

static void DEBUG_DUMP_HANDLE(MQTTHandle *handle) {
    DEBUG_LOG("MQTT Handle Dump:");
    DEBUG_LOG(" - Error Handler: %d", (int)handle->error_handler);
    DEBUG_LOG(" - Reader alive: %s", handle->reader_alive ? "true" : "false");
    DEBUG_LOG(" - Reader task: %d", handle->read_task_handle);
    DEBUG_LOG(" - Current packet id: %d", handle->packet_id_counter);
    DEBUG_LOG(" - Currently reconnecting: %s", handle->reconnecting ? "true" : "false");
    DEBUG_LOG(" - Keepalive timer id: %d", handle->keepalive_timer);

    DEBUG_LOG(" - Config");
    DEBUG_LOG("   - hostname: %s", handle->config->hostname);
    DEBUG_LOG("   - port: %d", handle->config->port);
    DEBUG_LOG("   - client_id: %s", handle->config->client_id);
    DEBUG_LOG("   - clean_session: %s", handle->config->clean_session ? "true" : "false");
    DEBUG_LOG("   - username: %s", handle->config->username ? handle->config->username : "NULL");
    DEBUG_LOG("   - password: %s", handle->config->password ? handle->config->password : "NULL");
    DEBUG_LOG("   - last_will_topic: %s", handle->config->last_will_topic ? handle->config->last_will_topic : "NULL");
    DEBUG_LOG("   - last_will_message: %s", handle->config->last_will_message ? handle->config->last_will_message : "NULL");
    DEBUG_LOG("   - last_will_retain: %s", handle->config->last_will_retain ? "true" : "false");

    DEBUG_LOG(" - Platform");
    platform_dump(handle);

    DEBUG_LOG(" - Callback Queue");

    DEBUG_LOG(" - Subscriptions");
    SubscriptionItem *item = handle->subscriptions.items;
    if (item == NULL) {
        DEBUG_LOG("   + NO SUBSCRIPTIONS");
    } else {
        while(item != NULL) {
            DEBUG_LOG("   - Item");
            DEBUG_LOG("     - topic: %s", item->topic);
            DEBUG_LOG("     - qos: %d", item->qos);
            DEBUG_LOG("     - pending: %s", item->pending ? "true" : "false");
            DEBUG_LOG("     - handler: %d", (int)item->handler);
            item = item->next;
        }
    }
}

#else /* DEBUG */
# define DEBUG_LOG(fmt, ...) /* */
# define DEBUG_DUMP_HANDLE(_handle) /* */
#endif /* DEBUG */


#endif /* debug_h__included */
