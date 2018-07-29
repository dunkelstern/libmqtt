#include <stdio.h>
#include <unistd.h>

#include "mqtt.h"

#define LOG(fmt, ...) fprintf(stdout, fmt "\n", ## __VA_ARGS__)

bool err_handler(MQTTHandle *handle, MQTTErrorCode error) {
    LOG("Error received: %d", error);
    
    return 1;
}

bool leave = false;
void callback(MQTTHandle *handle, char *topic, char *payload) {
    LOG("Received publish: %s -> %s", topic, payload);
    leave = true;
}

int main(int argc, char **argv) {
    MQTTConfig config;

    config.client_id = "libmqtt_testsuite";
    config.hostname = "test.mosquitto.org";
    config.port = 1883;

    LOG("Trying to connect to test.mosquitto.org");
    MQTTHandle *mqtt = mqtt_connect(&config, err_handler);

    if (mqtt == NULL) {
        LOG("Connection failed!");
        return 1;
    }
    LOG("Connected!");
    
    sleep(5);

    LOG("Trying subscribe on testsuite/mqtt/test...");
    MQTTStatus result = mqtt_subscribe(mqtt, "testsuite/mqtt/test", callback);
    if (result != MQTT_STATUS_OK) {
        LOG("Could not publish");
        return 1;
    }

    while (!leave) {
        LOG("Waiting...");
        sleep(1);
    }

    LOG("Disconnecting...");
    mqtt_disconnect(mqtt);
    return 0;
}
