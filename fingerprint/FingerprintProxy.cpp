/*
 * SPDX-FileCopyrightText: 2026 The MagicNekoROM Project
 * SPDX-License-Identifier: Apache-2.0
 *
 * AIDL fingerprint HAL that sits in front of the OPlus HAL and forwards every
 * call to it.
 *
 * Why this exists: the OPlus HAL implements the V2 interface but only the V1
 * method set. enrollWithContext/authenticateWithContext/... fall through to
 * ISessionDefault and answer EX_UNSUPPORTED_OPERATION, which the framework
 * turns into an UnsupportedOperationException inside system_server (doEnroll
 * only catches RemoteException). The framework decides whether to use those
 * methods from hasContextMethods(), i.e. from the interface version the HAL
 * reports.
 *
 * BnSession::getInterfaceVersion/getInterfaceHash are declared `final` in the
 * AIDL-generated BnSession, so a subclass cannot correct the version. Building
 * this HAL against the V1 ndk library instead makes it report version 1 by
 * construction, and the framework then drives the V1 method set -- the one the
 * hardware side actually implements. No framework patch needed.
 *
 * V1 is a strict prefix of V2 on the wire (AIDL appends methods and keeps the
 * existing transaction codes), so forwarding a V1 call to the V2 remote works.
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
#include <android/binder_process.h>
#include <android/binder_status.h>

#include <memory>
#include <string>
#include <vector>

namespace {

namespace fp = ::aidl::android::hardware::biometrics::fingerprint;
namespace common = ::aidl::android::hardware::biometrics::common;
namespace keymaster = ::aidl::android::hardware::keymaster;

constexpr const char* kDescriptor = "android.hardware.biometrics.fingerprint.IFingerprint";
constexpr const char* kRemoteInstance = "android.hardware.biometrics.fingerprint.IFingerprint/oplusfp";
constexpr const char* kLocalInstance = "android.hardware.biometrics.fingerprint.IFingerprint/default";

// Forwards the whole V1 ISession surface to the original HAL.
//
// Nothing is interpreted here: the original HAL owns the sensor, the TEE and
// the callbacks, and it notifies the framework through the ISessionCallback
// object the framework itself handed us in createSession. Passing that same
// callback pointer straight through keeps the callback path at one hop.
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

    ndk::ScopedAStatus enumerateEnrollments() override { return mRemote->enumerateEnrollments(); }

    ndk::ScopedAStatus removeEnrollments(const std::vector<int32_t>& enrollmentIds) override {
        return mRemote->removeEnrollments(enrollmentIds);
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

    // ---- V5 surface -------------------------------------------------------
    // The framework is compiled against fingerprint-V5-java, so SensorProps and
    // the session calls use the V5 layout. The stock HAL only implements the V1
    // method set, so every *WithContext entry point drops the context object and
    // forwards to its V1 equivalent. The last three have no V1 counterpart: the
    // stock HAL cannot honour them, so they report OK and do nothing.
    ndk::ScopedAStatus authenticateWithContext(
            int64_t operationId, const common::OperationContext& /*context*/,
            std::shared_ptr<common::ICancellationSignal>* out) override {
        return mRemote->authenticate(operationId, out);
    }

    ndk::ScopedAStatus enrollWithContext(const keymaster::HardwareAuthToken& hat,
                                         const common::OperationContext& /*context*/,
                                         std::shared_ptr<common::ICancellationSignal>* out) override {
        return mRemote->enroll(hat, out);
    }

    ndk::ScopedAStatus detectInteractionWithContext(
            const common::OperationContext& /*context*/,
            std::shared_ptr<common::ICancellationSignal>* out) override {
        return mRemote->detectInteraction(out);
    }

    ndk::ScopedAStatus onPointerDownWithContext(const fp::PointerContext& context) override {
        return mRemote->onPointerDown(context.pointerId, context.x, context.y, context.minor,
                                      context.major);
    }

    ndk::ScopedAStatus onPointerUpWithContext(const fp::PointerContext& context) override {
        return mRemote->onPointerUp(context.pointerId);
    }

    ndk::ScopedAStatus onContextChanged(const common::OperationContext& /*context*/) override {
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus onPointerCancelWithContext(const fp::PointerContext& /*context*/) override {
        return ndk::ScopedAStatus::ok();
    }

    ndk::ScopedAStatus setIgnoreDisplayTouches(bool /*shouldIgnore*/) override {
        return ndk::ScopedAStatus::ok();
    }

  private:
    std::shared_ptr<fp::ISession> mRemote;
};

class FingerprintProxy : public fp::BnFingerprint {
  public:
    explicit FingerprintProxy(std::shared_ptr<fp::IFingerprint> remote)
        : mRemote(std::move(remote)) {}

    ndk::ScopedAStatus getSensorProps(std::vector<fp::SensorProps>* out) override {
        return mRemote->getSensorProps(out);
    }

    ndk::ScopedAStatus createSession(int32_t sensorId, int32_t userId,
                                     const std::shared_ptr<fp::ISessionCallback>& cb,
                                     std::shared_ptr<fp::ISession>* out) override {
        std::shared_ptr<fp::ISession> remoteSession;
        ndk::ScopedAStatus status = mRemote->createSession(sensorId, userId, cb, &remoteSession);
        if (!status.isOk()) {
            LOG(ERROR) << "createSession(" << sensorId << ", " << userId
                       << ") failed: " << status.getDescription();
            return status;
        }
        if (remoteSession == nullptr) {
            LOG(ERROR) << "createSession returned a null session";
            return ndk::ScopedAStatus::fromServiceSpecificError(-1);
        }
        *out = ndk::SharedRefBase::make<SessionProxy>(remoteSession);
        return ndk::ScopedAStatus::ok();
    }

  private:
    std::shared_ptr<fp::IFingerprint> mRemote;
};

}  // namespace

int main() {
    android::base::InitLogging(nullptr, android::base::LogdLogger());

    // The original HAL registers first (as /oplusfp, renamed by
    // libfpservicerename) because both are in late_start and this one blocks
    // here until it shows up. If it never does, exiting is better than serving
    // a HAL that cannot talk to hardware.
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
    LOG(INFO) << "connected to " << kRemoteInstance;

    std::shared_ptr<FingerprintProxy> service = ndk::SharedRefBase::make<FingerprintProxy>(remote);

    // Some calls are interrupting by design (onPointerDown/onUiReady arrive
    // while an enroll or authenticate is in flight), so the pool needs more
    // than one thread or those would block behind the long operation.
    ABinderProcess_setThreadPoolMaxThreadCount(8);

    binder_status_t status = AServiceManager_addService(service->asBinder().get(), kLocalInstance);
    if (status != STATUS_OK) {
        LOG(FATAL) << "addService(" << kLocalInstance << ") failed: " << status;
        return 1;
    }
    LOG(INFO) << "registered " << kLocalInstance;

    ABinderProcess_joinThreadPool();
    return 0;
}
