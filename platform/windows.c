#include <windows.h>
//#include <winsock2.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>

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
    HANDLE tasks[MAX_TASKS];
    PlatformTimer timers[MAX_TIMERS];
    WSADATA wsa;
    SOCKET sock;
    int timer_task;
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
            return 0;
        }
    }
}

PlatformStatusCode platform_init(MQTTHandle *handle) {
    handle->platform = (PlatformData *)calloc(1, sizeof(struct _PlatformData));
    handle->platform->sock = INVALID_SOCKET;
    handle->platform->timer_task = -1;
    if (!handle->platform) {
        return PlatformStatusError;
    }

    if (WSAStartup(MAKEWORD(2,2),&handle->platform->wsa) != 0) {
        DEBUG_LOG("Winsock init failed. Error Code : %d", WSAGetLastError());
        return PlatformStatusError;
    }

    return PlatformStatusOk;
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

    DWORD thread_id;
    p->tasks[free_task] = CreateThread( 
            NULL,                   // default security attributes
            0,                      // use default stack size  
            callback,               // thread function name
            handle,                 // argument to thread function 
            0,                      // use default creation flags 
            &thread_id);            // returns the thread identifier 

    if (p->tasks[free_task] == NULL) {
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
        HANDLE wait[] = { p->tasks[task_handle] };
        WaitForMultipleObjects(1, wait, TRUE, INFINITE);
        CloseHandle(p->tasks[task_handle]);
        p->tasks[task_handle] = 0;
    }
    return PlatformStatusOk;
}

PlatformStatusCode platform_resolve_host(char *hostname , char *ip) {
    struct in_addr **addr_list;
    int i;
          
    struct hostent *he = gethostbyname(hostname);
    if (he == NULL) {
        DEBUG_LOG("Resolving host failed: %d", WSAGetLastError());
        return PlatformStatusError;
    }
     
    addr_list = (struct in_addr **) he->h_addr_list;
     
    for(i = 0; addr_list[i] != NULL; i++) {
        strncpy_s(ip, 40, inet_ntoa(*addr_list[i]), 40);
        break;
    }

    return PlatformStatusOk;
}

PlatformStatusCode platform_connect(MQTTHandle *handle) {
    PlatformData *p = handle->platform;

    int ret;
    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));

    p->sock = socket(AF_INET, SOCK_STREAM, 0);
    if (p->sock == INVALID_SOCKET) {
        bool free_handle = handle->error_handler(handle, handle->config, MQTT_Error_Internal);
        if (free_handle) {
            mqtt_free(handle);
        }
        char err_msg[200];
        strerror_s(err_msg, 200, errno);
        DEBUG_LOG("Resolving hostname failed: %s", err_msg);
        return PlatformStatusError;
    }
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(handle->config->port);

    char ip[40];
    if (platform_resolve_host(handle->config->hostname, ip) != PlatformStatusOk) {
        bool free_handle = handle->error_handler(handle, handle->config, MQTT_Error_Host_Not_Found);
        if (free_handle) {
            mqtt_free(handle);
        }
        char err_msg[200];
        strerror_s(err_msg, 200, errno);
        DEBUG_LOG("Resolving hostname failed: %s", err_msg);
		closesocket(p->sock);
        return PlatformStatusError;
    }
    servaddr.sin_addr.s_addr = inet_addr(ip);
 
    ret = connect(p->sock, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (ret != 0) {
        bool free_handle = handle->error_handler(handle, handle->config, MQTT_Error_Connection_Refused);
        if (free_handle) {
            mqtt_free(handle);
        }
        char err_msg[200];
        strerror_s(err_msg, 200, errno);
        DEBUG_LOG("Connection failed: %s", err_msg);
		closesocket(p->sock);
        return PlatformStatusError;
    }

    return PlatformStatusOk;
}

PlatformStatusCode platform_read(MQTTHandle *handle, Buffer *buffer) {
    PlatformData *p = handle->platform;

    while (1) {
        int num_bytes = recv(p->sock, &buffer->data[buffer->position], (int)buffer_free_space(buffer), 0);
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
        int bytes = send(p->sock, buffer->data + buffer->position, (int)buffer_free_space(buffer), 0);
        if (bytes <= 0) {
            return PlatformStatusError;
        }
        buffer->position += bytes;
    }

    return PlatformStatusOk;
}

PlatformStatusCode platform_disconnect(MQTTHandle *handle) {
    PlatformData *p = handle->platform;
    if (p->sock != INVALID_SOCKET) {
        closesocket(p->sock);
        WSACleanup();
        p->sock = INVALID_SOCKET;
    }

    return PlatformStatusOk;
}

PlatformStatusCode platform_create_timer(MQTTHandle *handle, int interval, int *timer_handle, PlatformTimerCallback callback) {
    PlatformData *p = handle->platform;
    uint8_t free_timer = 0;

    for (free_timer = 0; free_timer < MAX_TIMERS; free_timer++) {
        DEBUG_LOG("Timer %d: %s", free_timer, p->timers[free_timer].callback ? "Occupied" : "Free");
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
    HANDLE timer; 
    LARGE_INTEGER ft; 

    // Convert to 100 nanosecond interval, negative value indicates relative time
    ft.QuadPart = -(10 * milliseconds * 1000); 

    timer = CreateWaitableTimer(NULL, TRUE, NULL); 
    SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0); 
    WaitForSingleObject(timer, INFINITE); 
    CloseHandle(timer); 

    return PlatformStatusOk;
}