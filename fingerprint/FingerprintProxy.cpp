/*
 * SPDX-FileCopyrightText: 2026 The MagicNekoROM Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * AIDL fingerprint HAL proxy: V5 surface for the framework, V2 wire to the
 * stock OPlus HAL.
 *
 * EXPERIMENT BUILD: createSession diagnostics.
 *   - getSensorProps now reports a real UDFPS sensor again (so the framework
 *     actually calls createSession).
 *   - createSession always returns a Dummy session so system_server cannot die
 *     on an uncaught exception, while it logs three raw-transact probes that
 *     tell us why the V4 client call to the V2 HAL dies with BAD_TYPE.
 */

#define LOG_TAG "FpProxyHal"

#include <aidl/android/hardware/biometrics/fingerprint/BnFingerprint.h>
#include <aidl/android/hardware/biometrics/fingerprint/BnSession.h>
#include <aidl/android/hardware/biometrics/fingerprint/ISession.h>
#include <aidl/android/hardware/biometrics/fingerprint/ISessionCallback.h>
#include <aidl/android/hardware/biometrics/fingerprint/SensorProps.h>
#include <aidl/android/hardware/biometrics/common/ICancellationSignal.h>
#include <aidl/android/hardware/keymaster/HardwareAuthToken.h>

#include <android-base/logging.h>
#include <android/binder_ibinder.h>
#include <android/binder_manager.h>
#include <android/binder_parcel.h>
#include <android/binder_process.h>
#include <android/binder_status.h>

#include <cstdio>
#include <cstdlib>
#include <sys/system_properties.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace {

namespace fp = ::aidl::android::hardware::biometrics::fingerprint;
namespace common = ::aidl::android::hardware::biometrics::common;
namespace keymaster = ::aidl::android::hardware::keymaster;

constexpr const char* kRemoteInstance =
        "android.hardware.biometrics.fingerprint.IFingerprint/oplusfp";
constexpr const char* kLocalInstance =
        "android.hardware.biometrics.fingerprint.IFingerprint/default";

// FIRST_CALL_TRANSACTION + 1 == createSession in frozen V2 IFingerprint.aidl
constexpr uint32_t kCreateSessionTransaction = 1 /*FIRST_CALL_TRANSACTION*/ + 1;

// ---------------------------------------------------------------------------
// Dummy session: satisfies the framework well enough that it does not take
// down system_server. Nothing here touches the hardware.
// ---------------------------------------------------------------------------
class DummySession : public fp::BnSession {
  public:
    explicit DummySession(const char* why) : mWhy(why) {}
    ndk::ScopedAStatus generateChallenge() override {
        LOG(INFO) << tag() << "generateChallenge";
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus revokeChallenge(int64_t) override { return ndk::ScopedAStatus::ok(); }
    ndk::ScopedAStatus enroll(const keymaster::HardwareAuthToken&,
                              std::shared_ptr<common::ICancellationSignal>* out) override {
        LOG(INFO) << tag() << "enroll";
        *out = nullptr;
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus authenticate(int64_t,
                                    std::shared_ptr<common::ICancellationSignal>* out) override {
        LOG(INFO) << tag() << "authenticate";
        *out = nullptr;
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus detectInteraction(
            std::shared_ptr<common::ICancellationSignal>* out) override {
        *out = nullptr;
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus enumerateEnrollments() override { return ndk::ScopedAStatus::ok(); }
    ndk::ScopedAStatus removeEnrollments(const std::vector<int32_t>&) override {
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus getAuthenticatorId() override { return ndk::ScopedAStatus::ok(); }
    ndk::ScopedAStatus invalidateAuthenticatorId() override { return ndk::ScopedAStatus::ok(); }
    ndk::ScopedAStatus resetLockout(const keymaster::HardwareAuthToken&) override {
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus close() override {
        LOG(INFO) << tag() << "close";
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus onPointerDown(int32_t, int32_t, int32_t, float, float) override {
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus onPointerUp(int32_t) override { return ndk::ScopedAStatus::ok(); }
    ndk::ScopedAStatus onUiReady() override { return ndk::ScopedAStatus::ok(); }
    ndk::ScopedAStatus authenticateWithContext(
            int64_t, const common::OperationContext&,
            std::shared_ptr<common::ICancellationSignal>* out) override {
        *out = nullptr;
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus enrollWithContext(
            const keymaster::HardwareAuthToken&, const common::OperationContext&,
            std::shared_ptr<common::ICancellationSignal>* out) override {
        *out = nullptr;
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus detectInteractionWithContext(
            const common::OperationContext&,
            std::shared_ptr<common::ICancellationSignal>* out) override {
        *out = nullptr;
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus onPointerDownWithContext(const fp::PointerContext&) override {
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus onPointerUpWithContext(const fp::PointerContext&) override {
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus onContextChanged(const common::OperationContext&) override {
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus onPointerCancelWithContext(const fp::PointerContext&) override {
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus setIgnoreDisplayTouches(bool) override { return ndk::ScopedAStatus::ok(); }

  private:
    const char* tag() const { return mWhy; }
    const char* mWhy;
};

// ---------------------------------------------------------------------------
// Real forwarding session (kept for when the bridge works).
// ---------------------------------------------------------------------------
class SessionProxy : public fp::BnSession {
  public:
    explicit SessionProxy(std::shared_ptr<fp::ISession> remote) : mRemote(std::move(remote)) {}

    ndk::ScopedAStatus generateChallenge() override { return mRemote->generateChallenge(); }
    ndk::ScopedAStatus revokeChallenge(int64_t challenge) override {
        return mRemote->revokeChallenge(challenge);
    }
    ndk::ScopedAStatus enroll(const keymaster::HardwareAuthToken& hat,
                              std::shared_ptr<common::ICancellationSignal>* out) override {
        return mRemote->enroll(hat, out);
    }
    ndk::ScopedAStatus authenticate(int64_t operationId,
                                    std::shared_ptr<common::ICancellationSignal>* out) override {
        return mRemote->authenticate(operationId, out);
    }
    ndk::ScopedAStatus detectInteraction(
            std::shared_ptr<common::ICancellationSignal>* out) override {
        return mRemote->detectInteraction(out);
    }
    ndk::ScopedAStatus enumerateEnrollments() override {
        return mRemote->enumerateEnrollments();
    }
    ndk::ScopedAStatus removeEnrollments(const std::vector<int32_t>& ids) override {
        return mRemote->removeEnrollments(ids);
    }
    ndk::ScopedAStatus getAuthenticatorId() override { return mRemote->getAuthenticatorId(); }
    ndk::ScopedAStatus invalidateAuthenticatorId() override {
        return mRemote->invalidateAuthenticatorId();
    }
    ndk::ScopedAStatus resetLockout(const keymaster::HardwareAuthToken& hat) override {
        return mRemote->resetLockout(hat);
    }
    ndk::ScopedAStatus close() override { return mRemote->close(); }
    ndk::ScopedAStatus onPointerDown(int32_t pointerId, int32_t x, int32_t y, float minor,
                                     float major) override {
        return mRemote->onPointerDown(pointerId, x, y, minor, major);
    }
    ndk::ScopedAStatus onPointerUp(int32_t pointerId) override {
        return mRemote->onPointerUp(pointerId);
    }
    ndk::ScopedAStatus onUiReady() override { return mRemote->onUiReady(); }
    ndk::ScopedAStatus authenticateWithContext(
            int64_t operationId, const common::OperationContext&,
            std::shared_ptr<common::ICancellationSignal>* out) override {
        return mRemote->authenticate(operationId, out);
    }
    ndk::ScopedAStatus enrollWithContext(
            const keymaster::HardwareAuthToken& hat, const common::OperationContext&,
            std::shared_ptr<common::ICancellationSignal>* out) override {
        return mRemote->enroll(hat, out);
    }
    ndk::ScopedAStatus detectInteractionWithContext(
            const common::OperationContext&,
            std::shared_ptr<common::ICancellationSignal>* out) override {
        return mRemote->detectInteraction(out);
    }
    ndk::ScopedAStatus onPointerDownWithContext(const fp::PointerContext& ctx) override {
        return mRemote->onPointerDown(ctx.pointerId, ctx.x, ctx.y, ctx.minor, ctx.major);
    }
    ndk::ScopedAStatus onPointerUpWithContext(const fp::PointerContext& ctx) override {
        return mRemote->onPointerUp(ctx.pointerId);
    }
    ndk::ScopedAStatus onContextChanged(const common::OperationContext&) override {
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus onPointerCancelWithContext(const fp::PointerContext&) override {
        return ndk::ScopedAStatus::ok();
    }
    ndk::ScopedAStatus setIgnoreDisplayTouches(bool) override { return ndk::ScopedAStatus::ok(); }

  private:
    std::shared_ptr<fp::ISession> mRemote;
};

// ---------------------------------------------------------------------------
// Raw transact probe. Returns the binder_status_t of the transaction itself.
// ---------------------------------------------------------------------------
binder_status_t ProbeCreateSession(AIBinder* target, int32_t sensorId, int32_t userId,
                                   AIBinder* cb, int mode, const char* label) {
    AParcel* in = nullptr;
    binder_status_t st = AIBinder_prepareTransaction(target, &in);
    if (st != STATUS_OK) {
        LOG(ERROR) << label << ": prepareTransaction failed: " << st;
        return st;
    }
    AParcel_writeInt32(in, sensorId);
    AParcel_writeInt32(in, userId);
    if (mode >= 1) {
        // mode 1: write the real callback binder, mode 2: write a null binder,
        // mode 3: write nothing at all.
        if (mode == 2) {
            AParcel_writeStrongBinder(in, nullptr);
        } else {
            AParcel_writeStrongBinder(in, cb);
        }
    }
    AParcel* reply = nullptr;
    st = AIBinder_transact(target, kCreateSessionTransaction, &in, &reply, 0);

    char propVal[128];
    snprintf(propVal, sizeof(propVal), "transact=%d", st);
    __system_property_set((std::string("persist.vendor.fp.probe") + label[6]).c_str(), propVal);
    LOG(ERROR) << "[EXPDBG] " << label << ": transact status=" << st
              << " (0=OK, -129-ish mapped to EX_TRANSACTION_FAILED on the AIDL side)";
    if (reply != nullptr) {
        AStatus* status = nullptr;
        AParcel_readStatusHeader(reply, &status);
        if (status != nullptr) {
            {
                char ex[128];
                snprintf(ex, sizeof(ex), "exc=%d msg=%s", AStatus_getExceptionCode(status),
                         AStatus_getMessage(status) ? AStatus_getMessage(status) : "");
                __system_property_set((std::string("persist.vendor.fp.res") + label[6]).c_str(), ex);
            }
            LOG(ERROR) << "[EXPDBG] " << label << ": reply exception=" << AStatus_getExceptionCode(status)
                      << " message='" << (AStatus_getMessage(status) ? AStatus_getMessage(status) : "")
                      << "' serviceSpecific=" << AStatus_getServiceSpecificError(status);
            AStatus_delete(status);
        }
        AParcel_delete(reply);
    }
    AParcel_delete(in);
    return st;
}

class FingerprintProxy : public fp::BnFingerprint {
  public:
    explicit FingerprintProxy(std::shared_ptr<fp::IFingerprint> remote)
        : mRemote(std::move(remote)) {}

    ndk::ScopedAStatus getSensorProps(std::vector<fp::SensorProps>* out) override {
        fp::SensorProps props;
        props.commonProps.sensorId = 0;
        props.commonProps.sensorStrength = common::SensorStrength::STRONG;
        props.commonProps.maxEnrollmentsPerUser = 5;
        props.sensorType = fp::FingerprintSensorType::UNDER_DISPLAY_OPTICAL;
        props.supportsNavigationGestures = false;
        props.supportsDetectInteraction = true;
        props.halHandlesDisplayTouches = false;
        props.halControlsIllumination = true;
        props.touchDetectionParameters = std::nullopt;

        fp::SensorLocation loc;
        loc.sensorLocationX = 540;
        loc.sensorLocationY = 2170;
        loc.sensorRadius = 100;
        props.sensorLocations = {loc};

        out->clear();
        out->push_back(props);
        LOG(ERROR) << "[EXPDBG] getSensorProps: 1 UDFPS sensor";
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus createSession(int32_t sensorId, int32_t userId,
                                     const std::shared_ptr<fp::ISessionCallback>& cb,
                                     std::shared_ptr<fp::ISession>* out) override {
        const bool cbNull = (cb == nullptr);
        AIBinder* remote = mRemote->asBinder().get();
        AIBinder* cbBinder = cbNull ? nullptr : cb->asBinder().get();

        LOG(ERROR) << "[EXPDBG] === createSession probe: sensorId=" << sensorId << " userId=" << userId
                  << " cb=" << (cbNull ? "NULL" : "non-null")
                  << " cbBinder=" << (cbBinder ? "ok" : "null")
                  << " remoteAlive=" << (remote ? AIBinder_isAlive(remote) : 0);

        // Three probes, each on its own transaction (a failure does not poison
        // the next one because every probe builds a fresh parcel).
        __system_property_set("persist.vendor.fp.cbnull", cbNull ? "1" : "0");
        __system_property_set("persist.vendor.fp.remotealive",
                              (remote && AIBinder_isAlive(remote)) ? "1" : "0");
        ProbeCreateSession(remote, sensorId, userId, cbBinder, 1, "probe[1: int,int,cb]");
        ProbeCreateSession(remote, sensorId, userId, cbBinder, 2, "probe[2: int,int,null]");
        ProbeCreateSession(remote, sensorId, userId, cbBinder, 3, "probe[3: int,int only]");

        // Never let the framework see an exception here: it does not catch it
        // and system_server dies. Hand it a dummy session instead.
        *out = ndk::SharedRefBase::make<DummySession>("dummy");
        LOG(ERROR) << "[EXPDBG] createSession: DUMMY session returned";
        return ndk::ScopedAStatus::ok();
    }

  private:
    std::shared_ptr<fp::IFingerprint> mRemote;
};

}  // namespace

int main() {
    android::base::InitLogging(nullptr, android::base::LogdLogger());
    android::base::SetMinimumLogSeverity(android::base::VERBOSE);

    AIBinder* raw = AServiceManager_waitForService(kRemoteInstance);
    if (raw == nullptr) {
        LOG(FATAL) << "timed out waiting for " << kRemoteInstance;
        return 1;
    }
    std::shared_ptr<fp::IFingerprint> remote = fp::IFingerprint::fromBinder(ndk::SpAIBinder(raw));
    if (remote == nullptr) {
        LOG(FATAL) << kRemoteInstance << " is not an IFingerprint";
        return 1;
    }
    LOG(ERROR) << "[EXPDBG] connected to " << kRemoteInstance;

    std::shared_ptr<FingerprintProxy> service = ndk::SharedRefBase::make<FingerprintProxy>(remote);

    ABinderProcess_setThreadPoolMaxThreadCount(8);

    binder_status_t status = AServiceManager_addService(service->asBinder().get(), kLocalInstance);
    if (status != STATUS_OK) {
        LOG(FATAL) << "addService(" << kLocalInstance << ") failed: " << status;
        return 1;
    }
    LOG(ERROR) << "[EXPDBG] registered " << kLocalInstance;

    ABinderProcess_joinThreadPool();
    return 0;
}
