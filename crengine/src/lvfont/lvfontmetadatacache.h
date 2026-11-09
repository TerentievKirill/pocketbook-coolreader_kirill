#ifndef LV_FONT_METADATA_CACHE_H
#define LV_FONT_METADATA_CACHE_H

#include "lvfontdef.h"
#include "props.h"
#include "lvstreamutils.h"
#include <map>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>

// Disposable registry of font headers, never glyphs or document fonts. Every
// hit requires the same file identity, size and nanosecond modification/change
// times. Bad/old caches fall back to normal FreeType registration.
class LVFontMetadataCache {
    CRPropRef _old, _next;
    lString32 _filename;
    std::map<std::string, int> _files;
    int _count = 0;
    int _hits = 0;
    static lString8 key(int index, const char *field) {
        return lString8("font.") + fmt::decimal(index) + "." + field;
    }
    static lUInt32 checksum(CRPropRef props) {
        lUInt32 crc = 0;
        for (int i = 0; i < props->getCount(); ++i) {
            const char *name = props->getName(i);
            if (!strcmp(name, "checksum")) continue;
            lString8 value = UnicodeToUtf8(props->getValue(i));
            crc = lStr_crc32(crc, name, strlen(name) + 1);
            crc = lStr_crc32(crc, value.c_str(), value.length() + 1);
        }
        return crc;
    }
public:
    LVFontMetadataCache() : _old(LVCreatePropsContainer()), _next(LVCreatePropsContainer()) {}
    static lString8 fingerprint(const lString8 &path) {
#if defined(__linux__) || defined(__APPLE__)
        struct stat info;
        if (stat(path.c_str(), &info) || !S_ISREG(info.st_mode)) return lString8();
#ifdef __APPLE__
        long mns = info.st_mtimespec.tv_nsec, cns = info.st_ctimespec.tv_nsec;
#else
        long mns = info.st_mtim.tv_nsec, cns = info.st_ctim.tv_nsec;
#endif
        char stamp[256];
        snprintf(stamp, sizeof(stamp), "%llu:%llu:%lld:%lld:%ld:%lld:%ld",
                 (unsigned long long)info.st_dev, (unsigned long long)info.st_ino,
                 (long long)info.st_size, (long long)info.st_mtime, mns,
                 (long long)info.st_ctime, cns);
        return lString8(stamp);
#else
        return lString8();
#endif
    }
    void begin(lString32 filename, lString8 schema) {
        _old = LVCreatePropsContainer(); _next = LVCreatePropsContainer();
        _files.clear(); _count = 0; _hits = 0; _filename = filename;
        _next->setString("schema", schema.c_str());
        if (filename.empty()) return;
        LVStreamRef file = LVOpenFileStream(filename.c_str(), LVOM_READ);
        if (!file.isNull() && file->GetSize() <= 2 * 1024 * 1024)
            _old->loadFromStream(file.get());
        int count = _old->getIntDef("count", -1);
        if (_old->getStringDef("schema") != lString32(schema.c_str()) || count < 0 || count > 4096 ||
            _old->getInt64Def("checksum", -1) != checksum(_old)) {
            _old->clear();
            return;
        }
        for (int i = 0; i < count; ++i)
            _files[UnicodeToUtf8(_old->getStringDef(key(i, "path").c_str())).c_str()] = i;
    }
    int hits() const { return _hits; }
    bool enabled() const { return !_filename.empty(); }
    bool lookup(lString8 name, lString8 stamp, std::vector<LVFontDef> &defs) {
        if (_filename.empty() || stamp.empty()) return false;
        auto found = _files.find(name.c_str());
        if (found == _files.end()) return false;
        int index = found->second;
        if (_old->getStringDef(key(index, "stamp").c_str()) != lString32(stamp.c_str())) return false;
        int count = _old->getIntDef(key(index, "faces").c_str());
        if (count < 1 || count > 256) return false;
        std::vector<LVFontDef> parsed;
        for (int i = 0; i < count; ++i) {
            lString8 prefix = key(index, "face.") + fmt::decimal(i) + ".";
            int weight = _old->getIntDef((prefix + "weight").c_str(), -1);
            int italic = _old->getIntDef((prefix + "italic").c_str(), -1);
            int family = _old->getIntDef((prefix + "family").c_str(), -1);
            int face = _old->getIntDef((prefix + "index").c_str(), -1);
            lString8 typeface = UnicodeToUtf8(_old->getStringDef((prefix + "name").c_str()));
            if (weight < 1 || weight > 1000 || italic < 0 || italic > 2 || family < css_ff_serif ||
                family > css_ff_monospace || face < 0 || face > 65535 || typeface.empty() || typeface.length() > 1024)
                return false;
            parsed.push_back(LVFontDef(name, -1, weight, italic, -1, (css_font_family_t)family, typeface, face));
        }
        defs.swap(parsed);
        ++_hits;
        return true;
    }
    void remember(lString8 name, lString8 stamp, const std::vector<LVFontDef> &defs) {
        if (_filename.empty() || stamp.empty() || defs.empty() || defs.size() > 256 || _count >= 4096) return;
        int index = _count++;
        _next->setString(key(index, "path").c_str(), name.c_str());
        _next->setString(key(index, "stamp").c_str(), stamp.c_str());
        _next->setInt(key(index, "faces").c_str(), defs.size());
        for (size_t i = 0; i < defs.size(); ++i) {
            const LVFontDef &def = defs[i];
            lString8 prefix = key(index, "face.") + fmt::decimal(i) + ".";
            _next->setInt((prefix + "weight").c_str(), def.getWeight());
            _next->setInt((prefix + "italic").c_str(), def.getItalic() ? (def.isRealItalic() ? 1 : 2) : 0);
            _next->setInt((prefix + "family").c_str(), def.getFamily());
            _next->setInt((prefix + "index").c_str(), def.getIndex());
            _next->setString((prefix + "name").c_str(), def.getTypeFace().c_str());
        }
    }
    bool save() {
        if (_filename.empty() || !_count) return true;
        _next->setInt("count", _count);
        _next->setInt64("checksum", checksum(_next));
        if ((_old ^ _next)->getCount() == 0 && (_next ^ _old)->getCount() == 0) return true;
        LVStreamRef data = LVCreateMemoryStream();
        if (!_next->saveToStream(data.get()) || !LVWriteFileAtomic(_filename, data, false)) return false;
        _old = LVClonePropsContainer(_next);
        return true;
    }
};

#endif
