#include "ota_fw6_core.h"
static bool OTA_updateFW6(const char *branch) {
    if (strcmp(branch, OTA_BRANCH_STABLE) || getPbModelNumber() != lString32("632") ||
        strcmp(GetSoftwareVersion(), "U632.6.11.2001")) {
        Message(ICON_WARNING, "CoolReader", "No update for this model, firmware or channel.", 3000); return false;
    }
    if (!pbNetwork("connect")) { Message(ICON_ERROR, "CoolReader", "Couldn't connect to the network!", 3000); return false; }
    OpenProgressbar(ICON_INFORMATION, "OTA Update", "Checking for updates...", 0, NULL);
    // Small, strict channel descriptor: revision followed by archive SHA-256.
    std::string response = web::get(FW6_OTA_URL "ota/pb632-fw6.txt");
    std::istringstream manifest(response); std::string revision, checksum, extra;
    unsigned versionParts[3];
    manifest >> revision >> checksum;
    if (response.size() > 256 || !cr3ota::parseVersion(revision, versionParts) || checksum.size() != 64 || checksum.find_first_not_of("0123456789abcdef") != std::string::npos || (manifest >> extra)) {
        CloseProgressbar(); Message(ICON_ERROR, "CoolReader", "Couldn't read update information. Please try again later.", 4000); return false;
    }
    if (!cr3ota::newer(revision, CR3_RELEASE_VERSION)) {
        CloseProgressbar(); Message(ICON_INFORMATION, "CoolReader", "You have the latest version.", 2000); return false;
    }
    UpdateProgressbar("Downloading update...", 0);
    std::string url = std::string(FW6_OTA_URL) + "downloads/coolreader/" + revision + "/coolreader-touch-hd3.zip";
    bool ok = web::getBinary(url, FLASHDIR "/.cr3-ota.zip") && otaStagePackage(checksum, revision);
    unlink(FLASHDIR "/.cr3-ota.zip");
    if (ok) {
        std::ofstream ready(FW6_OTA_STAGE "/.ready"); ready << revision << '\n'; ready.close(); ok = !ready.fail();
    }
    if (!ok) {
        otaRemoveTree(FW6_OTA_STAGE); CloseProgressbar();
        Message(ICON_ERROR, "CoolReader", "Update failed verification or staging. Your installation is unchanged.", 4000); return false;
    }
    OTA_downloadSuccessful(); return true;
}
