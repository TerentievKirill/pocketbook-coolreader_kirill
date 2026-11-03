// Automatic checks reuse the asynchronous HTTPS transport, never Wi-Fi helpers.
// All timers are weak: they do not wake a sleeping/background reader.
namespace fw6background {
static bool active = false, loaded = false;
static time_t nextCheck = 0, promptedAt = 0;
static std::string promptedVersion, candidate, candidateChecksum;
static std::unique_ptr<WebTransfer> transfer;
static const time_t day = 24 * 60 * 60;
static const char *settings = FLASHDIR "/system/config/cr3/ota-check.txt";
}
static void OTA_backgroundTickFW6();
static time_t OTA_backgroundNowFW6() {
#ifdef OTA_BACKGROUND_TEST_CLOCK
    return OTA_BACKGROUND_TEST_CLOCK();
#else
    return time(NULL);
#endif
}
static void OTA_saveBackgroundFW6() {
    std::string temporary = std::string(fw6background::settings) + ".tmp";
    std::ofstream out(temporary.c_str());
    if (!out) return;
    out << fw6background::nextCheck << '\n' << fw6background::promptedAt << '\n'
        << (fw6background::promptedVersion.empty() ? "-" : fw6background::promptedVersion) << '\n';
    out.close();
    if (!out.fail()) rename(temporary.c_str(), fw6background::settings);
    unlink(temporary.c_str());
}
static void OTA_loadBackgroundFW6() {
    if (fw6background::loaded) return;
    fw6background::loaded = true;
    std::ifstream in(fw6background::settings);
    time_t next = 0, prompted = 0; std::string version; unsigned parts[3];
    time_t now = OTA_backgroundNowFW6();
    if (!(in >> next >> prompted >> version)) return;
    // A changed/reset device clock must not suppress updates indefinitely.
    if (next > 0 && next <= now + fw6background::day) fw6background::nextCheck = next;
    if (prompted >= 0 && prompted <= now && cr3ota::parseVersion(version, parts)) {
        fw6background::promptedAt = prompted; fw6background::promptedVersion = version;
    }
}
static void OTA_armBackgroundFW6(int milliseconds) {
    if (fw6background::active) SetWeakTimer("OTA_background", OTA_backgroundTickFW6, milliseconds);
}
static void OTA_interruptBackgroundFW6() {
    ClearTimer(OTA_backgroundTickFW6);
    if (fw6background::transfer) fw6background::transfer->cancel();
    fw6background::transfer.reset(); fw6background::candidate.clear(); fw6background::candidateChecksum.clear();
    OTA_armBackgroundFW6(30000);
}
void OTA_backgroundActive(bool active) {
    fw6background::active = active;
    if (!active) {
        ClearTimer(OTA_backgroundTickFW6);
        if (fw6background::transfer) fw6background::transfer->cancel();
        fw6background::transfer.reset();
        return;
    }
    OTA_loadBackgroundFW6(); OTA_armBackgroundFW6(30000);
}
static void OTA_backgroundTickFW6() {
    if (!fw6background::active) return;
    if (!pbCanRunBackgroundUpdate()) { OTA_interruptBackgroundFW6(); return; }
    if (!pbAutoUpdateEnabled()) { OTA_interruptBackgroundFW6(); return; }
    if (fw6ota::state != fw6ota::Idle) { OTA_interruptBackgroundFW6(); return; }
    if (getPbModelNumber() != lString32("632") || strcmp(GetSoftwareVersion(), "U632.6.11.2001")) return;
    time_t now = OTA_backgroundNowFW6();
    if (fw6background::transfer) {
        if (!fw6background::transfer->poll()) { OTA_armBackgroundFW6(200); return; }
        std::string version, checksum;
        bool valid = fw6background::transfer->succeeded() &&
            cr3ota::parseRelease(fw6background::transfer->body(), version, checksum);
        fw6background::transfer.reset();
        if (!valid) { OTA_armBackgroundFW6(30 * 60000); return; } // silent retry
        fw6background::nextCheck = now + fw6background::day; OTA_saveBackgroundFW6();
        if (cr3ota::newer(version, CR3_RELEASE_VERSION)) {
            fw6background::candidate = version; fw6background::candidateChecksum = checksum;
        }
    }
    if (!fw6background::candidate.empty()) {
        if (fw6background::candidate == fw6background::promptedVersion && now - fw6background::promptedAt < fw6background::day) {
            fw6background::candidate.clear();
        } else if (pbCanShowUpdatePrompt()) {
            // A staged update should not produce a second download prompt.
            std::ifstream ready(FW6_OTA_STAGE "/.ready"); std::string staged;
            if (!(std::getline(ready, staged) && staged == fw6background::candidate)) {
                fw6ota::version = fw6background::candidate; fw6ota::checksum = fw6background::candidateChecksum;
                fw6background::promptedVersion = fw6background::candidate; fw6background::promptedAt = now;
                OTA_saveBackgroundFW6(); fw6background::candidate.clear();
                OTA_confirmVersionFW6(true); OTA_armBackgroundFW6(30000); return;
            }
            fw6background::candidate.clear();
        } else { OTA_armBackgroundFW6(30000); return; }
    }
    if (now < fw6background::nextCheck) {
        // Re-evaluate the preference without making any network request.
        OTA_armBackgroundFW6(60000); return;
    }
    if (!pbCanShowUpdatePrompt()) { OTA_armBackgroundFW6(30000); return; }
    fw6background::nextCheck = now + 30 * 60; OTA_saveBackgroundFW6();
    fw6background::transfer.reset(new WebTransfer());
    // No pbNetwork("connect"): offline readers stay offline and errors stay silent.
    if (!fw6background::transfer->start(FW6_OTA_URL "ota/pb632-fw6.txt", "", 4096)) {
        fw6background::transfer.reset(); OTA_armBackgroundFW6(30 * 60000); return;
    }
    OTA_armBackgroundFW6(200);
}
