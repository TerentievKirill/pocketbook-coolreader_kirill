#include "crengine.h"
#include "hyphman.h"
#include "testutil.h"
#include <cstdlib>
#include <unistd.h>

lString32 pbSkinFileName, openedCacheFile, currentCacheDir;

static bool writePattern(const lString32 &file, const char *title, const char *language)
{
    lString8 xml = lString8("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n") +
        "<HyphenationDescription title=\"" + title + "\" lang=\"" + language +
        "\" lefthyphenmin=\"2\" righthyphenmin=\"2\"><pattern>ab1c</pattern></HyphenationDescription>";
    return LVWriteFileAtomic(file, LVCreateStringStream(xml), false);
}

static bool hasDictionary(HyphDictionaryList &list, const lString32 &id, const lChar32 *title,
                          const lChar32 *lang)
{
    const HyphDictionary *entry = list.find(id);
    return entry && entry->getTitle() == title && entry->getLangTag() == lang;
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    lString8 directory = lString8(argv[1]) + "/hyph-metadata-XXXXXX";
    char *unique = mkdtemp(directory.modify());
    CHECK(unique);
    lString32 root = Utf8ToUnicode(unique) + "/";
    lString32 patterns = root + "patterns/";
    CHECK(LVCreateDirectory(patterns));
    lString32 id("sample.pattern"), pattern = patterns + id, cache = root + "metadata.bin";
    CHECK(writePattern(pattern, "English", "en"));

    HyphDictionaryList list;
    CHECK(list.open(patterns, true, cache));
    CHECK(hasDictionary(list, id, U"English", U"en"));
    const lString8 stamp = LVFileMetadataFingerprint(UnicodeToUtf8(cache));
    CHECK(!stamp.empty());
    CHECK(list.open(patterns, true, cache));
    CHECK(hasDictionary(list, id, U"English", U"en"));
    CHECK(LVFileMetadataFingerprint(UnicodeToUtf8(cache)) == stamp);

    // A replacement of the same byte length must invalidate the cached header.
    CHECK(writePattern(pattern, "Russian", "ru"));
    CHECK(list.open(patterns, true, cache));
    CHECK(hasDictionary(list, id, U"Russian", U"ru"));
    CHECK(LVFileMetadataFingerprint(UnicodeToUtf8(cache)) != stamp);

    CHECK(LVWriteFileAtomic(cache, LVCreateStringStream(lString8("corrupt cache")), false));
    CHECK(list.open(patterns, true, cache));
    CHECK(hasDictionary(list, id, U"Russian", U"ru"));
    LVStreamRef rebuilt = LVOpenFileStream(cache.c_str(), LVOM_READ);
    CHECK(!rebuilt.isNull() && rebuilt->GetSize() > 13);
    rebuilt.Clear();

    CHECK(writePattern(pattern, "Missing language", ""));
    CHECK(list.open(patterns, true, cache));
    CHECK(!list.find(id));
    CHECK(list.length() == 3);
    const lString8 rejectedStamp = LVFileMetadataFingerprint(UnicodeToUtf8(cache));
    CHECK(list.open(patterns, true, cache));
    CHECK(!list.find(id));
    CHECK(LVFileMetadataFingerprint(UnicodeToUtf8(cache)) == rejectedStamp);

    CHECK(writePattern(pattern, "Replacement", "en"));
    CHECK(list.open(patterns, true, cache));
    CHECK(hasDictionary(list, id, U"Replacement", U"en"));
    CHECK(LVDeleteFile(pattern));
    CHECK(list.open(patterns, true, cache));
    CHECK(!list.find(id));
    CHECK(LVDeleteFile(cache));
    CHECK(rmdir(UnicodeToUtf8(patterns).c_str()) == 0);
    CHECK(rmdir(unique) == 0);
    puts("Hyphenation metadata cache survives corruption, replacement and removal");
    return 0;
}
