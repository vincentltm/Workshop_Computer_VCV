#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#define RTLD_LAZY 0
#define RTLD_LOCAL 0
#define RTLD_DEFAULT ((void*)0)
#define RTLD_NOLOAD 0

inline void* dlopen(const char* filename, int flags) {
    (void)flags;
    return (void*)LoadLibraryA(filename);
}

inline int dlclose(void* handle) {
    return FreeLibrary((HMODULE)handle) ? 0 : -1;
}

inline void* dlsym(void* handle, const char* symbol) {
    if (handle == nullptr) {
        HMODULE hModule = GetModuleHandleA(NULL);
        void* sym = (void*)GetProcAddress(hModule, symbol);
        if (sym) return sym;
        hModule = GetModuleHandleA("plugin.dll");
        if (hModule) {
            sym = (void*)GetProcAddress(hModule, symbol);
            if (sym) return sym;
        }
        return nullptr;
    }
    return (void*)GetProcAddress((HMODULE)handle, symbol);
}

inline const char* dlerror() {
    return "Dynamic linking error on Windows";
}
#else
#include <dlfcn.h>
#endif


// ── Monome SDK Structures (matching monome-rack layout) ─────────────────────

typedef enum {
    PROTOCOL_40H,
    PROTOCOL_SERIES,
    PROTOCOL_MEXT
} MonomeProtocol;

struct MonomeDevice {
    std::string id;
    std::string type;
    std::string prefix;
    int port;
    int width;
    int height;
    int rotation;
    MonomeProtocol protocol;
    bool varibright;
};

struct Grid {
    virtual ~Grid() {}
    virtual const MonomeDevice& getDevice() = 0;
    virtual void updateRow(int x_offset, int y, uint8_t bitfield) = 0;
    virtual void updateQuadrant(int x, int y, uint8_t* leds) = 0;
    virtual void updateRing(int n, uint8_t leds[64]) = 0;
    virtual void clearAll() = 0;
    virtual bool isHardware() = 0;
};

struct IGridConsumer {
    virtual ~IGridConsumer() {}
    virtual void gridConnected(Grid* grid) = 0;
    virtual void gridDisconnected(bool ownerChanged) = 0;
    virtual std::string gridGetCurrentDeviceId() = 0;
    virtual std::string gridGetLastDeviceId(bool owned) = 0;
    virtual void setLastDeviceId(std::string id) = 0;
    virtual void gridButtonEvent(int x, int y, bool state) = 0;
    virtual void encDeltaEvent(int n, int d) = 0;
    virtual Grid* gridGetDevice() = 0;
};

// ── Dynamic Linker Bridge ───────────────────────────────────────────────────

typedef void* (*fn_GridConnectionManager_get)();
typedef void (*fn_GridConnectionManager_registerConsumer)(void* manager, IGridConsumer* consumer);
typedef void (*fn_GridConnectionManager_deregisterConsumer)(void* manager, IGridConsumer* consumer);
typedef void (*fn_GridConnectionManager_connect)(void* manager, Grid* grid, IGridConsumer* consumer);
typedef void (*fn_GridConnectionManager_disconnect)(void* manager, IGridConsumer* consumer, bool ownerChanged);
typedef const std::set<Grid*>& (*fn_GridConnectionManager_getGrids)(void* manager);

struct MonomeBridge {
    bool initialized = false;
    void* manager_instance = nullptr;

    fn_GridConnectionManager_get get_fn = nullptr;
    fn_GridConnectionManager_registerConsumer reg_fn = nullptr;
    fn_GridConnectionManager_deregisterConsumer dereg_fn = nullptr;
    fn_GridConnectionManager_connect conn_fn = nullptr;
    fn_GridConnectionManager_disconnect disconn_fn = nullptr;
    fn_GridConnectionManager_getGrids get_grids_fn = nullptr;

    static MonomeBridge& get() {
        static MonomeBridge instance;
        return instance;
    }

    void* resolve_symbol(void* handle, const char* name) {
        // macOS prepends a leading underscore, making it two underscores for mangled C++ symbols
        std::string mac_name = "_" + std::string(name);
        void* sym = dlsym(handle, mac_name.c_str());
        if (sym) return sym;
        // Fallback for Linux and other platforms
        return dlsym(handle, name);
    }

    void init() {
        // VCV Rack loads plugins with RTLD_LOCAL, so their symbols are NOT visible
        // via RTLD_DEFAULT. We must open the monome plugin dylib directly using
        // RTLD_NOLOAD (to avoid re-mapping) and resolve symbols from that handle.

        // Build candidate paths for the monome plugin based on host OS
        std::vector<std::string> candidates;
#ifdef _WIN32
        const char* userprofile = getenv("USERPROFILE");
        if (userprofile) {
            std::string up = userprofile;
            candidates.push_back(up + "/Documents/Rack2/plugins/monome/plugin.dll");
            candidates.push_back(up + "/OneDrive/Documents/Rack2/plugins/monome/plugin.dll");
        }
        const char* localappdata = getenv("LOCALAPPDATA");
        if (localappdata) {
            std::string la = localappdata;
            candidates.push_back(la + "/Rack2/plugins/monome/plugin.dll");
        }
#elif defined(__APPLE__)
        const char* home = getenv("HOME");
        if (home) {
            std::string h = home;
            // Primary: arm64 plugin directory (Rack 2 on Apple Silicon)
            candidates.push_back(h + "/Library/Application Support/Rack2/plugins-mac-arm64/monome/plugin.dylib");
            // Fallback: universal / x86 directory
            candidates.push_back(h + "/Library/Application Support/Rack2/plugins/monome/plugin.dylib");
        }
        // App-bundle paths
        candidates.push_back("/Applications/VCV Rack 2 Pro.app/Contents/Resources/plugins/monome/plugin.dylib");
        candidates.push_back("/Applications/VCV Rack 2 Free.app/Contents/Resources/plugins/monome/plugin.dylib");
#else // Linux
        const char* home = getenv("HOME");
        if (home) {
            std::string h = home;
            candidates.push_back(h + "/.Rack2/plugins/monome/plugin.so");
            candidates.push_back(h + "/.local/share/Rack2/plugins/monome/plugin.so");
        }
#endif

        // Also try RTLD_DEFAULT first in case the host uses RTLD_GLOBAL
        {
            void* sym = resolve_symbol(RTLD_DEFAULT, "_ZN21GridConnectionManager3getEv");
            if (sym) {
                get_fn      = (fn_GridConnectionManager_get)sym;
                reg_fn      = (fn_GridConnectionManager_registerConsumer)resolve_symbol(RTLD_DEFAULT, "_ZN21GridConnectionManager20registerGridConsumerEP13IGridConsumer");
                dereg_fn    = (fn_GridConnectionManager_deregisterConsumer)resolve_symbol(RTLD_DEFAULT, "_ZN21GridConnectionManager22deregisterGridConsumerEP13IGridConsumer");
                conn_fn     = (fn_GridConnectionManager_connect)resolve_symbol(RTLD_DEFAULT, "_ZN21GridConnectionManager7connectEP4GridP13IGridConsumer");
                disconn_fn  = (fn_GridConnectionManager_disconnect)resolve_symbol(RTLD_DEFAULT, "_ZN21GridConnectionManager10disconnectEP13IGridConsumerb");
                get_grids_fn = (fn_GridConnectionManager_getGrids)resolve_symbol(RTLD_DEFAULT, "_ZN21GridConnectionManager8getGridsEv");
                if (get_fn && reg_fn && dereg_fn && conn_fn && disconn_fn && get_grids_fn) {
                    manager_instance = get_fn();
                    if (manager_instance) {
                        initialized = true;
                        return;
                    }
                }
            }
        }

        // Try each candidate path with RTLD_NOLOAD (get handle without re-mapping)
        for (const auto& path : candidates) {
            void* h = dlopen(path.c_str(), RTLD_NOLOAD | RTLD_LAZY | RTLD_LOCAL);
            if (!h) continue;

            void* sym = resolve_symbol(h, "_ZN21GridConnectionManager3getEv");
            if (!sym) { dlclose(h); continue; }

            get_fn      = (fn_GridConnectionManager_get)sym;
            reg_fn      = (fn_GridConnectionManager_registerConsumer)resolve_symbol(h, "_ZN21GridConnectionManager20registerGridConsumerEP13IGridConsumer");
            dereg_fn    = (fn_GridConnectionManager_deregisterConsumer)resolve_symbol(h, "_ZN21GridConnectionManager22deregisterGridConsumerEP13IGridConsumer");
            conn_fn     = (fn_GridConnectionManager_connect)resolve_symbol(h, "_ZN21GridConnectionManager7connectEP4GridP13IGridConsumer");
            disconn_fn  = (fn_GridConnectionManager_disconnect)resolve_symbol(h, "_ZN21GridConnectionManager10disconnectEP13IGridConsumerb");
            get_grids_fn = (fn_GridConnectionManager_getGrids)resolve_symbol(h, "_ZN21GridConnectionManager8getGridsEv");

            if (get_fn && reg_fn && dereg_fn && conn_fn && disconn_fn && get_grids_fn) {
                manager_instance = get_fn();
                if (manager_instance) {
                    initialized = true;
                    dlclose(h); // balanced close; dylib stays loaded since VCV holds a ref
                    return;
                }
            }
            dlclose(h);
        }

        // All strategies failed
        initialized = false;
    }

    bool is_available() {
        if (!initialized) {
            init();
        }
        return initialized;
    }

    void register_consumer(IGridConsumer* consumer) {
        if (initialized && reg_fn) {
            reg_fn(manager_instance, consumer);
        }
    }

    void deregister_consumer(IGridConsumer* consumer) {
        if (initialized && dereg_fn) {
            dereg_fn(manager_instance, consumer);
        }
    }

    void connect(Grid* grid, IGridConsumer* consumer) {
        if (initialized && conn_fn) {
            conn_fn(manager_instance, grid, consumer);
        }
    }

    void disconnect(IGridConsumer* consumer) {
        if (initialized && disconn_fn) {
            disconn_fn(manager_instance, consumer, false);
        }
    }

    std::vector<Grid*> get_grids() {
        std::vector<Grid*> result;
        if (initialized && get_grids_fn) {
            const std::set<Grid*>& s = get_grids_fn(manager_instance);
            for (Grid* g : s) {
                result.push_back(g);
            }
        }
        return result;
    }
};
