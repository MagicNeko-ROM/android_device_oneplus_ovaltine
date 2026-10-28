// Host-side checks for the per-app config rules used by vulkan.switcher.so.
// Build/run on the build machine:  g++ -O1 -o /tmp/test_conf test_conf.cpp && /tmp/test_conf

#include <cstdio>
#include <string>
#include <fstream>

#include "per_app_conf.h"

namespace {

int g_failures = 0;

void WriteFile(const char* path, const char* text) {
    std::ofstream f(path, std::ios::trunc);
    f << text;
}

void RemoveFile(const char* path) {
    std::remove(path);
}

void Check(const char* name, const std::string& got, const std::string& want) {
    if (got == want) {
        std::printf("PASS  %-46s -> '%s'\n", name, got.c_str());
        return;
    }
    std::printf("FAIL  %-46s got '%s' want '%s'\n", name, got.c_str(), want.c_str());
    ++g_failures;
}

}  // namespace

int main() {
    const char* rw = "/tmp/vk_conf_rw.conf";
    const char* ro = "/tmp/vk_conf_ro.conf";
    RemoveFile(rw);
    RemoveFile(ro);

    // --- 1. writable file only -------------------------------------------------
    WriteFile(rw,
              "# mapping\n"
              "com.a=freedreno\n"
              "   com.b   =   adreno   \n"
              "com.c=freedreno   # trailing comment\n"
              "broken line without equals\n"
              "*=adreno\n");
    Check("rw: exact match", vkswitch::LookupConfiguredSubname("com.a", rw, ro), "freedreno");
    Check("rw: whitespace around key/value", vkswitch::LookupConfiguredSubname("com.b", rw, ro), "adreno");
    Check("rw: trailing comment", vkswitch::LookupConfiguredSubname("com.c", rw, ro), "freedreno");
    Check("rw: unknown package uses *", vkswitch::LookupConfiguredSubname("com.unknown", rw, ro), "adreno");
    Check("rw: empty process name uses *", vkswitch::LookupConfiguredSubname("", rw, ro), "adreno");

    // --- 2. exact rule beats the other file's default -------------------------
    WriteFile(rw, "*=adreno\n");
    WriteFile(ro, "com.target=freedreno\n*=freedreno\n");
    Check("ro: exact beats rw default", vkswitch::LookupConfiguredSubname("com.target", rw, ro), "freedreno");
    Check("rw: default still wins over ro default", vkswitch::LookupConfiguredSubname("com.other", rw, ro), "adreno");

    // --- 3. rejected values leave the decision to the next source --------------
    WriteFile(rw,
              "com.path=../../evil\n"
              "com.case=FREEDRENO\n"
              "com.dots=free.dreno\n"
              "com.long=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\n"
              "com.empty=\n"
              "com.good=freedreno\n");
    WriteFile(ro, "com.path=adreno\n*=adreno\n");
    Check("reject: path traversal falls through", vkswitch::LookupConfiguredSubname("com.path", rw, ro), "adreno");
    Check("reject: uppercase rejected", vkswitch::LookupConfiguredSubname("com.case", rw, ro), "adreno");
    Check("reject: dot rejected", vkswitch::LookupConfiguredSubname("com.dots", rw, ro), "adreno");
    Check("reject: overlong rejected", vkswitch::LookupConfiguredSubname("com.long", rw, ro), "adreno");
    Check("reject: empty value ignored", vkswitch::LookupConfiguredSubname("com.empty", rw, ro), "adreno");
    Check("valid entry still works", vkswitch::LookupConfiguredSubname("com.good", rw, ro), "freedreno");

    // --- 4. no config at all --------------------------------------------------
    RemoveFile(rw);
    RemoveFile(ro);
    Check("no files: empty decision", vkswitch::LookupConfiguredSubname("com.a", rw, ro), "");

    RemoveFile(rw);
    RemoveFile(ro);

    if (g_failures) {
        std::printf("\n%d FAILURES\n", g_failures);
        return 1;
    }
    std::printf("\nall checks passed\n");
    return 0;
}
