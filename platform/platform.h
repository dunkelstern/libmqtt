#ifndef platform_h__included
#define platform_h__included

#include "mqtt_internal.h"

typedef void (*Reader)(MQTTHandle *handle);

/** maximum receiver buffer size, defined by platform */
extern const size_t max_receive_buffer_size;


bool hostname_to_ip(char *hostname, char *ip);

/**
 * Initialize platform specific data
 * 
 * @param handle: The handle to initialize
 */
void initialize_platform(MQTTHandle *handle);

/**
 * Platform specific function to start a reading thread
 * 
 * @param handle: The broker connection handle
 * @param reader: callback to run in the thread
 */
MQTTStatus run_read_task(MQTTHandle *handle, Reader reader);

/**
 * Platform specific function to clean up the reading thread
 * 
 * @param handle: The broker connection handle
 */
MQTTStatus join_read_task(MQTTHandle *handle);

/**
 * Platform specific function to release resources associated with a MQTTHandle
 * 
 * @param handle: The handle to clean up
 */
void release_platform(MQTTHandle *handle);

#endif /* platform_h__included */
