#ifndef CR3_OTA_VERSION_H
#define CR3_OTA_VERSION_H
#include <string>
#include <sstream>
#include <set>
#include <utility>
#include <map>
#include "pb_firmware.h"
namespace cr3ota {
// Stable channel uses SemVer MAJOR.MINOR.PATCH, without leading zeroes.
inline bool parseVersion(const std::string &text, unsigned parts[3]) {
    std::istringstream input(text);
    std::string part;
    for (int i = 0; i < 3; ++i) {
        if (!std::getline(input, part, '.') || part.empty() || part.size() > 6 ||
            (part.size() > 1 && part[0] == '0')) return false;
        parts[i] = 0;
        for (size_t j = 0; j < part.size(); ++j) {
            if (part[j] < '0' || part[j] > '9') return false;
            parts[i] = parts[i] * 10 + part[j] - '0';
        }
    }
    return text.back() != '.' && input.peek() == EOF;
}
inline bool parseRelease(const std::string &text, std::string &version, std::string &checksum) {
    version.clear(); checksum.clear();
    if (text.size() > 256) return false;
    std::istringstream input(text); std::string extra; unsigned parts[3];
    input >> version >> checksum;
    return parseVersion(version, parts) && checksum.size() == 64 &&
        checksum.find_first_not_of("0123456789abcdef") == std::string::npos && !(input >> extra);
}
struct SharedRelease {
    std::string family, version, checksum;
    std::set<std::pair<std::string, std::string> > devices;
    std::map<std::string, unsigned> minimumApis;
};
inline bool parseSharedRelease(const std::string &text, SharedRelease &release) {
    release = SharedRelease();
    if (text.size() > 16384) return false;
    std::istringstream input(text);
    std::string magic, model, firmware;
    unsigned parts[3];
    if (!(input >> magic >> release.family >> release.version >> release.checksum) ||
        (magic != "CR3-OTA-SHARED-1" && magic != "CR3-OTA-SHARED-2") || release.family.size() > 64 ||
        !parseVersion(release.version, parts) || release.checksum.size() != 64 ||
        release.checksum.find_first_not_of("0123456789abcdef") != std::string::npos) return false;
    while (input >> model) {
        if (!(input >> firmware) || model.empty() || model.size() > 16 ||
            model.find_first_not_of("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ") != std::string::npos) return false;
        if (magic == "CR3-OTA-SHARED-2") {
            unsigned minimum;
            if (!pbfirmware::number(firmware, minimum) || minimum < 610 || minimum > 9999 ||
                !release.minimumApis.insert(std::make_pair(model, minimum)).second || release.minimumApis.size() > 128)
                return false;
            continue;
        }
        if (
            firmware.size() > 64 || firmware.empty() || firmware[0] != 'U' ||
            firmware.find_first_not_of("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_.") != std::string::npos ||
            !release.devices.insert(std::make_pair(model, firmware)).second || release.devices.size() > 128) return false;
    }
    return !release.devices.empty() || !release.minimumApis.empty();
}
inline bool compatible(const SharedRelease &release, const std::string &family,
                       const std::string &model, const std::string &firmware) {
    if (release.family != family) return false;
    if (!release.minimumApis.empty()) {
        const auto rule = release.minimumApis.find(model);
        return rule != release.minimumApis.end() && pbfirmware::apiLevel(firmware.c_str()) >= rule->second;
    }
    return release.devices.count(std::make_pair(model, firmware)) != 0;
}
inline bool newer(const std::string &candidate, const std::string &installed) {
    unsigned a[3], b[3];
    if (!parseVersion(candidate, a) || !parseVersion(installed, b)) return false;
    for (int i = 0; i < 3; ++i) if (a[i] != b[i]) return a[i] > b[i];
    return false;
}
}
#endif
