#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>

#include "mqtt.h"
#include "mqtt_internal.h"
#include "platform.h"

// TODO: Make configurable by platform
#define BUF_LEN 256

static void _reader(MQTTHandle *handle) {
    int num_bytes;
    char buffer[BUF_LEN];

    while (1) {
        num_bytes = read(handle->sock, &buffer, BUF_LEN);
        if (num_bytes == 0) {
            /* Socket closed */
            return;
        } else if (num_bytes < 0) {
            /* Error, TODO: Handle */
        }

        // TODO: Parse and dispatch
    }
}

MQTTHandle *mqtt_connect(MQTTConfig *config, MQTTErrorHandler callback) {
    int ret;
    MQTTHandle *handle = calloc(sizeof(struct _MQTTHandle), 1);
    initialize_platform(handle);

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
 
    if (config->port == 0) {
        config->port = 1883;
    }

    handle->sock = socket(AF_INET, SOCK_STREAM, 0); 
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(config->port);
 
    ret = inet_pton(AF_INET, config->hostname, &(servaddr.sin_addr));
    if (ret == 0) {
        callback(handle, MQTT_Error_Host_Not_Found);
        close(handle->sock);
        free(handle);
        return NULL;
    }

    ret = connect(handle->sock, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (ret != 0) {
        callback(handle, MQTT_Error_Connection_Refused);
        close(handle->sock);
        free(handle);
        return NULL;
    }

    run_read_task(handle, _reader);

    return handle;
}

MQTTStatus mqtt_reconnect(MQTTHandle *handle) {
    // TODO: reconnect
}

MQTTStatus mqtt_subscribe(MQTTHandle *handle, char *topic, MQTTEventHandler callback) {
    // TODO: subscribe
}

MQTTStatus mqtt_unsubscribe(MQTTHandle *handle, char *topic) {
    // TODO: unsubscribe
}

MQTTStatus mqtt_publish(MQTTHandle *handle, char *topic, char *payload, MQTTQosLevel qos_level) {
    // TODO: publish
}

MQTTStatus mqtt_disconnect(MQTTHandle *handle) {
    release_platform(handle);
    free(handle);
}
