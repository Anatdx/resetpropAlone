# resetpropAlone

Standalone **resetprop** — get/set/list Android system properties.  
CMake-only, no Magisk/Android.mk. Intended for use on device (build with Android NDK).

## Features

- **List** all properties: `resetprop` (no args)
- **Get** one property: `resetprop NAME`
- **Set** property: `resetprop NAME VALUE`

Uses bionic `__system_property_*` API. Meaningful only on **Android** (NDK build). Host build produces a stub that prints usage.

## Requirements

- CMake ≥ 3.20
- C++17
- **Android NDK** for device binary (no extra libs; links against bionic)

## Build

### Android (NDK)

```bash
export ANDROID_NDK=/path/to/ndk
cmake -S . -B build_android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-21
cmake --build build_android
```

Binary: `build_android/resetprop`. Push to device and run (e.g. `adb push build_android/resetprop /data/local/tmp`).

Other ABIs: `-DANDROID_ABI=armeabi-v7a`, `x86_64`, `x86`.

### Host (stub only)

```bash
cmake -S . -B build && cmake --build build
```

Prints that resetprop is for Android only.

## Usage

```bash
resetprop                    # list all [name]: [value]
resetprop ro.build.version.sdk   # get one (prints value)
resetprop my.prop "value"    # set (requires root for many props)
```

## Project layout

```
.
├── CMakeLists.txt
├── README.md
├── LICENSE
└── src/
    └── resetprop_main.cpp   # CLI + get/set/foreach
```

## Origin and license

- Logic aligned with **Magisk**’s resetprop (getprop/setprop style). This repo is a standalone, dependency-minimal build for projects (e.g. [YukiSU](https://github.com/YukiSU)) that need only the resetprop binary.
- **License**: GPL-3.0 (see [LICENSE](LICENSE)).
