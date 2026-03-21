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

#include <chrono>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/mman.h>
#include <thread>
#include <unistd.h>
#include <utility>
#include <vector>

#if defined(__ANDROID__)
#include <sys/system_properties.h>
#endif

namespace {

static void usage(std::ostream& out, const char* prog) {
    out << "Usage: " << prog << " [-n] [NAME [VALUE]]\n"
        << "  No args     List all properties (name=value).\n"
        << "  NAME        Get property value (like getprop NAME).\n"
        << "  NAME VALUE  Set property (like setprop NAME VALUE).\n"
        << "Options:\n"
        << "  -n          Accept compatibility mode and skip init-side effects.\n"
        << "  -w NAME VAL Wait until property NAME equals VAL.\n"
        << "  -h, --help  Show this help.\n";
}

#if defined(__ANDROID__)

#if defined(__ANDROID_API__) && __ANDROID_API__ >= 26

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

#endif  // defined(__ANDROID_API__) && __ANDROID_API__ >= 26

constexpr const char* kPropertyDir = PROP_DIRNAME;
constexpr mode_t kWritableMode = 0644;
constexpr mode_t kReadonlyMode = 0444;
constexpr auto kWaitPollInterval = std::chrono::milliseconds(200);

struct PropertyMapping {
    void* start = nullptr;
    size_t length = 0;
    off_t offset = 0;
    int prot = PROT_READ;
    std::string path;
};

static bool chmod_property_tree(const std::string& path, mode_t mode) {
    DIR* dir = opendir(path.c_str());
    if (dir == nullptr) {
        std::cerr << "resetprop: opendir failed for " << path << ": " << strerror(errno) << "\n";
        return false;
    }

    bool ok = true;
    while (true) {
        errno = 0;
        dirent* entry = readdir(dir);
        if (entry == nullptr) {
            if (errno != 0) {
                ok = false;
                std::cerr << "resetprop: readdir failed for " << path << ": " << strerror(errno)
                          << "\n";
            }
            break;
        }

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        const std::string child = path + "/" + entry->d_name;
        struct stat st {};
        if (lstat(child.c_str(), &st) != 0) {
            ok = false;
            std::cerr << "resetprop: lstat failed for " << child << ": " << strerror(errno)
                      << "\n";
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            if (!chmod_property_tree(child, mode)) {
                ok = false;
            }
            continue;
        }

        if (!S_ISREG(st.st_mode)) {
            continue;
        }

        if (chmod(child.c_str(), mode) != 0) {
            ok = false;
            std::cerr << "resetprop: chmod failed for " << child << ": " << strerror(errno)
                      << "\n";
        }
    }

    closedir(dir);
    return ok;
}

static bool parse_mapping_permissions(const std::string& perms, int* prot) {
    if (perms.size() < 4) {
        return false;
    }

    int value = PROT_NONE;
    if (perms[0] == 'r') {
        value |= PROT_READ;
    }
    if (perms[1] == 'w') {
        value |= PROT_WRITE;
    }
    if (perms[2] == 'x') {
        value |= PROT_EXEC;
    }
    *prot = value;
    return true;
}

static bool collect_property_mappings(std::vector<PropertyMapping>* mappings) {
    FILE* maps = fopen("/proc/self/maps", "re");
    if (maps == nullptr) {
        std::cerr << "resetprop: failed to open /proc/self/maps: " << strerror(errno) << "\n";
        return false;
    }

    bool ok = true;
    char* line = nullptr;
    size_t line_cap = 0;
    while (getline(&line, &line_cap, maps) != -1) {
        unsigned long start = 0;
        unsigned long end = 0;
        unsigned long offset = 0;
        char perms[5] = {};
        unsigned int major = 0;
        unsigned int minor = 0;
        unsigned long inode = 0;
        int consumed = 0;
        if (sscanf(line, "%lx-%lx %4s %lx %x:%x %lu %n", &start, &end, perms, &offset, &major,
                   &minor, &inode, &consumed) < 7) {
            continue;
        }

        const char* path_ptr = line + consumed;
        while (*path_ptr == ' ' || *path_ptr == '\t') {
            ++path_ptr;
        }

        std::string path(path_ptr);
        while (!path.empty() && (path.back() == '\n' || path.back() == '\r')) {
            path.pop_back();
        }

        if (path.rfind(kPropertyDir, 0) != 0) {
            continue;
        }

        int prot = PROT_NONE;
        if (!parse_mapping_permissions(perms, &prot)) {
            ok = false;
            std::cerr << "resetprop: failed to parse mapping perms for " << path << "\n";
            continue;
        }

        PropertyMapping mapping;
        mapping.start = reinterpret_cast<void*>(start);
        mapping.length = static_cast<size_t>(end - start);
        mapping.offset = static_cast<off_t>(offset);
        mapping.prot = prot;
        mapping.path = std::move(path);
        mappings->push_back(std::move(mapping));
    }

    free(line);
    fclose(maps);
    return ok;
}

static bool remap_property_mapping(const PropertyMapping& mapping, int target_prot, int open_flags) {
    const int fd = open(mapping.path.c_str(), open_flags | O_CLOEXEC);
    if (fd < 0) {
        std::cerr << "resetprop: open failed for " << mapping.path << ": " << strerror(errno)
                  << "\n";
        return false;
    }

    void* result =
        mmap(mapping.start, mapping.length, target_prot, MAP_SHARED | MAP_FIXED, fd, mapping.offset);
    const int saved_errno = errno;
    close(fd);

    if (result != mapping.start) {
        std::cerr << "resetprop: mmap remap failed for " << mapping.path << ": "
                  << strerror(saved_errno) << "\n";
        return false;
    }

    return true;
}

class ScopedPropertyWriteAccess {
public:
    ScopedPropertyWriteAccess() {
        active_ = chmod_property_tree(kPropertyDir, kWritableMode) &&
                  collect_property_mappings(&mappings_) && remap_writable();
    }

    ScopedPropertyWriteAccess(const ScopedPropertyWriteAccess&) = delete;
    ScopedPropertyWriteAccess& operator=(const ScopedPropertyWriteAccess&) = delete;

    ~ScopedPropertyWriteAccess() {
        if (active_) {
            (void)remap_original();
            (void)chmod_property_tree(kPropertyDir, kReadonlyMode);
        }
    }

    bool active() const { return active_; }

private:
    bool remap_writable() {
        bool ok = true;
        for (const auto& mapping : mappings_) {
            if (!remap_property_mapping(mapping, mapping.prot | PROT_WRITE, O_RDWR)) {
                ok = false;
            }
        }
        return ok;
    }

    bool remap_original() const {
        bool ok = true;
        for (const auto& mapping : mappings_) {
            if (!remap_property_mapping(mapping, mapping.prot, O_RDONLY)) {
                ok = false;
            }
        }
        return ok;
    }

    std::vector<PropertyMapping> mappings_;
    bool active_ = false;
};

using SystemPropertyAddFn = int (*)(const char*, unsigned int, const char*, unsigned int);
using SystemPropertyUpdateFn = int (*)(prop_info*, const char*, unsigned int);

static bool resolve_property_mutators(SystemPropertyUpdateFn* update_fn, SystemPropertyAddFn* add_fn) {
    static bool resolved = false;
    static SystemPropertyUpdateFn cached_update = nullptr;
    static SystemPropertyAddFn cached_add = nullptr;

    if (!resolved) {
        cached_update =
            reinterpret_cast<SystemPropertyUpdateFn>(dlsym(RTLD_DEFAULT, "__system_property_update"));
        cached_add =
            reinterpret_cast<SystemPropertyAddFn>(dlsym(RTLD_DEFAULT, "__system_property_add"));
        resolved = true;
    }

    *update_fn = cached_update;
    *add_fn = cached_add;
    return cached_update != nullptr && cached_add != nullptr;
}

static bool set_property_direct(const char* name, const char* value) {
    SystemPropertyUpdateFn update_fn = nullptr;
    SystemPropertyAddFn add_fn = nullptr;
    if (!resolve_property_mutators(&update_fn, &add_fn)) {
        std::cerr << "resetprop: property mutator symbols are unavailable at runtime\n";
        return false;
    }

    const prop_info* pi = __system_property_find(name);
    if (pi != nullptr) {
        auto* mutable_pi = const_cast<prop_info*>(pi);
        return update_fn(mutable_pi, value, strlen(value)) == 0;
    }
    return add_fn(name, strlen(name), value, strlen(value)) == 0;
}

static bool wait_for_property_value(const char* name, const char* expected) {
    while (true) {
        char value[PROP_VALUE_MAX] = {};
        if (prop_get(name, value) >= 0 && strcmp(value, expected) == 0) {
            return true;
        }
        std::this_thread::sleep_for(kWaitPollInterval);
    }
}

#endif  // __ANDROID__

}  // namespace

extern "C" {

/* Stub for NDK link; device libc may provide the real symbol at runtime. */
int __system_properties_init(void) { return 0; }

int resetprop_main(int argc, char** argv) {
    const char* prog = (argv && argv[0]) ? argv[0] : "resetprop";

#if !defined(__ANDROID__)
    std::cerr << "resetprop: Android only. Build with NDK for device.\n";
    usage(std::cerr, prog);
    return 1;
#else

#if defined(__ANDROID_API__) && __ANDROID_API__ >= 26
    if (__system_properties_init() != 0) {
        std::cerr << "resetprop: __system_properties_init failed\n";
        return 1;
    }
#endif

    bool wait_mode = false;
    std::vector<const char*> positional;

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            usage(std::cerr, prog);
            return 0;
        }
        if (strcmp(arg, "-n") == 0) {
            continue;
        }
        if (strcmp(arg, "-w") == 0) {
            wait_mode = true;
            continue;
        }
        positional.push_back(arg);
    }

    if (wait_mode) {
        if (positional.size() != 2) {
            usage(std::cerr, prog);
            return 1;
        }
        return wait_for_property_value(positional[0], positional[1]) ? 0 : 1;
    }

    if (positional.empty()) {
        std::vector<std::pair<std::string, std::string>> props;
        if (__system_property_foreach(list_foreach_callback, &props) != 0) {
            std::cerr << "resetprop: __system_property_foreach failed\n";
            return 1;
        }
        for (const auto& p : props)
            std::cout << "[" << p.first << "]: [" << p.second << "]\n";
        return 0;
    }

    if (positional.size() == 1) {
        char value[PROP_VALUE_MAX];
        if (prop_get(positional[0], value) < 0) {
            std::cerr << "resetprop: property not found: " << positional[0] << "\n";
            return 1;
        }
        std::cout << value << "\n";
        return 0;
    }

    if (positional.size() == 2) {
        // Force bionic to map the backing property area before we snapshot and remap it writable.
        char current[PROP_VALUE_MAX] = {};
        (void)prop_get(positional[0], current);

        ScopedPropertyWriteAccess access;
        if (!access.active()) {
            std::cerr << "resetprop: failed to gain write access to property area\n";
            return 1;
        }
        if (!set_property_direct(positional[0], positional[1])) {
            std::cerr << "resetprop: direct property update failed for " << positional[0] << "\n";
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
