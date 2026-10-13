// 汐 2026-10-10：查 display.qservice 到底注册在哪条 binder 上（带诊断）。
#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <binder/IBinder.h>
#include <cstdio>
#include <cstring>

using namespace android;

static void dump(const char* tag, const sp<IServiceManager>& sm) {
    if (sm == nullptr) { printf("[%s] manager = NULL\n", tag); return; }

    sp<IBinder> b = sm->checkService(String16("display.qservice"));
    printf("[%s] checkService(display.qservice) = %s\n", tag, b ? "*** FOUND ***" : "not found");

    for (int prio : {0, -1}) {
        Vector<String16> names = sm->listServices(prio);
        printf("[%s] listServices(%d) -> %zu\n", tag, prio, names.size());
        int hits = 0;
        for (size_t i = 0; i < names.size(); i++) {
            const char* n = String8(names[i]).c_str();
            if (strstr(n, "qservice") || strstr(n, "qdisplay")) {
                printf("[%s]   HIT: %s\n", tag, n);
                hits++;
            }
        }
        if (names.size() > 0 && hits == 0) {
            printf("[%s]   前 3 条: ", tag);
            for (size_t i = 0; i < names.size() && i < 3; i++)
                printf("%s | ", String8(names[i]).c_str());
            printf("\n");
        }
    }
}

int main(int argc, char** argv) {
    if (argc > 1) { ProcessState::initWithDriver(argv[1]); printf("forced: %s\n", argv[1]); }
    else { ProcessState::self(); printf("driver: (build default)\n"); }

    sp<ProcessState> ps = ProcessState::self();
    printf("getDriverName() = %s\n", ps ? ps->getDriverName().c_str() : "(null)");

    dump("main", defaultServiceManager());
    return 0;
}
