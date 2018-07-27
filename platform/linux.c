#include <pthread.h>

#include "platform.h"

struct _PlatformData {
    pthread_t read_thread;
};

void initialize_platform(MQTTHandle *handle) {
    handle->platform = calloc(sizeof(struct _PlatformData));
}

MQTTStatus run_read_task(MQTTHandle *handle, Reader reader) {
    if (pthread_create(&handle->platform->read_thread, NULL, reader, (void *)handle)) {
        return MQTT_STATUS_ERROR;
    }
}

MQTTStatus join_read_task(MQTTHandle *handle) {
    pthread_join(handle->platform->read_thread);
}

void release_platform(MQTTHandle *handle) {
    free(handle->platform);
}
