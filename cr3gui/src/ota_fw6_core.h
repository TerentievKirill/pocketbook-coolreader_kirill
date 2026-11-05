// SHA-256-verified FW6 updates are staged before the launcher switches directories.
#include <openssl/sha.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cerrno>
#include <set>
#include <fstream>
#include <sstream>
#include "ota_version.h"
#ifdef CR3_SHARED_PACKAGE
#define CR3_OTA_FAMILY "pocketbook-armv7-softfp"
#define FW6_OTA_STAGE FLASHDIR "/system/share/cr3-ota-shared-new"
#else
#define FW6_OTA_STAGE FLASHDIR "/system/share/cr3-ota-new"
#endif
#define FW6_OTA_URL "https://pocketbook.nimbushub.eu/"

static bool otaRemoveTree(const std::string &path) {
    struct stat64 st;
    if (lstat64(path.c_str(), &st)) { if (errno == ENOENT) return true; fprintf(stderr, "OTA: cannot stat %s: %s\n", path.c_str(), strerror(errno)); return false; }
    if (!S_ISDIR(st.st_mode)) return unlink(path.c_str()) == 0;
    DIR *dir = opendir(path.c_str());
    if (!dir) return false;
    bool ok = true;
    while (struct dirent64 *entry = readdir64(dir)) {
        std::string name(entry->d_name);
        if (name != "." && name != "..") ok = otaRemoveTree(path + "/" + name) && ok;
    }
    closedir(dir);
    if (!ok || rmdir(path.c_str())) { fprintf(stderr, "OTA: cannot remove %s: %s\n", path.c_str(), strerror(errno)); return false; }
    return true;
}

static std::string otaStageError;
static bool otaStageFailure(const char *text) { otaStageError = text; return false; }
static bool otaStagePackage(const std::string &checksum, const std::string &revision,
                            const std::string &model = "", const std::string &firmware = "") {
    otaStageError = "The update archive is invalid or contains unsafe paths.";
    const char *zip = FLASHDIR "/.cr3-ota.zip";
    FILE *file = fopen(zip, "rb");
    if (!file) return otaStageFailure("Could not read the downloaded package.");
    SHA256_CTX ctx; SHA256_Init(&ctx);
    unsigned char buffer[32768], hash[SHA256_DIGEST_LENGTH];
    size_t count;
    while ((count = fread(buffer, 1, sizeof(buffer), file))) SHA256_Update(&ctx, buffer, count);
    bool ok = !ferror(file); fclose(file); SHA256_Final(hash, &ctx);
    char hex[65];
    for (int i = 0; i < 32; ++i) sprintf(hex + i * 2, "%02x", hash[i]);
    if (!ok) return otaStageFailure("Could not read the downloaded package.");
    if (checksum != hex) return otaStageFailure("Checksum mismatch. The download may be damaged. Please try again.");
    if (!otaRemoveTree(FW6_OTA_STAGE) || mkdir(FW6_OTA_STAGE, 0755)) return otaStageFailure("Could not create staging files. Check free storage and USB storage access.");
    LVContainerRef archive = LVOpenArchieve(LVOpenFileStream(zip, LVOM_READ));
    if (archive.isNull()) return false;
    std::set<std::string> names;
    unsigned long long total = 0;
    for (int i = 0; i < archive->GetObjectCount(); ++i) {
        const LVContainerItemInfo *info = archive->GetObjectInfo(i);
        std::string name = UnicodeToUtf8(lString32(info->GetName())).c_str();
        const std::string prefix = "system/share/cr3/";
        if (name.compare(0, prefix.size(), prefix)) continue;
        name.erase(0, prefix.size());
        if (name.empty() || name[0] == '/' || name.find('\\') != std::string::npos || !names.insert(name).second) return false;
        std::istringstream parts(name); std::string part, target = FW6_OTA_STAGE;
        while (std::getline(parts, part, '/')) {
            if (part.empty() || part == "." || part == "..") return false;
            target += "/" + part;
            if (parts.peek() != EOF && mkdir(target.c_str(), 0755) && errno != EEXIST) return otaStageFailure("Could not create staging folders. Check free storage.");
        }
        LVStreamRef input = archive->OpenStream(info->GetName(), LVOM_READ);
        if (input.isNull() || input->GetSize() > 64 * 1024 * 1024) return false;
        total += input->GetSize(); if (total > 128 * 1024 * 1024) return false;
        FILE *out = fopen(target.c_str(), "wb"); if (!out) return otaStageFailure("Could not write staging files. Check free storage.");
        lvsize_t remaining = input->GetSize();
        while (remaining) {
            lvsize_t n = 0;
            if (input->Read(buffer, remaining < sizeof(buffer) ? remaining : sizeof(buffer), &n) != LVERR_OK || !n || fwrite(buffer, 1, n, out) != n) { fclose(out); return otaStageFailure("Could not unpack or save the update. Check free storage and try downloading again."); }
            remaining -= n;
        }
        if (fclose(out)) return otaStageFailure("Could not save staging files. Check free storage.");
        if ((name == "bin/cr3-pb.app" || name == "bin/cr3-pb-executable.app") && chmod(target.c_str(), 0755)) return false;
    }
    std::ifstream versionFile(FW6_OTA_STAGE "/release-version.txt");
    std::string packageVersion; std::getline(versionFile, packageVersion);
    if (packageVersion != revision || !names.count("bin/cr3-pb.app") || !names.count("cr3-defaults.ini") || !names.count("skins/pb631fw5.cr3skin"))
        return otaStageFailure("The downloaded package is for a different version or is incomplete.");
#ifdef CR3_SHARED_PACKAGE
    std::ifstream familyFile(FW6_OTA_STAGE "/package-family.txt");
    std::string family; std::getline(familyFile, family);
    const bool apiPolicy = names.count("firmware-api-compatibility.txt") != 0;
    std::ifstream compatibilityFile(apiPolicy ? FW6_OTA_STAGE "/firmware-api-compatibility.txt"
                                             : FW6_OTA_STAGE "/firmware-compatibility.txt");
    std::string compatibility((std::istreambuf_iterator<char>(compatibilityFile)), std::istreambuf_iterator<char>());
    cr3ota::SharedRelease release;
    if (family != CR3_OTA_FAMILY || !cr3ota::parseSharedRelease(
            std::string(apiPolicy ? "CR3-OTA-SHARED-2\n" : "CR3-OTA-SHARED-1\n") + family + "\n" + revision + "\n" + checksum + "\n" + compatibility, release) ||
            !cr3ota::compatible(release, CR3_OTA_FAMILY, model, firmware))
        return otaStageFailure("The downloaded package is not compatible with this reader and firmware.");
#endif
    return true;
}

