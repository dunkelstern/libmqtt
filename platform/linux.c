#include <pthread.h>
#include <stdlib.h>

#include "platform.h"

struct _PlatformData {
    pthread_t read_thread;
};

void initialize_platform(MQTTHandle *handle) {
    handle->platform = calloc(sizeof(struct _PlatformData), 1);
}

MQTTStatus run_read_task(MQTTHandle *handle, Reader reader) {
    if (pthread_create(&handle->platform->read_thread, NULL, (void *(*)(void *))reader, (void *)handle)) {
        return MQTT_STATUS_ERROR;
    }

    return MQTT_STATUS_OK;
}

MQTTStatus join_read_task(MQTTHandle *handle) {
    pthread_join(handle->platform->read_thread, NULL);

    return MQTT_STATUS_OK;
}

void release_platform(MQTTHandle *handle) {
    free(handle->platform);
}
