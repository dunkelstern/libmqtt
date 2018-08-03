#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "mqtt.h"

int leave = 0;

#define LOG(fmt, ...) fprintf(stdout, fmt "\n", ## __VA_ARGS__)

#ifndef _unused
#define _unused __attribute__((unused))
#endif

bool err_handler(_unused MQTTHandle *handle, _unused MQTTConfig *config, MQTTErrorCode error) {
    LOG("Error received: %d", error);
    exit(1);

    return true;
}

void publish_handler(_unused MQTTHandle *handle, char *topic, char *message) {
    LOG("Published %s -> %s", topic, message);

    leave++;
}

void mqtt_connected(MQTTHandle *handle, _unused void *context) {
    LOG("Connected!");
    MQTTStatus result;

    LOG("Trying publish to testsuite/mqtt/test...");
    result = mqtt_publish(handle, "testsuite/mqtt/test", "payload1", MQTT_QOS_0, publish_handler);
    if (result != MQTT_STATUS_OK) {
        LOG("Could not publish");
        exit(1);
    }

    LOG("Trying publish to testsuite/mqtt/test_qos1...");
    result = mqtt_publish(handle, "testsuite/mqtt/test_qos1", "payload2", MQTT_QOS_1, publish_handler);
    if (result != MQTT_STATUS_OK) {
        LOG("Could not publish");
        exit(1);
    }

    LOG("Trying publish to testsuite/mqtt/test_qos2...");
    result = mqtt_publish(handle, "testsuite/mqtt/test_qos2", "payload3", MQTT_QOS_2, publish_handler);
    if (result != MQTT_STATUS_OK) {
        LOG("Could not publish");
        exit(1);
    }

    leave = true;
}

int main(_unused int argc, _unused char **argv) {
    MQTTConfig config = { 0 };

    config.client_id = "libmqtt_testsuite_this_is_too_long";
    config.hostname = "localhost";

    config.last_will_topic = "testsuite/last_will";
    config.last_will_message = "RIP";

    LOG("Testing too long client id...");
    MQTTHandle *mqtt = mqtt_connect(&config, mqtt_connected, NULL, err_handler);
    if (mqtt != NULL) {
        LOG("Handle should be NULL, but it wasn't");
        return 1;
    }    

    config.client_id = "libmqtt_testsuite";
    LOG("Trying to connect to %s", config.hostname);
    mqtt = mqtt_connect(&config, mqtt_connected, NULL, err_handler);

    if (mqtt == NULL) {
        LOG("Connection failed!");
        return 1;
    }

    int cancel = 0;
    while (leave < 3) {
        LOG("Waiting...");
        sleep(1);
        cancel++;
        if (cancel == 10) {
            break;
        }
    }

    LOG("Waiting for ping to happen...");
    sleep(5);

    LOG("Disconnecting...");
    MQTTStatus result = mqtt_disconnect(mqtt, NULL, NULL);
    if (result != MQTT_STATUS_OK) {
        LOG("Could not disconnect");
        exit(1);
    }
}
