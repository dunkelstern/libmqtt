#include<string.h>
#include<sys/socket.h>
#include<errno.h>
#include<netdb.h>
#include<arpa/inet.h>

#include <pthread.h>
#include <stdlib.h>

#include "platform.h"

const size_t max_receive_buffer_size = 4 * 4096; // 16 KB

struct _PlatformData {
    pthread_t read_thread;
};

void initialize_platform(MQTTHandle *handle) {
    handle->platform = calloc(1, sizeof(struct _PlatformData));
}

MQTTStatus run_read_task(MQTTHandle *handle, Reader reader) {
    if (pthread_create(&handle->platform->read_thread, NULL, (void *(*)(void *))reader, (void *)handle)) {
        return MQTT_STATUS_ERROR;
    }

    return MQTT_STATUS_OK;
}

MQTTStatus join_read_task(MQTTHandle *handle) {
    if (handle->platform->read_thread) {
        pthread_join(handle->platform->read_thread, NULL);
        handle->platform->read_thread = 0;
    }
    return MQTT_STATUS_OK;
}

void release_platform(MQTTHandle *handle) {
    free(handle->platform);
}


bool hostname_to_ip(char *hostname , char *ip) {
    struct addrinfo hints, *servinfo;
    struct sockaddr_in *h;
 
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC; // use AF_INET6 to force IPv6
    hints.ai_socktype = SOCK_STREAM;
 
    int ret = getaddrinfo(hostname, NULL, &hints, &servinfo);
    if (ret != 0) {
        // fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return false;
    }
 
    // loop through all the results and connect to the first we can
    for(struct addrinfo *p = servinfo; p != NULL; p = p->ai_next) {
        h = (struct sockaddr_in *)p->ai_addr;
        strcpy(ip , inet_ntoa( h->sin_addr ) );
    }
     
    freeaddrinfo(servinfo); // all done with this structure
    return true;
}
