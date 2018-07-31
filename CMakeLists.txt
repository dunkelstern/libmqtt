cmake_minimum_required(VERSION 2.6)
project(LIBMQTT)

set (LIBMQTT_VERSION_MAJOR 0)
set (LIBMQTT_VERSION_MINOR 1)

set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -fprofile-arcs -ftest-coverage -O0 -pthread -DDEBUG=1")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wall -Os -pthread")

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


set(mqtt-source src/mqtt.c src/packet.c src/protocol.c src/state_queue.c src/subscriptions.c ${PLATFORM_CODE})

add_library(mqtt-full STATIC ${mqtt-source})
    target_compile_definitions(mqtt-full PRIVATE MQTT_SERVER=1 MQTT_CLIENT=1)

add_library(mqtt_static STATIC ${mqtt-source})
add_library(mqtt SHARED ${mqtt-source})


include_directories ("${PROJECT_SOURCE_DIR}/src")

add_executable (connect_publish.test tests/connect_publish.c)
    target_link_libraries (connect_publish.test mqtt-full ${PLATFORM_LIBS})

add_executable (connect_subscribe.test tests/connect_subscribe.c)
    target_link_libraries (connect_subscribe.test mqtt-full ${PLATFORM_LIBS})

add_executable (decode_packet.test tests/decode_packet.c)
    target_link_libraries (decode_packet.test mqtt-full ${PLATFORM_LIBS})

add_executable (encode_packet.test tests/encode_packet.c)
    target_link_libraries (encode_packet.test mqtt-full ${PLATFORM_LIBS})


