#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mqtt.h"

bool leave = false;

#define LOG(fmt, ...) fprintf(stdout, fmt "\n", ## __VA_ARGS__)

bool err_handler(MQTTHandle *handle, MQTTErrorCode error) {
    LOG("Error received: %d", error);

    return 1;
}

void callback(MQTTHandle *handle, char *topic, char *payload) {
    LOG("Received publish: %s -> %s", topic, payload);

    MQTTStatus result = mqtt_unsubscribe(handle, "testsuite/mqtt/test");
    if (result != MQTT_STATUS_OK) {
        LOG("Could not unsubscribe test");
        exit(1);
    }

    result = mqtt_unsubscribe(handle, "testsuite/mqtt/test2");
    if (result != MQTT_STATUS_OK) {
        LOG("Could not unsubscribe test 2");
        exit(1);
    }
    sleep(1);

    leave = true;
}

void mqtt_connected(MQTTHandle *handle, void *context) {
    LOG("Connected!");

    LOG("Trying subscribe on testsuite/mqtt/test...");
    MQTTStatus result = mqtt_subscribe(handle, "testsuite/mqtt/test", MQTT_QOS_0, callback);
    if (result != MQTT_STATUS_OK) {
        LOG("Could not subscribe test");
        exit(1);
    }

    result = mqtt_subscribe(handle, "testsuite/mqtt/test2", MQTT_QOS_0, callback);
    if (result != MQTT_STATUS_OK) {
        LOG("Could not subscribe test 2");
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

    while (!leave) {
        LOG("Waiting...");
        sleep(1);
    }

    LOG("Disconnecting...");
    mqtt_disconnect(mqtt, NULL, NULL);
}
