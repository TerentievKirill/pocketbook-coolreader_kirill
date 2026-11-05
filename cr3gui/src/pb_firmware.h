#pragma once
#include <climits>
#include <string>

namespace pbfirmware {
struct Version {
    unsigned major, minor, build;
    Version() : major(0), minor(0), build(0) {}
};

inline bool number(const std::string &text, unsigned &value) {
    if (text.empty() || text.size() > 10) return false;
    value = 0;
    for (char ch : text) {
        if (ch < '0' || ch > '9' || value > (UINT_MAX - (ch - '0')) / 10) return false;
        value = value * 10 + ch - '0';
    }
    return true;
}

// Like KOReader, read the three numeric fields at the end of the software
// string. Model prefixes such as U743g must not be parsed as version numbers.
inline bool parse(const char *software, Version &version) {
    version = Version();
    if (!software) return false;
    const std::string text(software);
    if (text.size() > 256) return false;
    const size_t build = text.rfind('.');
    if (build == std::string::npos || !build) return false;
    const size_t minor = text.rfind('.', build - 1);
    if (minor == std::string::npos || !minor) return false;
    const size_t major = text.rfind('.', minor - 1);
    if (major == std::string::npos || !major) return false;
    Version parsed;
    if (!number(text.substr(major + 1, minor - major - 1), parsed.major) ||
        !number(text.substr(minor + 1, build - minor - 1), parsed.minor) ||
        !number(text.substr(build + 1), parsed.build) ||
        parsed.major == 0 || parsed.major > UINT_MAX / 100 - 1 || parsed.minor > 99) return false;
    version = parsed;
    return true;
}

// This port uses the common 6.10 interfaces, also checked on 6.11. Choose
// the newest known level <= installed firmware, as KOReader does. Patch/build
// numbers are qualification evidence rather than runtime admission rules.
inline unsigned apiLevel(const char *software) {
    Version version;
    if (!parse(software, version)) return 0;
    const unsigned installed = version.major * 100 + version.minor;
    return installed >= 611 ? 611 : installed >= 610 ? 610 : 0;
}
}
