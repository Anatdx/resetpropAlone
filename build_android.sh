#!/usr/bin/env bash
# Build resetprop for Android (NDK). Usage: ./build_android.sh [abi]
# Default ABI: arm64-v8a. Others: armeabi-v7a, x86_64, x86

set -e
ABI="${1:-arm64-v8a}"
NDK="${ANDROID_NDK:-${ANDROID_HOME}/ndk/$(ls ${ANDROID_HOME}/ndk 2>/dev/null | head -1)}"
if [ -z "$NDK" ] || [ ! -d "$NDK" ]; then
  echo "Set ANDROID_NDK or ANDROID_HOME with NDK installed."
  exit 1
fi

BUILD_DIR="build_android_${ABI}"
cmake -S . -B "$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI="$ABI" \
  -DANDROID_PLATFORM=android-21
cmake --build "$BUILD_DIR"
echo "Output: $BUILD_DIR/resetprop"
