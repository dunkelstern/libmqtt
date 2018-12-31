#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

#include <freertos/task.h>
#include <stdlib.h>

#include "debug.h"
#include "mqtt_internal.h"
#include "platform.h"

const size_t max_receive_buffer_size = 2 * 4096; // 8 KB

#define MAX_TASKS 4
#define MAX_TIMERS 2

typedef struct {
    PlatformTimerCallback callback;
    int status;
    int interval;

} PlatformTimer;

struct _PlatformData {
    TaskHandle_t tasks[MAX_TASKS];
    PlatformTimer timers[MAX_TIMERS];
    int timer_task;
    int sock;
};

PlatformTaskFunc(timer_task) {
    MQTTHandle *handle = (MQTTHandle *)context;
    while (1) {
        platform_sleep(1000);

        for (uint8_t i = 0; i < MAX_TIMERS; i++) {
            PlatformTimer *timer = &handle->platform->timers[i];

            if (timer->callback != NULL) {
                timer->status--;

                if (timer->status == 0) {
                    timer->callback(handle, i);
                    timer->status = timer->interval;
                }
            }
        }
    }
}

MQTTErrorCode platform_init(MQTTHandle *handle) {
    DEBUG_LOG("Platform init...")
    handle->platform = (PlatformData *)calloc(1, sizeof(struct _PlatformData));
    if (!handle->platform) {
        return MQTT_Error_Out_Of_Memory;
    }

    handle->platform->sock = -1;
    handle->platform->timer_task = -1;

    DEBUG_LOG("Platform handle %d", (int)handle->platform);
    return MQTT_Error_Ok;
}

MQTTErrorCode platform_release(MQTTHandle *handle) {
    PlatformData *p = handle->platform;

    // shut down all timers
    if (p->timer_task >= 0) {
        for (uint8_t free_timer = 0; free_timer < MAX_TIMERS; free_timer++) {
            MQTTErrorCode ret = platform_destroy_timer(handle, free_timer);
            if (ret != MQTT_Error_Ok) {
                DEBUG_LOG("Could not shut down all timers");
                return ret;
            }
        }
        vTaskDelete(p->tasks[p->timer_task]);
    }

    // check if there are tasks running
    for (uint8_t free_task = 0; free_task < MAX_TASKS; free_task++) {
        if (p->tasks[free_task] != 0) {
            DEBUG_LOG("Cannot free platform handle, there are tasks running!");
            return MQTT_Error_Tasks_Running;
        }
    }

    free(handle->platform);
    return MQTT_Error_Ok;
}

MQTTErrorCode platform_run_task(MQTTHandle *handle, int *task_handle, PlatformTask callback) {
    PlatformData *p = handle->platform;
    uint8_t free_task = 0;

    for (free_task = 0; free_task < MAX_TASKS; free_task++) {
        if (p->tasks[free_task] == 0) {
            break;
        }
    }
    if (free_task == MAX_TASKS) {
        DEBUG_LOG("Error creating task, out of slots");
        return MQTT_Error_Max_Number_Of_Tasks_Exceeded;
    }

    DEBUG_LOG("Creating task on handle %d", free_task);

    if (xTaskCreatePinnedToCore(callback, "mqtt_task", 4096, handle, tskIDLE_PRIORITY, &p->tasks[free_task], 0) != pdPASS) {
        return MQTT_Error_Internal;
    }

    *task_handle = free_task;
    return MQTT_Error_Ok;
}

MQTTErrorCode platform_cleanup_task(MQTTHandle *handle, int task_handle) {
    PlatformData *p = handle->platform;
    DEBUG_LOG("Cleaning up task %d", task_handle);

    if ((task_handle < 0) || (task_handle >= MAX_TASKS)) {
        return MQTT_Error_Invalid_Parameter;
    }

    if (p->tasks[task_handle]) {
        vTaskDelete(p->tasks[task_handle]);
        p->tasks[task_handle] = 0;
    }
    return MQTT_Error_Ok;
}

void platform_suspend_task(MQTTHandle *handle, int task_handle) {
    DEBUG_LOG("Suspending task %d...", task_handle);
    vTaskResume(handle->platform->tasks[task_handle]);
}

void platform_resume_task(MQTTHandle *handle, int task_handle) {
    DEBUG_LOG("Resuming task %d...", task_handle);
    vTaskSuspend(handle->platform->tasks[task_handle]);
}

void platform_suspend_self() {
    DEBUG_LOG("Suspending current task...");
    vTaskSuspend(NULL);
}

MQTTErrorCode platform_resolve_host(char *hostname , char *ip) {
    struct addrinfo hints, *servinfo;
    struct sockaddr_in *h;
 
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
    hints.ai_socktype = SOCK_STREAM;
 
    int ret = getaddrinfo(hostname, NULL, &hints, &servinfo);
    if (ret != 0) {
        DEBUG_LOG("Resolving host failed: %s", strerror(ret));
        return MQTT_Error_Host_Not_Found;
    }

    // FIXME: we do not try to connect here, perhaps return a list or just the first
    // loop through all the results and connect to the first we can
    for(struct addrinfo *p = servinfo; p != NULL; p = p->ai_next) {
        h = (struct sockaddr_in *)p->ai_addr;
        strcpy(ip , inet_ntoa( h->sin_addr ) );
    }
     
    freeaddrinfo(servinfo); // all done with this structure
    return MQTT_Error_Ok;
}

MQTTErrorCode platform_connect(MQTTHandle *handle) {
    PlatformData *p = handle->platform;

    int ret;
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    DEBUG_LOG("Creating socket...");
    p->sock = socket(AF_INET, SOCK_STREAM, 0);
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(handle->config->port);

    DEBUG_LOG("Resolving host...");
    char ip[40];
    if (platform_resolve_host(handle->config->hostname, ip) != MQTT_Error_Ok) {
        close(p->sock);
        DEBUG_LOG("Resolving hostname failed: %s", strerror(errno));
        return MQTT_Error_Host_Not_Found;
    }

    ret = inet_pton(AF_INET, ip, &(servaddr.sin_addr));
    DEBUG_LOG("Host resolved to %d.%d.%d.%d", 
        (servaddr.sin_addr.s_addr) & 0xff,
        (servaddr.sin_addr.s_addr >> 8) & 0xff,
        (servaddr.sin_addr.s_addr >> 16) & 0xff,
        (servaddr.sin_addr.s_addr >> 24) & 0xff
    );

    if (ret == 0) {
        DEBUG_LOG("Converting to servaddr failed: %s", strerror(errno));
        close(p->sock);
        return MQTT_Error_Internal;
    }

    DEBUG_LOG("Connecting...")
    ret = connect(p->sock, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (ret != 0) {
        DEBUG_LOG("Connection failed: %s", strerror(errno));
        close(p->sock);
        return MQTT_Error_Connection_Refused;
    }

    return MQTT_Error_Ok;
}

MQTTErrorCode platform_read(MQTTHandle *handle, Buffer *buffer) {
    PlatformData *p = handle->platform;

    DEBUG_LOG("Read loop entered...");
    while (1) {
        ssize_t num_bytes = read(p->sock, &buffer->data[buffer->position], buffer_free_space(buffer));
        if (num_bytes == 0) {
            /* Socket closed, coordinated shutdown */
            DEBUG_LOG("Socket closed");
            return MQTT_Error_Connection_Reset;
        } else if (num_bytes < 0) {
            if ((errno == EINTR) || (errno == EAGAIN)) {
                continue;
            }

            /* Some error occured */
            DEBUG_LOG("Error reading...");
            return MQTT_Error_Internal;
        }

        buffer->position += num_bytes;
        DEBUG_LOG("Read %d bytes, buffer now at %d bytes", num_bytes, buffer->position);
        return MQTT_Error_Ok;
    }
}

MQTTErrorCode platform_write(MQTTHandle *handle, Buffer *buffer) {
    PlatformData *p = handle->platform;

    DEBUG_LOG("Writing data...");
    if (!handle->reader_alive) {
        DEBUG_LOG("Reader task is dead!");
        return MQTT_Error_Connection_Reset;
    }
    while (!buffer_eof(buffer)) {
        ssize_t bytes = write(p->sock, buffer->data + buffer->position, buffer_free_space(buffer));
        if (bytes <= 0) {
            DEBUG_LOG("Error while writing...");
            return MQTT_Error_Internal;
        }
        buffer->position += bytes;
    }

    DEBUG_LOG("Write succeeded");
    return MQTT_Error_Ok;
}

MQTTErrorCode platform_disconnect(MQTTHandle *handle) {
    DEBUG_LOG("Closing socket");
    PlatformData *p = handle->platform;
    if (p->sock >= 0) {
        close(p->sock);
        p->sock = -1;
    }

    return MQTT_Error_Ok;
}

MQTTErrorCode platform_create_timer(MQTTHandle *handle, int interval, int *timer_handle, PlatformTimerCallback callback) {
    PlatformData *p = handle->platform;
    uint8_t free_timer = 0;

    for (free_timer = 0; free_timer < MAX_TIMERS; free_timer++) {
        // DEBUG_LOG("Timer %d: %s", free_timer, p->timers[free_timer].callback ? "Occupied" : "Free");
        if (p->timers[free_timer].callback == NULL) {
            break;
        }
    }
    if (free_timer == MAX_TIMERS) {
        DEBUG_LOG("Could not allocate timer, out of slots");
        return MQTT_Error_Max_Number_Of_Timers_Exceeded;
    }

    DEBUG_LOG("Creating timer with interval %d at %d...", interval, free_timer);

    PlatformTimer *timer = &p->timers[free_timer];

    timer->callback = callback;
    timer->status = interval;
    timer->interval = interval;

    if (p->timer_task < 0) {
        MQTTErrorCode ret = platform_run_task(handle, &p->timer_task, timer_task);
        if (ret != MQTT_Error_Ok) {
            DEBUG_LOG("Could not start timer task");
            return ret;
        }
    }

    *timer_handle = free_timer;
    return MQTT_Error_Ok;
}

MQTTErrorCode platform_destroy_timer(MQTTHandle *handle, int timer_handle) {
    PlatformData *p = handle->platform;

    DEBUG_LOG("Destroying timer %d", timer_handle);

    if ((timer_handle < 0) || (timer_handle >= MAX_TIMERS)) {
        DEBUG_LOG("Invalid timer handle");
        return MQTT_Error_Invalid_Parameter;
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
        MQTTErrorCode ret = platform_cleanup_task(handle, p->timer_task);
        if (ret == MQTT_Error_Ok) {
            p->timer_task = -1;
        } else {
            DEBUG_LOG("Could not finish timer task");
            return ret;
        }
    }

    return MQTT_Error_Ok;
}


MQTTErrorCode platform_sleep(int milliseconds) {
    vTaskDelay(milliseconds / portTICK_PERIOD_MS);
    return MQTT_Error_Ok;
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
        DEBUG_LOG("     - %d, handle: %d", i, (int)handle->platform->tasks[i]);
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