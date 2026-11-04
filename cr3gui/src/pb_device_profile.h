#pragma once
#include <algorithm>
#include <cctype>
#include <string>

namespace pbdevice {
struct Profile {
    const char *model;
    const char *name;
    int dpi, width, height;
    const char *skin;
    const char *firmware;
};
#include "pb_device_data.h"

inline std::string modelNumber(const char *description) {
    std::string model = description ? description : "";
    const std::string::size_type open = model.rfind("(PB");
    if (open != std::string::npos && model.find(')', open) != std::string::npos)
        model = model.substr(open + 1, model.find(')', open) - open - 1);
    model.erase(std::remove_if(model.begin(), model.end(), [](unsigned char ch) { return std::isspace(ch); }), model.end());
    if (model.compare(0, 10, "PocketBook") == 0) model.erase(0, 10);
    if (model.compare(0, 2, "PB") == 0) model.erase(0, 2);
    std::transform(model.begin(), model.end(), model.begin(), [](unsigned char ch) { return std::toupper(ch); });
    return model;
}

inline const Profile *identify(const char *description) {
    const std::string model = modelNumber(description);
    for (const Profile &profile : profiles)
        if (model == profile.model) return &profile;
    return NULL;
}

inline bool qualified(const char *description, const char *firmware) {
    const Profile *profile = identify(description);
    return profile && firmware && std::string(firmware) == profile->firmware;
}

// Only fill missing defaults. Saved user/book properties are loaded afterwards.
template <typename Properties>
inline void applyDefaults(Properties defaults, const Profile &profile) {
    defaults->setIntDef("crengine.font.size", (42 * profile.dpi + 150) / 300);
    defaults->setIntDef("crengine.page.header.font.size", (20 * profile.dpi + 150) / 300);
    const int margin = (24 * profile.dpi + 150) / 300;
    defaults->setIntDef("crengine.page.margin.left", margin);
    defaults->setIntDef("crengine.page.margin.right", margin);
    defaults->setIntDef("crengine.page.margin.top", margin);
    defaults->setIntDef("crengine.page.margin.bottom", margin);
}
}
