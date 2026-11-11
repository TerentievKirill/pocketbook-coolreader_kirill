#ifndef CR3_DICTIONARY_DOWNLOAD_CORE_H
#define CR3_DICTIONARY_DOWNLOAD_CORE_H
#include <openssl/evp.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include "stardict_install.h"
namespace cr3dict {
struct Entry {
    const char *label, *filename, *url, *sha256, *credit;
    size_t size;
    const char *format = "pocketbook";
    size_t installedSize = 0;
};
#include "dictionary_catalog.h"
inline std::string sizeLabel(size_t bytes) {
    char text[32]; snprintf(text, sizeof(text), "%.1f MB", bytes / 1048576.0); return text;
}
inline bool installed(const std::string &directory, const Entry &entry) {
    struct stat64 st;
    return lstat64((directory + "/" + entry.filename).c_str(), &st) == 0;
}
inline bool prepare(const std::string &directory, const Entry &entry, std::string &error) {
    if (mkdir(directory.c_str(), 0777) && errno != EEXIST) {
        error = "Could not create the dictionary folder. Check the reader's storage."; return false;
    }
    if (installed(directory, entry)) {
        error = "This dictionary is already installed. Your existing file has been kept."; return false;
    }
    struct statvfs space;
    if (statvfs(directory.c_str(), &space) ||
        static_cast<unsigned long long>(space.f_bavail) * space.f_frsize < entry.size + entry.installedSize + 1024 * 1024) {
        error = "Not enough free storage. Free some space on the reader and try again."; return false;
    }
    return true;
}
// Verify in small steps so the UI can handle Back/Cancel even on large packs.
class Installation {
    std::string temporary, directory, failure;
    const Entry &entry;
    FILE *file = NULL;
    EVP_MD_CTX *context = NULL;
    size_t bytes = 0;
    bool header = false, done = false, success = false;
    std::unique_ptr<StarDictInstallation> archive;
    bool fail(const char *text) { failure = text; done = true; return true; }
public:
    Installation(const std::string &path, const std::string &folder, const Entry &item)
        : temporary(path), directory(folder), entry(item) {}
    ~Installation() { if (file) fclose(file); if (context) EVP_MD_CTX_free(context); }
    size_t received() const { return bytes; }
    bool succeeded() const { return success; }
    const std::string &error() const { return failure; }
    bool poll() {
        if (done) return true;
        if (archive) {
            if (!archive->poll()) return false;
            success = archive->succeeded(); failure = archive->error(); done = true; return true;
        }
        if (!file) {
            file = fopen(temporary.c_str(), "rb");
            context = EVP_MD_CTX_new();
            if (!file || !context || EVP_DigestInit_ex(context, EVP_sha256(), NULL) != 1)
                return fail("The downloaded file could not be checked.");
        }
        unsigned char buffer[65536];
        for (int block = 0; block < 16; ++block) {
            size_t count = fread(buffer, 1, sizeof(buffer), file);
            if (!bytes) header = count >= 64 && (!strcmp(entry.format,"stardict") ? !memcmp(buffer,"PK\003\004",4) : !memcmp(buffer, "SDIC", 4));
            if (count > entry.size - bytes)
                return fail("The dictionary download is larger than expected.");
            bytes += count;
            if (EVP_DigestUpdate(context, buffer, count) != 1)
                return fail("The downloaded file could not be checked.");
            if (count == sizeof(buffer)) continue;
            bool readOK = !ferror(file); fclose(file); file = NULL;
            unsigned char digest[32]; unsigned length = 0;
            if (EVP_DigestFinal_ex(context, digest, &length) != 1 || length != 32)
                return fail("The downloaded file could not be checked.");
            char checksum[65];
            for (unsigned i = 0; i < length; ++i) snprintf(checksum + 2*i, 3, "%02x", digest[i]);
            if (!readOK || !header || bytes != entry.size || entry.sha256 != std::string(checksum))
                return fail("The dictionary download could not be verified. Please try again.");
            if (!strcmp(entry.format,"stardict")) {
                archive.reset(new StarDictInstallation(temporary,directory,entry.filename));
                return false;
            }
            if (installed(directory, entry))
                return fail("A dictionary with this filename already exists. Your existing file has been kept.");
            int fd = open(temporary.c_str(), O_RDWR);
            bool saved = fd >= 0 && fsync(fd) == 0;
            if (fd >= 0 && close(fd)) saved = false;
            if (!saved || rename(temporary.c_str(), (directory + "/" + entry.filename).c_str()))
                return fail("Could not finish installing the dictionary. Check the reader's storage and try again.");
            success = done = true;
            return true;
        }
        return false;
    }
};
inline bool install(const std::string &temporary, const std::string &directory,
                    const Entry &entry, std::string &error) {
    Installation installation(temporary, directory, entry);
    while (!installation.poll()) {}
    error = installation.error(); return installation.succeeded();
}
}
#endif
