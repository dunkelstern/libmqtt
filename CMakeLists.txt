cmake_minimum_required(VERSION 2.6)
project(LIBMQTT)

set (LIBMQTT_VERSION_MAJOR 0)
set (LIBMQTT_VERSION_MINOR 1)

set(CMAKE_CFLAGS "-g -Wall -Os -pthread")

if (UNIX)
    set(PLATFORM_CODE platform/linux.c)
    find_package(Threads REQUIRED)
    set(PLATFORM_LIBS ${CMAKE_THREAD_LIBS_INIT})
endif()

configure_file (
        "${PROJECT_SOURCE_DIR}/src/mqtt.h.in"
        "${PROJECT_BINARY_DIR}/mqtt.h"
)
include_directories("${PROJECT_BINARY_DIR}")
include_directories("${PROJECT_SOURCE_DIR}/platform")


set(mqtt-source src/debug.c src/mqtt.c src/packet.c src/protocol.c src/state_queue.c src/subscriptions.c ${PLATFORM_CODE})

add_library(mqtt-full STATIC ${mqtt-source})
    target_compile_definitions(mqtt-full PRIVATE DEBUG=1 MQTT_SERVER=1 MQTT_CLIENT=1)

add_library(mqtt-debug STATIC ${mqtt-source})
    target_compile_definitions(mqtt-full PRIVATE DEBUG=1 MQTT_SERVER=1 MQTT_CLIENT=1)

add_library(mqtt_static STATIC ${mqtt-source})
add_library(mqtt SHARED ${mqtt-source})


include_directories ("${PROJECT_SOURCE_DIR}/src")

add_executable (connect_publish.test tests/connect_publish.c)
    target_link_libraries (connect_publish.test mqtt-debug ${PLATFORM_LIBS})

add_executable (connect_subscribe.test tests/connect_subscribe.c)
    target_link_libraries (connect_subscribe.test mqtt-debug ${PLATFORM_LIBS})

add_executable (decode_packet.test tests/decode_packet.c)
    target_link_libraries (decode_packet.test mqtt-full ${PLATFORM_LIBS})

add_executable (encode_packet.test tests/encode_packet.c)
    target_link_libraries (encode_packet.test mqtt-full ${PLATFORM_LIBS})


