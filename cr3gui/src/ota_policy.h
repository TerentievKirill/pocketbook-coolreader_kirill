#pragma once
#include "ota_version.h"

#ifdef CR3_SHARED_PACKAGE
#include "pb_device_profile.h"
#ifndef CR3_OTA_FAMILY
#define CR3_OTA_FAMILY "pocketbook-armv7-softfp"
#endif
#define CR3_OTA_CHANNEL "ota/pocketbook-armv7-softfp-api.txt"
#define CR3_OTA_PACKAGE "coolreader-pocketbook-armv7-softfp.zip"
#define CR3_OTA_DOWNLOADS "downloads/coolreader-shared/"
static const size_t otaManifestLimit = 16384;
#else
#define CR3_OTA_CHANNEL "ota/pb632-fw6.txt"
#define CR3_OTA_PACKAGE "coolreader-touch-hd3.zip"
#define CR3_OTA_DOWNLOADS "downloads/coolreader/"
static const size_t otaManifestLimit = 4096;
#endif

static bool OTA_deviceEligibleFW6() {
#ifdef CR3_SHARED_PACKAGE
    return pbdevice::supported(UnicodeToUtf8(getPbModelNumber()).c_str(), GetSoftwareVersion());
#else
    return getPbModelNumber() == lString32("632") && !strcmp(GetSoftwareVersion(), "U632.6.11.2001");
#endif
}
static bool OTA_parseReleaseFW6(const std::string &text, std::string &version, std::string &checksum) {
#ifdef CR3_SHARED_PACKAGE
    cr3ota::SharedRelease release;
    if (!cr3ota::parseSharedRelease(text, release) || !cr3ota::compatible(release, CR3_OTA_FAMILY,
            UnicodeToUtf8(getPbModelNumber()).c_str(), GetSoftwareVersion())) return false;
    version = release.version;
    checksum = release.checksum;
    return true;
#else
    return cr3ota::parseRelease(text, version, checksum);
#endif
}
