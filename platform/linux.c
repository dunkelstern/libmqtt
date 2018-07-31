#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include <pthread.h>
#include <stdlib.h>

#include "mqtt_internal.h"
#include "platform.h"

const size_t max_receive_buffer_size = 4 * 4096; // 16 KB

#define MAX_TASKS 16

struct _PlatformData {
    pthread_t tasks[MAX_TASKS];
    int sock;
};

PlatformStatusCode platform_init(MQTTHandle *handle) {
    handle->platform = calloc(1, sizeof(struct _PlatformData));
    handle->platform->sock = -1;
    if (handle->platform) {
        return PlatformStatusOk;
    }

    return PlatformStatusError;
}

PlatformStatusCode platform_release(MQTTHandle *handle) {
    PlatformData *p = handle->platform;

    for (uint8_t free_task = 0; free_task < MAX_TASKS; free_task++) {
        if (p->tasks[free_task] != 0) {
            DEBUG_LOG("Cannot free platform handle, there are tasks running!");
            return PlatformStatusError;
        }
    }

    free(handle->platform);
    return PlatformStatusOk;
}

PlatformStatusCode platform_run_task(MQTTHandle *handle, int *task_handle, PlatformTask callback) {
    PlatformData *p = handle->platform;
    uint8_t free_task = 0;

    for (free_task = 0; free_task < MAX_TASKS; free_task++) {
        if (p->tasks[free_task] == 0) {
            break;
        }
    }
    if (free_task == MAX_TASKS) {
        return PlatformStatusError;
    }

    if (pthread_create(&p->tasks[free_task], NULL, (void *(*)(void *))callback, (void *)handle)) {
        return PlatformStatusError;
    }

    return PlatformStatusOk;
}

PlatformStatusCode platform_cleanup_task(MQTTHandle *handle, int task_handle) {
    PlatformData *p = handle->platform;

    if ((task_handle < 0) || (task_handle >= MAX_TASKS)) {
        return PlatformStatusError;
    }

    if (p->tasks[task_handle]) {
        pthread_join(p->tasks[task_handle], NULL);
        p->tasks[task_handle] = 0;
    }
    return PlatformStatusOk;
}

PlatformStatusCode platform_resolve_host(char *hostname , char *ip) {
    struct addrinfo hints, *servinfo;
    struct sockaddr_in *h;
 
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
    hints.ai_socktype = SOCK_STREAM;
 
    int ret = getaddrinfo(hostname, NULL, &hints, &servinfo);
    if (ret != 0) {
        DEBUG_LOG("Resolving host failed: %s", gai_strerror(ret));
        return PlatformStatusError;
    }

    // FIXME: we do not try to connect here, perhaps return a list or just the first
    // loop through all the results and connect to the first we can
    for(struct addrinfo *p = servinfo; p != NULL; p = p->ai_next) {
        h = (struct sockaddr_in *)p->ai_addr;
        strcpy(ip , inet_ntoa( h->sin_addr ) );
    }
     
    freeaddrinfo(servinfo); // all done with this structure
    return PlatformStatusOk;
}

PlatformStatusCode platform_connect(MQTTHandle *handle) {
    PlatformData *p = handle->platform;

    int ret;
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    p->sock = socket(AF_INET, SOCK_STREAM, 0);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(handle->config->port);

    char ip[40];
    if (platform_resolve_host(handle->config->hostname, ip) != PlatformStatusOk) {
        bool free_handle = handle->error_handler(handle, handle->config, MQTT_Error_Host_Not_Found);
        if (free_handle) {
            mqtt_free(handle);
        }
        DEBUG_LOG("Resolving hostname failed: %s", strerror(errno));
        close(p->sock);
        return PlatformStatusError;
    }
    ret = inet_pton(AF_INET, ip, &(servaddr.sin_addr));
    if (ret == 0) {
        bool free_handle = handle->error_handler(handle, handle->config, MQTT_Error_Host_Not_Found);
        if (free_handle) {
            mqtt_free(handle);
        }
        DEBUG_LOG("Converting to servaddr failed: %s", strerror(errno));
        close(p->sock);
        return PlatformStatusError;
    }

    ret = connect(p->sock, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (ret != 0) {
        bool free_handle = handle->error_handler(handle, handle->config, MQTT_Error_Connection_Refused);
        if (free_handle) {
            mqtt_free(handle);
        }
        DEBUG_LOG("Connection failed: %s", strerror(errno));
        close(p->sock);
        return PlatformStatusError;
    }

    return PlatformStatusOk;
}

PlatformStatusCode platform_read(MQTTHandle *handle, Buffer *buffer) {
    PlatformData *p = handle->platform;

    while (1) {
        ssize_t num_bytes = read(p->sock, &buffer->data[buffer->position], buffer_free_space(buffer));
        if (num_bytes == 0) {
            /* Socket closed, coordinated shutdown */
            DEBUG_LOG("Socket closed");
            return PlatformStatusError;
        } else if (num_bytes < 0) {
            if ((errno == EINTR) || (errno == EAGAIN)) {
                continue;
            }

            /* Some error occured */
            return PlatformStatusError;
        }

        buffer->position += num_bytes;
        return PlatformStatusOk;
    }
}

PlatformStatusCode platform_write(MQTTHandle *handle, Buffer *buffer) {
    PlatformData *p = handle->platform;

    while (!buffer_eof(buffer)) {
        ssize_t bytes = write(p->sock, buffer->data + buffer->position, buffer_free_space(buffer));
        if (bytes <= 0) {
            return PlatformStatusError;
        }
        buffer->position += bytes;
    }

    return PlatformStatusOk;
}

PlatformStatusCode platform_disconnect(MQTTHandle *handle) {
    PlatformData *p = handle->platform;
    if (p->sock >= 0) {
        close(p->sock);
        p->sock = -1;
    }

    return PlatformStatusOk;
}
