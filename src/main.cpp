/*
 * resetprop - get/set/list Android system properties.
 * Uses bionic __system_property_* API on Android.
 * Compatible with API 21+ (uses __system_property_read on API < 26).
 *
 * Copyright (C) Magisk (original resetprop)
 * Copyright (C) YukiSU - standalone C++ implementation
 *
 * Licensed under the Apache License, Version 2.0.
 */

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#if defined(__ANDROID__)
#include <sys/system_properties.h>
#endif

namespace {

static void usage(std::ostream& out, const char* prog) {
    out << "Usage: " << prog << " [NAME [VALUE]]\n"
        << "  No args     List all properties (name=value).\n"
        << "  NAME        Get property value (like getprop NAME).\n"
        << "  NAME VALUE  Set property (like setprop NAME VALUE).\n"
        << "Options:\n"
        << "  -h, --help  Show this help.\n";
}

#if defined(__ANDROID__)

#if __ANDROID_API__ >= 26

struct GetCookie {
    char* out;
    bool done;
};

static void get_callback(void* cookie, const char* /*name*/, const char* value, uint32_t /*serial*/) {
    auto* c = static_cast<GetCookie*>(cookie);
    if (value) {
        strncpy(c->out, value, PROP_VALUE_MAX - 1);
        c->out[PROP_VALUE_MAX - 1] = '\0';
    }
    c->done = true;
}

static int prop_get(const char* name, char* value) {
    const prop_info* pi = __system_property_find(name);
    if (!pi) return -1;
    GetCookie cb{value, false};
    __system_property_read_callback(pi, get_callback, &cb);
    return cb.done ? static_cast<int>(strlen(value)) : -1;
}

static void list_read_callback(void* cookie, const char* name, const char* value, uint32_t /*serial*/) {
    auto* out = static_cast<std::vector<std::pair<std::string, std::string>>*>(cookie);
    out->emplace_back(name ? name : "", value ? value : "");
}

static void list_foreach_callback(const prop_info* pi, void* cookie) {
    auto* out = static_cast<std::vector<std::pair<std::string, std::string>>*>(cookie);
    __system_property_read_callback(pi, list_read_callback, out);
}

#else  // API < 26: use deprecated __system_property_read

static int prop_get(const char* name, char* value) {
    const prop_info* pi = __system_property_find(name);
    if (!pi) return -1;
    char name_buf[PROP_NAME_MAX];
    int ret = __system_property_read(pi, name_buf, value);
    return ret > 0 ? ret : -1;
}

static void list_foreach_callback(const prop_info* pi, void* cookie) {
    auto* out = static_cast<std::vector<std::pair<std::string, std::string>>*>(cookie);
    char name_buf[PROP_NAME_MAX];
    char value_buf[PROP_VALUE_MAX];
    if (__system_property_read(pi, name_buf, value_buf) > 0)
        out->emplace_back(name_buf, value_buf);
}

#endif  // __ANDROID_API__ >= 26

#endif  // __ANDROID__

}  // namespace

extern "C" {

int resetprop_main(int argc, char** argv) {
    const char* prog = (argv && argv[0]) ? argv[0] : "resetprop";

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(std::cerr, prog);
            return 0;
        }
    }

#if !defined(__ANDROID__)
    std::cerr << "resetprop: Android only. Build with NDK for device.\n";
    usage(std::cerr, prog);
    return 1;
#else

#if __ANDROID_API__ >= 26
    if (__system_properties_init() != 0) {
        std::cerr << "resetprop: __system_properties_init failed\n";
        return 1;
    }
#endif

    if (argc == 1) {
        std::vector<std::pair<std::string, std::string>> props;
        if (__system_property_foreach(list_foreach_callback, &props) != 0) {
            std::cerr << "resetprop: __system_property_foreach failed\n";
            return 1;
        }
        for (const auto& p : props)
            std::cout << "[" << p.first << "]: [" << p.second << "]\n";
        return 0;
    }

    if (argc == 2) {
        char value[PROP_VALUE_MAX];
        if (prop_get(argv[1], value) < 0) {
            std::cerr << "resetprop: property not found: " << argv[1] << "\n";
            return 1;
        }
        std::cout << value << "\n";
        return 0;
    }

    if (argc == 3) {
        if (__system_property_set(argv[1], argv[2]) != 0) {
            std::cerr << "resetprop: __system_property_set failed\n";
            return 1;
        }
        return 0;
    }

    usage(std::cerr, prog);
    return 1;
#endif
}

}  // extern "C"

#if defined(RESETPROP_STANDALONE)
int main(int argc, char** argv) {
    return resetprop_main(argc, argv);
}
#endif
