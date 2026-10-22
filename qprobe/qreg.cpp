// 汐 2026-10-10：display.qservice 占位，对所有 transact 答成功，探测 disp_api_init 的真实需求。
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binder/Binder.h>
#include <binder/Parcel.h>
#include <cstdio>

using namespace android;

class YesBinder : public BBinder {
  protected:
    status_t onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flags) override {
        printf("  [transact] code=%u flags=%u\n", code, flags);
        fflush(stdout);
        if (reply != nullptr) {
            reply->writeInt32(0);     // status = 0 (OK)
            reply->writeInt32(0);     // 一个通用返回值
        }
        return NO_ERROR;
    }
};

int main(int argc, char** argv) {
    const char* drv = (argc > 1) ? argv[1] : "/dev/binder";
    ProcessState::initWithDriver(drv);
    ProcessState::self()->startThreadPool();
    printf("driver = %s\n", drv);
    fflush(stdout);

    sp<IServiceManager> sm = defaultServiceManager();
    if (sm == nullptr) { printf("manager NULL\n"); return 1; }

    status_t st = sm->addService(String16("display.qservice"), new YesBinder(), false);
    printf("addService status = %d\n", st);
    fflush(stdout);
    sleep(3600);
    return 0;
}
