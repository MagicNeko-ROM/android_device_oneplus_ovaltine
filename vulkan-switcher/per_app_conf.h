/*
 * Parsing for /data/misc/vulkan/per_app.conf and /vendor/etc/vulkan/per_app.conf.
 *
 * Header-only and dependency-free on purpose: vulkan.switcher.cpp includes it
 * for the real thing, tests/test_conf.cpp includes it on the host to check the
 * rules without a device.
 */

#ifndef VULKAN_SWITCHER_PER_APP_CONF_H
#define VULKAN_SWITCHER_PER_APP_CONF_H

#include <fstream>
#include <string>

namespace vkswitch {

inline std::string Trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const auto begin = s.find_first_not_of(ws);
    if (begin == std::string::npos)
        return "";
    const auto end = s.find_last_not_of(ws);
    return s.substr(begin, end - begin + 1);
}

// The value becomes part of a library name (vulkan.<value>.so), so anything
// that could escape /vendor/lib64/hw/ or unbalance the name is rejected here.
inline bool IsValidSubname(const std::string& s) {
    if (s.empty() || s.size() > 32)
        return false;
    for (const char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-')
            continue;
        return false;
    }
    return true;
}

// want_exact selects between "pkg=<subname>" and "*=<subname>".
// Comments start with '#' anywhere on the line; unparsable lines are skipped.
inline std::string ParseConf(const char* path, const std::string& pkg, bool want_exact) {
    if (!path)
        return "";
    std::ifstream f(path);
    if (!f)
        return "";

    std::string line;
    while (std::getline(f, line)) {
        const auto hash = line.find('#');
        if (hash != std::string::npos)
            line.resize(hash);
        line = Trim(line);
        if (line.empty())
            continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = Trim(line.substr(0, eq));
        const std::string value = Trim(line.substr(eq + 1));
        if (key.empty() || !IsValidSubname(value))
            continue;
        if (want_exact) {
            if (!pkg.empty() && key == pkg)
                return value;
        } else if (key == "*") {
            return value;
        }
    }
    return "";
}

// Writable override first, then the image default; within each file an exact
// package rule beats the "*" line.
inline std::string LookupConfiguredSubname(const std::string& pkg,
                                           const char* rw_path,
                                           const char* ro_path) {
    if (!pkg.empty()) {
        std::string value = ParseConf(rw_path, pkg, true);
        if (!value.empty())
            return value;
        value = ParseConf(ro_path, pkg, true);
        if (!value.empty())
            return value;
    }
    std::string value = ParseConf(rw_path, pkg, false);
    if (!value.empty())
        return value;
    return ParseConf(ro_path, pkg, false);
}

}  // namespace vkswitch

#endif  // VULKAN_SWITCHER_PER_APP_CONF_H
