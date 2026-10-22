/*
 * SPDX-FileCopyrightText: 2026 The MagicNekoROM Project
 * SPDX-License-Identifier: Apache-2.0
 */

#define LOG_TAG "FpServiceRename"

#include <android-base/logging.h>

#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_status.h>

#include <dlfcn.h>

#include <string>

namespace {

// The OPlus fingerprint HAL registers IFingerprint/default with the instance
// name hardcoded in main(). The proxy HAL has to own "default" for the
// framework to find it, so this library renames the original registration from
// inside the original HAL's own process.
//
// Why not patch the "/default" string in the binary instead: that same string
// is used by five other call sites as a "<path>/default" path suffix --
// Device::Device, Device::controlTp, Device::set_fp_active_flag,
// Perf::setUxthread and DcsReport::sendToDcsService. Renaming the string would
// silently change all of those paths. Intercepting only the registration call
// leaves them untouched, and the original blob stays byte-for-byte intact.
constexpr const char* kDescriptor = "android.hardware.biometrics.fingerprint.IFingerprint";
constexpr const char* kFromSuffix = "/default";
constexpr const char* kToSuffix = "/oplusfp";

}  // namespace

extern "C" binder_status_t AServiceManager_addService(AIBinder* binder, const char* instance) {
    static auto real = reinterpret_cast<decltype(&AServiceManager_addService)>(
            dlsym(RTLD_NEXT, "AServiceManager_addService"));

    if (real == nullptr) {
        LOG(ERROR) << "dlsym(RTLD_NEXT, AServiceManager_addService) failed; cannot rename";
        return STATUS_UNKNOWN_ERROR;
    }
    if (instance == nullptr) {
        return real(binder, instance);
    }

    const std::string asDefault = std::string(kDescriptor) + kFromSuffix;
    if (asDefault == instance) {
        const std::string renamed = std::string(kDescriptor) + kToSuffix;
        LOG(INFO) << "fingerprint HAL instance " << instance << " -> " << renamed;
        return real(binder, renamed.c_str());
    }
    return real(binder, instance);
}
