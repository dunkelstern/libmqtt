#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mqtt.h"

bool leave = false;

#define LOG(fmt, ...) fprintf(stdout, fmt "\n", ## __VA_ARGS__)

bool err_handler(MQTTHandle *handle, MQTTErrorCode error) {
    LOG("Error received: %d", error);
    exit(1);

    return true;
}

void mqtt_connected(MQTTHandle *handle, void *context) {
    LOG("Connected!");

    LOG("Trying publish to testsuite/mqtt/test...");
    MQTTStatus result = mqtt_publish(handle, "testsuite/mqtt/test", "payload", MQTT_QOS_0);
    if (result != MQTT_STATUS_OK) {
        LOG("Could not publish");
        exit(1);
    }

    sleep(1);

    LOG("Disconnecting...");
    result = mqtt_disconnect(handle, NULL, NULL);
    if (result != MQTT_STATUS_OK) {
        LOG("Could not disconnect");
        exit(1);
    }

    exit(0);
}

int main(int argc, char **argv) {
    MQTTConfig config = { 0 };

    config.client_id = "libmqtt_testsuite";
    config.hostname = "localhost";
    config.port = 1883;

    LOG("Trying to connect to %s", config.hostname);
    MQTTHandle *mqtt = mqtt_connect(&config, mqtt_connected, NULL, err_handler);

    if (mqtt == NULL) {
        LOG("Connection failed!");
        return 1;
    }

    while (!leave) {
        LOG("Waiting...");
        sleep(1);
    }
}
