#!/bin/bash
# Build micro-ROS library for ESP32-S3 (xtensa lx7)
# 由原版 build_microros.sh (早期 riscv32) 改造而来
# Uses PlatformIO's ESP-IDF toolchain (xtensa-esp32s3)

set -e

PIO_PACKAGES=~/.platformio/packages
TOOLCHAIN=$PIO_PACKAGES/toolchain-xtensa-esp32s3
IDF_PATH=$PIO_PACKAGES/framework-arduinoespressif32/tools/sdk/esp32s3
TARGET=xtensa-esp32s3-elf
MCU_WS=/tmp/firmware/mcu_ws
BUILD_DIR=/tmp/firmware/build_esp32s3
INSTALL_DIR=$BUILD_DIR/install

export PATH=$TOOLCHAIN/bin:$PATH
export CC=$TARGET-gcc
export CXX=$TARGET-g++
export AR=$TARGET-ar
export LD=$TARGET-ld
export OBJCOPY=$TARGET-objcopy
export STRIP=$TARGET-strip

# FreeRTOS + IDF compile flags (与原版结构相同, 路径换成 esp32s3)
FREERTOS_INC="$IDF_PATH/include/freertos/include"
LWIP_INC="$IDF_PATH/include/lwip/include"
HAL_INC="$IDF_PATH/include/hal/include"
SOC_INC="$IDF_PATH/include/soc/include"
NEWLIB_INC="$TOOLCHAIN/xtensa-esp32s3-elf/include"
CONFIG_INC="$IDF_PATH/include/config"

# ESP32-S3 是 Xtensa LX7: 需要 -mlongcalls; 无需 riscv32 的 march/mabi
CFLAGS="-Os -ffunction-sections -fdata-sections -mlongcalls"
CFLAGS="$CFLAGS -D__ESP32S3__ -DESP_PLATFORM"
CFLAGS="$CFLAGS -I$FREERTOS_INC -I$IDF_PATH/include -I$CONFIG_INC"
CFLAGS="$CFLAGS -I$HAL_INC -I$SOC_INC -I$NEWLIB_INC"

CXXFLAGS="$CFLAGS -fno-rtti -fno-exceptions"

# Use colcon - need ROS2 and dev_ws for build tools (ament_cmake, etc.)
source /opt/ros/humble/setup.bash
source /tmp/firmware/dev_ws/install/setup.bash 2>/dev/null || true

cd $MCU_WS

# We only need the client libs, skip tests and examples
colcon build \
    --merge-install \
    --install-base $INSTALL_DIR \
    --cmake-args \
        -DCMAKE_SYSTEM_NAME=Generic \
        -DCMAKE_SYSTEM_PROCESSOR=xtensa \
        -DCMAKE_C_COMPILER=$CC \
        -DCMAKE_CXX_COMPILER=$CXX \
        -DCMAKE_C_FLAGS="$CFLAGS" \
        -DCMAKE_CXX_FLAGS="$CXXFLAGS" \
        -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
        -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
        -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
        -DCMAKE_PREFIX_PATH="$INSTALL_DIR;/opt/ros/humble" \
        -DCMAKE_INSTALL_PREFIX=$INSTALL_DIR \
        -DBUILD_TESTING=OFF \
        -DRCUTILS_NO_FILESYSTEM=ON \
        -DRCUTILS_NO_THREAD_SUPPORT=ON \
        -DRCUTILS_NO_64_ATOMIC=ON \
        -DUCLIENT_PROFILE_CUSTOM_TRANSPORT=ON \
        -DUCLIENT_PROFILE_UDP=OFF \
        -DUCLIENT_PROFILE_SERIAL=OFF \
        -DUCLIENT_PROFILE_TCP=OFF \
        -DRMW_UXRCE_TRANSPORT=custom \
    --continue-on-error \
    --packages-up-to \
        rmw_microxrcedds rclc sensor_msgs std_msgs geometry_msgs \
    --metas colcon.meta 2>&1 | tail -50

echo "=== Done ==="
echo "Libraries in: $INSTALL_DIR/lib/"
find $INSTALL_DIR -name "*.a" 2>/dev/null

echo
echo "下一步: 把安装产物拷进固件工程"
echo "  cp $INSTALL_DIR/lib/libmicroros.a <工程>/firmware/nanocam_soarm/lib/microros/libmicroros.a"
echo "  rsync -a --delete $INSTALL_DIR/include/ <工程>/firmware/nanocam_soarm/lib/microros/include/"
