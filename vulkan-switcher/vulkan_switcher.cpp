/*
 * vulkan.switcher.so -- runtime Vulkan ICD selector for Android's hwvulkan HAL.
 *
 * Install: /vendor/lib64/hw/vulkan.switcher.so, selected with
 *          ro.hardware.vulkan=switcher
 *
 * libvulkan asks the HAL for exactly one module (HMI), opens it once per
 * process and keeps the returned hwvulkan_device_t. This library is that
 * module: instead of talking to the GPU it picks the real ICD
 * (vulkan.adreno.so = OEM Qualcomm, vulkan.freedreno.so = Mesa Turnip),
 * loads it, and hands its device struct back to the loader unchanged.
 *
 * Nothing in frameworks/native is touched, and both ICDs stay installed side
 * by side in /vendor/lib64/hw/, so switching is a line in a config file plus
 * a process restart -- no rebuild, no property reset, no framework patch.
 *
 * Config: /data/misc/vulkan/per_app.conf
 *   <package>=<subname>     # e.g. com.tungsten.fcl=freedreno
 *   *=<subname>             # global default, used when no package matches
 * A missing file, an unparsable line, or a package that matches nothing falls
 * through to the compiled-in default (VKSWITCH_DEFAULT_SUBNAME, set from
 * GPU_DRIVER_TURNIP in Android.bp) and finally to the OEM driver, so a bad
 * config can never leave the device without Vulkan.
 *
 * There is deliberately no config file on /vendor: sepolicy's neverallow on
 * domain.te:2155 forbids app domains from reading vendor_file_type files, so
 * the image default is a constant instead.
 */

#define LOG_TAG "vkswitch"

#include <dlfcn.h>
#include <hardware/hardware.h>
#include <hardware/hwvulkan.h>
#include <log/log.h>

#include <cstring>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include "per_app_conf.h"

namespace {

constexpr char kRwConfPath[] = "/data/misc/vulkan/per_app.conf";

#ifndef VKSWITCH_DEFAULT_SUBNAME
#define VKSWITCH_DEFAULT_SUBNAME "freedreno"
#endif

// Kept as literals: they double as the library suffix (vulkan.<subname>.so)
// and as the fallback chain when the configured entry cannot be loaded.
constexpr char kOemSubname[] = "adreno";
constexpr char kTurnipSubname[] = "freedreno";

int OpenDevice(const hw_module_t* module, const char* id, hw_device_t** device);
int CloseRealDevice(hw_device_t* device);

std::mutex g_lock;

// Our copy of the real device. The loader caches this pointer for the whole
// process, so it must stay alive and keep pointing at the real module: on
// driver unload the loader calls hw_device_t::close() and then unloads
// hw_device_t::module->dso itself.
hwvulkan_device_t g_dev;
hwvulkan_device_t* g_real = nullptr;
std::string g_loaded_subname;

hw_module_methods_t g_module_methods = {.open = OpenDevice};

// A package name for apps, a program name for native services
// ("/system/bin/gpuservice" -> "gpuservice").
std::string CurrentProcessName() {
    std::ifstream f("/proc/self/cmdline", std::ios::binary);
    if (!f)
        return "";
    char buf[256] = {};
    f.read(buf, sizeof(buf) - 1);
    std::string name(buf);
    const auto slash = name.rfind('/');
    if (slash != std::string::npos)
        name = name.substr(slash + 1);
    // "com.example:remote" shares the package's rule.
    const auto colon = name.find(':');
    if (colon != std::string::npos)
        name.resize(colon);
    return name;
}

// The shim itself is loaded through the sphal namespace (that is how libvulkan
// loads any ICD), so a bare library name resolves there first. The absolute
// path is the fallback for setups where the search path does not cover
// /vendor/lib64/hw.
void* OpenIcd(const std::string& subname) {
    const std::string soname = "vulkan." + subname + ".so";
    void* so = dlopen(soname.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (so)
        return so;
    ALOGW("dlopen(%s) failed: %s", soname.c_str(), dlerror());

    const std::string abs_path = "/vendor/lib64/hw/" + soname;
    so = dlopen(abs_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (so)
        return so;
    ALOGW("dlopen(%s) failed: %s", abs_path.c_str(), dlerror());
    return nullptr;
}

// Never unloads the real library: the loader owns that through
// g_dev.common.module->dso once it sees the device struct.
bool AdoptIcd(const std::string& subname) {
    void* so = OpenIcd(subname);
    if (!so)
        return false;

    auto* hmi = static_cast<hw_module_t*>(dlsym(so, HAL_MODULE_INFO_SYM_AS_STR));
    if (!hmi) {
        ALOGE("vulkan.%s.so has no %s: %s", subname.c_str(),
              HAL_MODULE_INFO_SYM_AS_STR, dlerror());
        dlclose(so);
        return false;
    }
    if (strcmp(hmi->id, HWVULKAN_HARDWARE_MODULE_ID) != 0) {
        ALOGE("vulkan.%s.so reports HAL id '%s', expected '%s'", subname.c_str(),
              hmi->id, HWVULKAN_HARDWARE_MODULE_ID);
        dlclose(so);
        return false;
    }
    if (!hmi->methods || !hmi->methods->open) {
        ALOGE("vulkan.%s.so exposes no open() method", subname.c_str());
        dlclose(so);
        return false;
    }

    hwvulkan_device_t* real = nullptr;
    const int rc = hmi->methods->open(hmi, HWVULKAN_DEVICE_0,
                                      reinterpret_cast<hw_device_t**>(&real));
    if (rc != 0 || !real) {
        ALOGE("vulkan.%s.so open() failed: %d", subname.c_str(), rc);
        dlclose(so);
        return false;
    }

    g_real = real;
    g_dev.common.tag = HARDWARE_DEVICE_TAG;
    g_dev.common.version = real->common.version;
    // Point at the real module on purpose: the loader unloads that dso, not us.
    g_dev.common.module = hmi;
    g_dev.common.close = CloseRealDevice;
    g_dev.EnumerateInstanceExtensionProperties =
        real->EnumerateInstanceExtensionProperties;
    g_dev.CreateInstance = real->CreateInstance;
    g_dev.GetInstanceProcAddr = real->GetInstanceProcAddr;
    g_loaded_subname = subname;
    ALOGI("adopted vulkan.%s.so", subname.c_str());
    return true;
}

// Called by the loader with our own g_dev.common; forwards to the real driver.
int CloseRealDevice(hw_device_t* /*device*/) {
    std::lock_guard<std::mutex> guard(g_lock);
    hwvulkan_device_t* real = g_real;
    g_real = nullptr;
    if (real && real->common.close)
        return real->common.close(&real->common);
    return 0;
}

int OpenDevice(const hw_module_t* /*module*/, const char* id, hw_device_t** device) {
    if (!device)
        return -EINVAL;
    if (strcmp(id, HWVULKAN_DEVICE_0) != 0)
        return -ENOENT;

    std::lock_guard<std::mutex> guard(g_lock);
    if (g_real) {
        *device = &g_dev.common;
        return 0;
    }

    const std::string pkg = CurrentProcessName();
    const std::string configured =
        vkswitch::LookupConfiguredSubname(pkg, kRwConfPath, nullptr);

    // Ordered candidate list: explicit config, compiled-in default, then both
    // known ICDs, so one unloadable library never costs the process its Vulkan.
    std::vector<std::string> candidates;
    if (!configured.empty())
        candidates.push_back(configured);
    candidates.push_back(VKSWITCH_DEFAULT_SUBNAME);
    candidates.push_back(kOemSubname);
    candidates.push_back(kTurnipSubname);

    for (size_t i = 0; i < candidates.size(); ++i) {
        const std::string& candidate = candidates[i];
        if (!vkswitch::IsValidSubname(candidate))
            continue;
        bool duplicate = false;
        for (size_t j = 0; j < i; ++j) {
            if (candidates[j] == candidate) {
                duplicate = true;
                break;
            }
        }
        if (duplicate)
            continue;
        if (AdoptIcd(candidate)) {
            ALOGI("pkg=%s configured=%s -> %s", pkg.c_str(),
                  configured.empty() ? "(none)" : configured.c_str(),
                  g_loaded_subname.c_str());
            *device = &g_dev.common;
            return 0;
        }
    }

    ALOGE("pkg=%s: no usable Vulkan ICD, falling back to the stub HAL",
          pkg.c_str());
    return -ENOENT;
}

}  // namespace

// ---------------------------------------------------------------------------
// HAL module entry point. Same shape as the reference implementations in
// frameworks/native/vulkan/nulldrv: the loader dlsym()s "HMI" and checks id.
// ---------------------------------------------------------------------------
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-variable-declarations"
__attribute__((visibility("default"))) hwvulkan_module_t HAL_MODULE_INFO_SYM = {
    .common =
        {
            .tag = HARDWARE_MODULE_TAG,
            .module_api_version = HWVULKAN_MODULE_API_VERSION_0_1,
            .hal_api_version = HARDWARE_HAL_API_VERSION,
            .id = HWVULKAN_HARDWARE_MODULE_ID,
            .name = "Vulkan ICD switcher",
            .author = "ovaltine device tree",
            .methods = &g_module_methods,
        },
};
#pragma clang diagnostic pop
