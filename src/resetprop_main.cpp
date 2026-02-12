/**
 * Standalone resetprop — get/set/list Android system properties.
 * Uses bionic __system_property_* API. Build with Android NDK for device.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#if defined(RESETPROP_ANDROID)
#include <sys/system_properties.h>
#include <android/log.h>
#define LOG_TAG "resetprop"
#define LOGE(fmt, ...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, fmt, ##__VA_ARGS__)
#else
#define LOGE(fmt, ...) fprintf(stderr, "resetprop: " fmt "\n", ##__VA_ARGS__)
#endif

#define PROP_VALUE_MAX 92

static void usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s [NAME [VALUE]]\n"
        "  No args     List all properties (name=value).\n"
        "  NAME        Get property value (like getprop NAME).\n"
        "  NAME VALUE  Set property (like setprop NAME VALUE).\n"
        "Options:\n"
        "  -h, --help  Show this help.\n",
        prog);
}

#if defined(RESETPROP_ANDROID)

static int prop_get(const char* name, char* value) {
    const prop_info* pi = __system_property_find(name);
    if (!pi) return -1;
    struct Cb { char* out; bool done; };
    Cb cb{value, false};
    __system_property_read_callback(pi, [](void* cookie, const char*, const char* val, uint32_t) {
        Cb* c = static_cast<Cb*>(cookie);
        strncpy(c->out, val, PROP_VALUE_MAX - 1);
        c->out[PROP_VALUE_MAX - 1] = '\0';
        c->done = true;
    }, &cb);
    return cb.done ? (int)strlen(value) : -1;
}

static void list_cb(const prop_info* pi, void* cookie) {
    using vec_t = std::vector<std::pair<std::string, std::string>>;
    vec_t* out = static_cast<vec_t*>(cookie);
    __system_property_read_callback(pi, [](void* cookie2, const char* n, const char* v, uint32_t) {
        static_cast<vec_t*>(cookie2)->emplace_back(n, v);
    }, out);
}

int main(int argc, char** argv) {
    if (__system_properties_init() != 0) {
        LOGE("__system_properties_init failed");
        return 1;
    }

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            return 0;
        }
    }

    if (argc == 1) {
        // List all
        std::vector<std::pair<std::string, std::string>> props;
        if (__system_property_foreach(list_cb, &props) != 0) {
            LOGE("__system_property_foreach failed");
            return 1;
        }
        for (const auto& p : props)
            printf("[%s]: [%s]\n", p.first.c_str(), p.second.c_str());
        return 0;
    }

    if (argc == 2) {
        // Get one
        char value[PROP_VALUE_MAX];
        if (prop_get(argv[1], value) < 0) {
            LOGE("property not found: %s", argv[1]);
            return 1;
        }
        printf("%s\n", value);
        return 0;
    }

    if (argc == 3) {
        // Set
        if (__system_property_set(argv[1], argv[2]) != 0) {
            LOGE("__system_property_set failed");
            return 1;
        }
        return 0;
    }

    usage(argv[0]);
    return 1;
}

#else

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    LOGE("resetprop is for Android only. Build with Android NDK for device.");
    usage(argv[0]);
    return 1;
}

#endif
