#ifndef CR3_OTA_VERSION_H
#define CR3_OTA_VERSION_H
#include <string>
#include <sstream>
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
inline bool newer(const std::string &candidate, const std::string &installed) {
    unsigned a[3], b[3];
    if (!parseVersion(candidate, a) || !parseVersion(installed, b)) return false;
    for (int i = 0; i < 3; ++i) if (a[i] != b[i]) return a[i] > b[i];
    return false;
}
}
#endif
