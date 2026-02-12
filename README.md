# resetpropAlone

Standalone (or multi-call) **resetprop** — get/set/list Android system properties.

- **Entry**: `resetprop_main(int argc, char **argv)` for use when linked into ksud; define `RESETPROP_STANDALONE` for a standalone executable with `main()`.
- **Android**: Uses bionic `__system_property_*` API (list/get/set). Build with Android NDK for device; on non-Android host builds, prints "Android only" and exits.
- **Usage**: no args = list all; `NAME` = get; `NAME VALUE` = set.

## Build

### Android (NDK)

```bash
export ANDROID_NDK=/path/to/ndk
cmake -S . -B build_android \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-26
cmake --build build_android
```

(API 26+ for `__system_property_read_callback`.)

### Host (stub)

```bash
cmake -S . -B build && cmake --build build
```

Prints that resetprop is for Android only.
