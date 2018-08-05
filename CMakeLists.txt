cmake_minimum_required(VERSION 3.0)
project(LIBMQTT)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMakeModules)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_C_STANDARD 99)

#
# Version
#
set (LIBMQTT_VERSION_MAJOR 0)
set (LIBMQTT_VERSION_MINOR 1)

configure_file (
    "${PROJECT_SOURCE_DIR}/src/mqtt.h.in"
    "${PROJECT_SOURCE_DIR}/src/mqtt.h"
)

#
# Build flags
#
if (UNIX)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -Wextra -Wno-unused-parameter -fprofile-arcs -ftest-coverage -O0 -pthread -DDEBUG=1")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Wall -Wno-unused-parameter -fvisibility=hidden -Os -pthread")
    set(CMAKE_EXE_LINKER_FLAGS_DEBUG "-lgcov")
endif() # UNIX

if (MSVC)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -DDEBUG=1 -DMSVC -Dstrdup=_strdup /std:c++latest")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -DMSVC -Dstrdup=_strdup /std:c++latest")
endif()

#
# Platform abstraction
#
if (MSVC)
    set(PLATFORM_CODE 
        platform/windows.c
        platform/platform.h
    )
    find_package(Threads REQUIRED)
    set(PLATFORM_LIBS wsock32 ws2_32 ${CMAKE_THREAD_LIBS_INIT})
endif()

if (UNIX)
    set(PLATFORM_CODE 
        platform/linux.c
        platform/platform.h
    )
    find_package(Threads REQUIRED)
    set(PLATFORM_LIBS ${CMAKE_THREAD_LIBS_INIT})
endif()

# 
# Source files
# 
set(mqtt-source
    src/mqtt.c
    src/mqtt_internal.h
    src/packet.c
    src/packet.h
    src/protocol.c
    src/protocol.h
    src/state_queue.c
    src/state_queue.h
    src/subscriptions.c
    src/subscriptions.h
    src/debug.h
    src/buffer.h
    src/mqtt.h
    ${PLATFORM_CODE}
)

# full build library for testing
add_library(mqtt-full STATIC ${mqtt-source})
    target_compile_definitions(mqtt-full PRIVATE MQTT_SERVER=1 MQTT_CLIENT=1 KEEPALIVE_INTERVAL=4)
    set_target_properties(mqtt-full PROPERTIES PUBLIC_HEADER "src/mqtt.h")
    target_include_directories(mqtt-full
        PUBLIC 
            $<INSTALL_INTERFACE:include>    
            $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/platform
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

# libraries that could be deployed
add_library(mqtt_static STATIC ${mqtt-source})
    set_target_properties(mqtt_static PROPERTIES PUBLIC_HEADER "src/mqtt.h")
    target_include_directories(mqtt_static
        PUBLIC 
            $<INSTALL_INTERFACE:include>    
            $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/platform
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

add_library(mqtt SHARED ${mqtt-source})
    set_target_properties(mqtt PROPERTIES
        PUBLIC_HEADER "src/mqtt.h"
        VERSION "${LIBMQTT_VERSION_MAJOR}.${LIBMQTT_VERSION_MINOR}"
        SOVERSION ${LIBMQTT_VERSION_MAJOR}
    )
    target_link_libraries(mqtt ${PLATFORM_LIBS})
    target_include_directories(mqtt
        PUBLIC 
            $<INSTALL_INTERFACE:include>    
            $<BUILD_INTERFACE:${PROJECT_BINARY_DIR}>
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/platform
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )

# Test executables
add_executable (connect_publish.test tests/connect_publish.c)
    target_link_libraries (connect_publish.test mqtt-full ${PLATFORM_LIBS})
    target_include_directories(connect_publish.test
        PRIVATE
            ${PROJECT_BINARY_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/platform
    )
    add_test(NAME ConnectPublish COMMAND ${PROJECT_BINARY_DIR}/connect_publish.test)

add_executable (connect_subscribe.test tests/connect_subscribe.c)
    target_link_libraries (connect_subscribe.test mqtt-full ${PLATFORM_LIBS})
    target_include_directories(connect_subscribe.test
        PRIVATE
            ${PROJECT_BINARY_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/platform
    )
    add_test(NAME ConnectSubscribe COMMAND ${PROJECT_BINARY_DIR}/connect_subscribe.test)

add_executable (connect_reconnect.test tests/connect_reconnect.c)
    target_link_libraries (connect_reconnect.test mqtt-full ${PLATFORM_LIBS})
    target_include_directories(connect_reconnect.test
        PRIVATE
            ${PROJECT_BINARY_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/platform
    )
    add_test(NAME ConnectReconnect COMMAND ${PROJECT_BINARY_DIR}/connect_reconnect.test)

add_executable (decode_packet.test tests/decode_packet.c)
    target_link_libraries (decode_packet.test mqtt-full ${PLATFORM_LIBS})
    target_include_directories(decode_packet.test
        PRIVATE
            ${PROJECT_BINARY_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME DecodePacket COMMAND ${PROJECT_BINARY_DIR}/decode_packet.test)

add_executable (encode_packet.test tests/encode_packet.c)
    target_link_libraries (encode_packet.test mqtt-full ${PLATFORM_LIBS})
    target_include_directories(encode_packet.test
        PRIVATE
            ${PROJECT_BINARY_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    add_test(NAME EncodePacket COMMAND ${PROJECT_BINARY_DIR}/encode_packet.test)

include(CTest)
enable_testing()

# check target, builds and runs all tests as the 'test' target is not able to build
add_custom_target(check COMMAND ${CMAKE_CTEST_COMMAND} DEPENDS 
    connect_publish.test
    connect_subscribe.test
    decode_packet.test
    encode_packet.test
)

# code coverage target
if(CMAKE_COMPILER_IS_GNUCXX)
    include(CodeCoverage)
    setup_target_for_coverage(coverage check coverage)
endif()

#
# Install
#

install(TARGETS mqtt mqtt_static
        RUNTIME DESTINATION bin
        PUBLIC_HEADER DESTINATION include/mqtt
        LIBRARY DESTINATION lib CONFIGURATIONS Release
        ARCHIVE DESTINATION lib CONFIGURATIONS Release)
