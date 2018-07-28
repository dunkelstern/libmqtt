#ifndef mqtt_h__included
#define mqtt_h__included

#include <stdint.h>
#include <stdbool.h>

typedef struct _MQTTHandle MQTTHandle;

typedef struct {
    char *hostname; /**< Hostname to connect to, will do DNS resolution */
    uint16_t port;  /**< Port the broker listens on, set to 0 for 1883 default */

    char *clientID; /**< Client identification */

    char *username; /**< User name, set to NULL to connect anonymously */
    char *password; /**< Password, set to NULL to connect without password */
} MQTTConfig;

typedef enum {
    MQTT_STATUS_OK = 0, /**< All ok, no error */
    MQTT_STATUS_ERROR   /**< Error, action did not succeed, error handler will be called */
} MQTTStatus;

typedef enum {
    MQTT_QOS_0 = 0, /**< At most once, drop message if nobody is listening, no ACK */
    MQTT_QOS_1,     /**< At least once, wait for ACK from broker */
    MQTT_QOS_2,     /**< Exactly once, do triple way handshake with broker */
} MQTTQosLevel;

typedef enum {
    MQTT_Error_Host_Not_Found,         /**< Host could not be resolved */
    MQTT_Error_Connection_Refused,     /**< Connection was refused, wrong port? */
    MQTT_Error_Broker_Disconnected,    /**< Broker went down, perhaps restart? */
    MQTT_Error_Authentication,         /**< Authentication error, wrong or missing username/password? */
    MQTT_Error_Protocol_Not_Supported, /**< Broker does not speak MQTT protocol 3.1.1 (aka version 4) */
    MQTT_Error_Connection_Reset        /**< Network connection reset, perhaps network went down? */
} MQTTErrorCode;

/** Error handler callback */
typedef void (*MQTTErrorHandler)(MQTTHandle *handle, MQTTErrorCode code);

/** Event handler callback */
typedef void (*MQTTEventHandler)(MQTTHandle *handle, char *topic, char *payload);

/**
 * Connect to MQTT broker
 * 
 * @param config: MQTT configuration
 * @param callback: Callback function to call on errors
 * @returns handle to mqtt connection or NULL on error
 */
MQTTHandle *mqtt_connect(MQTTConfig *config, MQTTErrorHandler callback);

/**
 * Re-Connect to MQTT broker
 * 
 * Usually called in the MQTTErrorHandler callback, if called on a working
 * connection the connection will be disconnected before reconnecting.
 * 
 * If there were registered subscriptions they will be re-instated after
 * a successful reconnect.
 * 
 * @param handle: MQTT Handle from `mqtt_connect`
 * @returns: Status code
 */
MQTTStatus mqtt_reconnect(MQTTHandle *handle);

/**
 * Subscribe to a topic
 * 
 * @param handle: MQTT Handle from `mqtt_connect`
 * @param topic: Topic to subscribe
 * @param callback: Callback function to call when receiving something for that topic
 * @returns: Status code
 */
MQTTStatus mqtt_subscribe(MQTTHandle *handle, char *topic, MQTTEventHandler callback);

/**
 * Un-Subscribe from a topic
 * 
 * @param handle: MQTT Handle from `mqtt_connect`
 * @param topic: Topic to unsubscribe
 * @returns: Status code
 */
MQTTStatus mqtt_unsubscribe(MQTTHandle *handle, char *topic);

/**
 * Publish something to the broker
 * 
 * @param handle: MQTT Handle from `mqtt_connect`
 * @param topic: Topic to publish to
 * @param payload: Message payload to publish
 * @returns: Status code
 */
MQTTStatus mqtt_publish(MQTTHandle *handle, char *topic, char *payload, MQTTQosLevel qos_level);

/**
 * Disconnect from MQTT broker
 * 
 * @param handle: MQTT Handle from `mqtt_connect`
 * @returns: Status code
 * 
 * @attention: do not use the handle after calling this function,
 *             all resources will be freed, this handle is now invalid!
 */
MQTTStatus mqtt_disconnect(MQTTHandle *handle);

#endif /* mqtt_h__included */
