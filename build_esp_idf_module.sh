#!/bin/bash

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"
DIST=dist/esp32

rm -rf "${DIST}"
mkdir -p "${DIST}/components/libmqtt"

pushd "${DIST}/components/libmqtt" > /dev/null

# copy files
echo "Copying files..."
cp "${DIR}"/src/*.c . || exit 1
cp "${DIR}"/src/*.h . || exit 1
cp "${DIR}/platform/platform.h" . || exit 1
cp "${DIR}/platform/esp32.c" platform.c || exit 1

# CMake file
echo "Creating CMakeLists.txt..."
cat >CMakeLists.txt <<EOF
set(COMPONENT_SRCS $(ls *.c|tr "\n" " "))
set(COMPONENT_ADD_INCLUDEDIRS ".")

register_component()
EOF

# Component config
echo "Creating Kconfig..."
cat >Kconfig <<EOF
config MQTT_SERVER
    bool "Enable the MQTT Server."
    default n
    help
        This enables server support in libmqtt.
config MQTT_CLIENT
    bool "Enable the MQTT Client."
    default y
    help
        This enables client support in libmqtt.
EOF

popd > /dev/null

echo "Creating package..."
pushd "$DIST" > /dev/null
tar czf ../esp32_component.tar.gz components || exit 1
popd > /dev/null

echo "Cleaning up..."
# rm -rf "$DIST"

echo
echo "=> Find your component in ${DIST}/esp32_component.tar.gz"
echo 