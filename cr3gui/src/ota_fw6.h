#include "ota_fw6_core.h"
#include <memory>
#include <ctime>
#include "ota_policy.h"
static void OTA_restoreAfterDialogFW6() { SetHardTimer("OTA_restore",pbRestoreUpdateScreen,150); }
static void OTA_resultDismissed(int) { OTA_restoreAfterDialogFW6(); }
static void OTA_result(int icon, const char *text) {
    CRLog::info("OTA result: %s", text);
    Dialog(icon, "CoolReader OTA Update", text, _("OK"), NULL, OTA_resultDismissed);
}
namespace fw6ota {
enum State { Idle, Checking, Confirming, Downloading, Verifying, RestartPrompt };
static State state = Idle;
static std::unique_ptr<WebTransfer> transfer;
static std::string version, checksum;
static bool progressOpen = false;
static int lastPercent = -1;
static time_t lastPaint = 0;
}
static void OTA_showVersionFW6() {
    Dialog(ICON_INFORMATION, _("Version"), "CoolReader " CR3_RELEASE_VERSION,
           _("OK"), NULL, OTA_resultDismissed);
}
static void OTA_interruptBackgroundFW6();
static void OTA_pollFW6();
static void OTA_startDownloadFW6();
static void OTA_closeProgressFW6() {
    if (fw6ota::progressOpen) { fw6ota::progressOpen = false; CloseProgressbar(); pbRestoreUpdateScreen(); }
}
static void OTA_finishFW6(int icon, const std::string &message) {
    ClearTimer(OTA_pollFW6); ClearTimer(OTA_startDownloadFW6);
    fw6ota::transfer.reset(); fw6ota::state = fw6ota::Idle;
    OTA_closeProgressFW6();
    OTA_result(icon, message.c_str());
}
static void OTA_cancelFW6(int) {
    if (fw6ota::state != fw6ota::Checking && fw6ota::state != fw6ota::Downloading) return;
    if (fw6ota::transfer) fw6ota::transfer->cancel();
    unlink(FLASHDIR "/.cr3-ota.zip");
    OTA_finishFW6(ICON_INFORMATION, "Update cancelled. Your installation is unchanged.");
}
static void OTA_restartConfirmedFW6(int button) {
    fw6ota::state = fw6ota::Idle;
    if (button == 1) OTA_DL_dialog_handler(button);
    else OTA_restoreAfterDialogFW6();
}
static void OTA_offerRestartFW6() {
    fw6ota::state = fw6ota::RestartPrompt;
    std::string text = "Version " + fw6ota::version + " is verified and ready.\nRestart now to install it, or choose Later to install it the next time CoolReader starts.";
    Dialog(ICON_INFORMATION, "CoolReader OTA Update", text.c_str(), "Restart now", "Later", OTA_restartConfirmedFW6);
}
static void OTA_downloadConfirmedFW6(int button) {
    if (fw6ota::state != fw6ota::Confirming) return;
    if (button != 1) { fw6ota::state = fw6ota::Idle; OTA_restoreAfterDialogFW6(); return; }
    fw6ota::state = fw6ota::Downloading;
    SetHardTimer("OTA_download", OTA_startDownloadFW6, 150);
}
static void OTA_confirmVersionFW6(bool automatic) {
    fw6ota::state = fw6ota::Confirming;
    std::string text = "Installed: " CR3_RELEASE_VERSION "\nAvailable: " + fw6ota::version + "\nDownload this update? Your settings and reading history will be preserved. You can choose when to restart after verification.";
    Dialog(ICON_INFORMATION, "CoolReader OTA Update", text.c_str(), "Download", automatic ? "Later" : "Cancel", OTA_downloadConfirmedFW6);
}
static void OTA_startDownloadFW6() {
    if (fw6ota::state != fw6ota::Downloading) return;
    fw6ota::lastPercent = -1; fw6ota::lastPaint = 0;
    fw6ota::transfer.reset(new WebTransfer());
    pbRestoreUpdateScreen();
    OpenProgressbar(ICON_INFORMATION, "OTA Update", "Connecting to the download server...", 0, OTA_cancelFW6);
    fw6ota::progressOpen = true;
    std::string url = std::string(FW6_OTA_URL) + CR3_OTA_DOWNLOADS + fw6ota::version + "/" CR3_OTA_PACKAGE;
    if (!fw6ota::transfer->start(url, FLASHDIR "/.cr3-ota.zip", 64 * 1024 * 1024)) {
        OTA_finishFW6(ICON_ERROR, "Could not start the download.\n" + fw6ota::transfer->error()); return;
    }
    SetHardTimer("OTA_poll", OTA_pollFW6, 100);
}
static void OTA_pollFW6() {
    if (!fw6ota::transfer) return;
    bool done = fw6ota::transfer->poll();
    if (fw6ota::state == fw6ota::Downloading) {
        long long received = fw6ota::transfer->received(), total = fw6ota::transfer->total();
        int percent = total > 0 ? static_cast<int>(received * 100 / total) : 0;
        if (percent > 100) percent = 100;
        time_t now = time(NULL);
        if (done || (now != fw6ota::lastPaint && (percent != fw6ota::lastPercent || total <= 0))) {
            std::string label = "Downloading " + fw6ota::version + ": " + std::to_string(received / 1024) + " KB";
            if (total > 0) label += " / " + std::to_string(total / 1024) + " KB (" + std::to_string(percent) + "%)";
            UpdateProgressbar(label.c_str(), percent);
            fw6ota::lastPaint = now; fw6ota::lastPercent = percent;
        }
    }
    if (!done) { SetHardTimer("OTA_poll", OTA_pollFW6, 100); return; }
    if (!fw6ota::transfer->succeeded()) {
        std::string detail = fw6ota::transfer->error();
        unlink(FLASHDIR "/.cr3-ota.zip");
        OTA_finishFW6(ICON_ERROR, "Could not " + std::string(fw6ota::state == fw6ota::Checking ? "check for updates." : "download the update.") + "\n" + detail + "\nYour installation is unchanged."); return;
    }
    if (fw6ota::state == fw6ota::Checking) {
        std::string response = fw6ota::transfer->body();
        fw6ota::transfer.reset(); OTA_closeProgressFW6();
        if (!OTA_parseReleaseFW6(response, fw6ota::version, fw6ota::checksum)) {
            OTA_finishFW6(ICON_ERROR, "The update server returned invalid release information or no compatible update for this reader. Please try again later."); return;
        }
        if (!cr3ota::newer(fw6ota::version, CR3_RELEASE_VERSION)) {
            OTA_finishFW6(ICON_INFORMATION, "You have the latest version (" CR3_RELEASE_VERSION ")."); return;
        }
        OTA_confirmVersionFW6(false);
        return;
    }
    fw6ota::transfer.reset(); fw6ota::state = fw6ota::Verifying;
    OTA_closeProgressFW6();
    OpenProgressbar(ICON_INFORMATION, "OTA Update", "Verifying and preparing the update...", 100, NULL);
    fw6ota::progressOpen = true;
    bool ok = otaStagePackage(fw6ota::checksum, fw6ota::version,
                             UnicodeToUtf8(getPbModelNumber()).c_str(), GetSoftwareVersion());
    unlink(FLASHDIR "/.cr3-ota.zip");
    if (!ok) {
        otaRemoveTree(FW6_OTA_STAGE);
        OTA_finishFW6(ICON_ERROR, otaStageError + "\nYour installation is unchanged."); return;
    }
    std::ofstream ready(FW6_OTA_STAGE "/.ready"); ready << fw6ota::version << '\n'; ready.close();
    if (ready.fail()) {
        otaRemoveTree(FW6_OTA_STAGE);
        OTA_finishFW6(ICON_ERROR, "Could not save the staged update. Check available storage.\nYour installation is unchanged."); return;
    }
    OTA_closeProgressFW6(); OTA_offerRestartFW6();
}
static bool OTA_updateFW6(const char *branch) {
    if (fw6ota::state != fw6ota::Idle) return false;
    OTA_interruptBackgroundFW6();
    if (strcmp(branch, OTA_BRANCH_STABLE) || !OTA_deviceEligibleFW6()) {
        std::string detail = "No update for this model, firmware or channel.\nModel: ";
        detail += UnicodeToUtf8(getPbModelNumber()).c_str(); detail += "\nFirmware: "; detail += GetSoftwareVersion();
        OTA_result(ICON_WARNING, detail.c_str()); return false;
    }
    std::ifstream ready(FW6_OTA_STAGE "/.ready"); std::string staged;
    if (std::getline(ready, staged) && cr3ota::newer(staged, CR3_RELEASE_VERSION) && access(FW6_OTA_STAGE "/bin/cr3-pb.app", X_OK) == 0) {
        fw6ota::version = staged; OTA_offerRestartFW6(); return true;
    }
    if (!pbNetwork("connect")) { OTA_result(ICON_ERROR, "Couldn't connect to Wi-Fi. Check the reader's network settings and try again."); return false; }
    fw6ota::state = fw6ota::Checking;
    pbRestoreUpdateScreen();
    OpenProgressbar(ICON_INFORMATION, "OTA Update", "Checking for updates...", 0, OTA_cancelFW6);
    fw6ota::progressOpen = true;
    fw6ota::transfer.reset(new WebTransfer());
    if (!fw6ota::transfer->start(FW6_OTA_URL CR3_OTA_CHANNEL, "", otaManifestLimit)) {
        OTA_finishFW6(ICON_ERROR, "Could not check for updates.\n" + fw6ota::transfer->error()); return false;
    }
    SetHardTimer("OTA_poll", OTA_pollFW6, 100); return true;
}
