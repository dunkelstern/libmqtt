#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mqtt.h"

#define LOG(fmt, ...) fprintf(stdout, fmt "\n", ## __VA_ARGS__)

bool err_handler(MQTTHandle *handle, MQTTErrorCode error) {
    LOG("Error received: %d", error);
    exit(1);

    return true;
}

int main(int argc, char **argv) {
    MQTTConfig config = { 0 };

    config.client_id = "libmqtt_testsuite";
    config.hostname = "localhost";
    config.port = 1883;

    LOG("Trying to connect to %s", config.hostname);
    MQTTHandle *mqtt = mqtt_connect(&config, err_handler);

    if (mqtt == NULL) {
        LOG("Connection failed!");
        return 1;
    }

    LOG("Connected!");

    sleep(5);

    LOG("Trying publish to testsuite/mqtt/test...");
    MQTTStatus result = mqtt_publish(mqtt, "testsuite/mqtt/test", "payload", MQTT_QOS_0);
    if (result != MQTT_STATUS_OK) {
        LOG("Could not publish");
        return 1;
    }

    sleep(5);

    LOG("Disconnecting...");
    result = mqtt_disconnect(mqtt);
    if (result != MQTT_STATUS_OK) {
        LOG("Could not disconnect");
        return 1;
    }
    return 0;
}
