#include <stdio.h>
#include <stdlib.h>

#include "platform.h"
#include "mqtt.h"

int leave = 0;

#define LOG(fmt, ...) fprintf(stdout, fmt "\n", ## __VA_ARGS__)

bool err_handler(MQTTHandle *handle, MQTTConfig *config, MQTTErrorCode error) {
    LOG("Error received: %d", error);

    return 1;
}

void mqtt_reconnected(MQTTHandle *handle, void *context) {
    LOG("Reconnected!");
}

void callback(MQTTHandle *handle, char *topic, char *payload) {
    LOG("Received publish: %s -> %s", topic, payload);

    leave++;
}

void mqtt_connected(MQTTHandle *handle, void *context) {
    LOG("Connected!");

    LOG("Trying subscribe on testsuite/mqtt/test...");
    MQTTStatus result = mqtt_subscribe(handle, "testsuite/mqtt/test", MQTT_QOS_0, callback);
    if (result != MQTT_STATUS_OK) {
        LOG("Could not subscribe test");
        exit(1);
    }
}

int main(int argc, char **argv) {
    MQTTConfig config = { 0 };

    config.client_id = "libmqtt_testsuite";
    config.hostname = "localhost";
    config.clean_session = true;

    LOG("Trying to connect to %s...", config.hostname);
    MQTTHandle *mqtt = mqtt_connect(&config, mqtt_connected, NULL, err_handler);

    if (mqtt == NULL) {
        LOG("Connection failed!");
        return 1;
    }

    while (leave < 1) {
        LOG("Waiting for first publish...");
        platform_sleep(1000);
    }
    mqtt_reconnect(mqtt, mqtt_reconnected, NULL);

    while (leave < 2) {
        LOG("Waiting for second publish...");
        platform_sleep(1000);
    }

    LOG("Disconnecting...");
    mqtt_disconnect(mqtt, NULL, NULL);
}
