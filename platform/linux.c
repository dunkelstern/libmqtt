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

#include "debug.h"
#include "mqtt_internal.h"
#include "platform.h"

const size_t max_receive_buffer_size = 4 * 4096; // 16 KB

#define MAX_TASKS 16
#define MAX_TIMERS 5

typedef struct {
    PlatformTimerCallback callback;
    int status;
    int interval;

} PlatformTimer;

struct _PlatformData {
    pthread_t tasks[MAX_TASKS];
    PlatformTimer timers[MAX_TIMERS];
    int timer_task;
    int sock;
};

PlatformTaskFunc(timer_task) {
    MQTTHandle *handle = (MQTTHandle *)context;
    while (1) {
        platform_sleep(1000);

        bool active = false;
        for (uint8_t i = 0; i < MAX_TIMERS; i++) {
            PlatformTimer *timer = &handle->platform->timers[i];

            if (timer->callback != NULL) {
                timer->status--;

                if (timer->status == 0) {
                    timer->callback(handle, i);
                    timer->status = timer->interval;
                }

                active = true;
            }
        }

        if (!active) {
            return NULL;
        }
    }
}

PlatformStatusCode platform_init(MQTTHandle *handle) {
    handle->platform = (PlatformData *)calloc(1, sizeof(struct _PlatformData));
    handle->platform->sock = -1;
    handle->platform->timer_task = -1;
    if (handle->platform) {
        return PlatformStatusOk;
    }

    return PlatformStatusError;
}

PlatformStatusCode platform_release(MQTTHandle *handle) {
    PlatformData *p = handle->platform;

    // shut down all timers
    if (p->timer_task >= 0) {
        for (uint8_t free_timer = 0; free_timer < MAX_TIMERS; free_timer++) {
            PlatformStatusCode ret = platform_destroy_timer(handle, free_timer);
            if (ret != PlatformStatusOk) {
                DEBUG_LOG("Could not shut down all timers");
                return PlatformStatusError;
            }
        }
    }

    // check if there are tasks running
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

    *task_handle = free_task;
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
        close(p->sock);
        // FIXME: This will lead to stack overflow
        bool free_handle = handle->error_handler(handle, handle->config, MQTT_Error_Host_Not_Found);
        if (free_handle) {
            mqtt_free(handle);
        }
        DEBUG_LOG("Resolving hostname failed: %s", strerror(errno));
        return PlatformStatusError;
    }
    ret = inet_pton(AF_INET, ip, &(servaddr.sin_addr));
    if (ret == 0) {
        close(p->sock);
        // FIXME: This will lead to stack overflow
        bool free_handle = handle->error_handler(handle, handle->config, MQTT_Error_Host_Not_Found);
        if (free_handle) {
            mqtt_free(handle);
        }
        DEBUG_LOG("Converting to servaddr failed: %s", strerror(errno));
        return PlatformStatusError;
    }

    ret = connect(p->sock, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (ret != 0) {
        close(p->sock);
        // FIXME: This will lead to stack overflow
        bool free_handle = handle->error_handler(handle, handle->config, MQTT_Error_Connection_Refused);
        if (free_handle) {
            mqtt_free(handle);
        }
        DEBUG_LOG("Connection failed: %s", strerror(errno));
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

PlatformStatusCode platform_create_timer(MQTTHandle *handle, int interval, int *timer_handle, PlatformTimerCallback callback) {
    PlatformData *p = handle->platform;
    uint8_t free_timer = 0;

    for (free_timer = 0; free_timer < MAX_TIMERS; free_timer++) {
        // DEBUG_LOG("Timer %d: %s", free_timer, p->timers[free_timer].callback ? "Occupied" : "Free");
        if (p->timers[free_timer].callback == NULL) {
            break;
        }
    }
    if (free_timer == MAX_TASKS) {
        return PlatformStatusError;
    }

    PlatformTimer *timer = &p->timers[free_timer];

    timer->callback = callback;
    timer->status = interval;
    timer->interval = interval;

    if (p->timer_task < 0) {
        PlatformStatusCode ret = platform_run_task(handle, &p->timer_task, timer_task);
        if (ret != PlatformStatusOk) {
            DEBUG_LOG("Could not start timer task");
            return PlatformStatusError;
        }
    }

    *timer_handle = free_timer;
    return PlatformStatusOk;
}

PlatformStatusCode platform_destroy_timer(MQTTHandle *handle, int timer_handle) {
    PlatformData *p = handle->platform;

    if ((timer_handle < 0) || (timer_handle >= MAX_TIMERS)) {
        DEBUG_LOG("Invalid timer handle");
        return PlatformStatusError;
    }

    p->timers[timer_handle].callback = NULL;


    // check if there is a timer running
    uint8_t free_timer = 0;

    for (free_timer = 0; free_timer < MAX_TIMERS; free_timer++) {
        if (p->timers[free_timer].callback != NULL) {
            break;
        }
    }
    if ((free_timer == MAX_TIMERS) && (p->timer_task >= 0)) {
        // if we get here we have no running timers, so we destroy the timer task
        PlatformStatusCode ret = platform_cleanup_task(handle, p->timer_task);
        if (ret == PlatformStatusOk) {
            p->timer_task = -1;
        } else {
            DEBUG_LOG("Could not finish timer task");
            return PlatformStatusError;
        }
    }

    return PlatformStatusOk;
}


PlatformStatusCode platform_sleep(int milliseconds) {
    usleep(milliseconds * 1000);
    return PlatformStatusOk;
}

#if DEBUG
void platform_dump(MQTTHandle *handle) {
    if (handle->platform == NULL) {
        DEBUG_LOG("   + PLATFORM HANDLE NOT INITIALIZED");
        return;
    }

    DEBUG_LOG("   - Timer task ID: %d", handle->platform->timer_task);
    DEBUG_LOG("   - Socket: %d", handle->platform->sock);
    DEBUG_LOG("   - Tasks");
    for (int i = 0; i < MAX_TASKS; i++) {
        DEBUG_LOG("     - %d, handle: %d", i, handle->platform->tasks[i]);
    }
    DEBUG_LOG("   - Timers");
    for (int i = 0; i < MAX_TIMERS; i++) {
        DEBUG_LOG("     - Timer %d", i);
        DEBUG_LOG("       - callback: %d", (int)handle->platform->timers[i].callback);
        DEBUG_LOG("       - status: %d",  handle->platform->timers[i].status);
        DEBUG_LOG("       - interval: %d", handle->platform->timers[i].interval);
    }
}
#endif