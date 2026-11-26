#include "crengine.h"
#include "lvstreamfragment.h"
#include "lvutf8.h"
#include "testutil.h"
#include <cstring>
#include <limits>
lString32 pbSkinFileName, openedCacheFile, currentCacheDir;

static bool entryEquals(const LVContainerRef &archive, const lChar32 *name, const char *expected)
{
    LVStreamRef stream = archive->OpenStream(name, LVOM_READ);
    char bytes[16];
    lvsize_t count = 0;
    return !stream.isNull() && stream->Read(bytes, sizeof(bytes), &count) == LVERR_OK &&
           count == strlen(expected) && !memcmp(bytes, expected, count);
}

int main(int argc, char **argv) {
    CHECK(argc == 2);
    LVStreamFragment fragment(LVCreateStringStream(lString8("ABCDE")),1,2);
    CHECK(fragment.GetMode() == LVOM_READ);
    char bytes[128] = {}; lvsize_t count = 99;
    CHECK(fragment.Read(bytes,4,&count) == LVERR_OK && count == 2 && !memcmp(bytes,"BC",2));
    CHECK(fragment.Read(bytes,4,&count) == LVERR_OK && count == 0);
    CHECK(fragment.Seek(-1,LVSEEK_END,NULL) == LVERR_OK && fragment.GetPos() == 1);
    CHECK(fragment.Read(bytes,1,&count) == LVERR_OK && bytes[0] == 'C');
    CHECK(fragment.Seek(-3,LVSEEK_END,NULL) != LVERR_OK);
    CHECK(fragment.Seek(1,LVSEEK_END,NULL) != LVERR_OK);
    CHECK(fragment.Seek(std::numeric_limits<lvoffset_t>::min(),LVSEEK_CUR,NULL) != LVERR_OK);
    CHECK(fragment.Seek(0,LVSEEK_SET,NULL) == LVERR_OK && fragment.GetPos() == 0);
    LVStreamFragment empty(LVCreateStringStream(lString8("hidden")),0,0);
    CHECK(empty.Read(bytes,1,&count) == LVERR_OK && count == 0);
    const char *archives[] = {"utf8-stored.zip", "utf8-deflated.zip", "descriptor-stored.zip", "descriptor-deflated.zip", "legacy.zip", "ascii.zip"};
    for (int i = 0; i < 6; ++i) {
        lString32 file = Utf8ToUnicode(argv[1]) + lString32("/") + lString32(archives[i]);
        LVContainerRef arc = LVOpenArchieve(LVOpenFileStream(file.c_str(),LVOM_READ)); CHECK(!arc.isNull());
        lString32 name = i < 2 ? Utf8ToUnicode("Тест/章节-é.xhtml") : i == 4 ? Utf8ToUnicode("Тест.xhtml") : lString32("chapter.xhtml");
        LVStreamRef entry = arc->OpenStream(name.c_str(),LVOM_READ); CHECK(!entry.isNull());
        memset(bytes,0,sizeof(bytes)); count=0;
        CHECK(entry->Read(bytes,sizeof(bytes)-1,&count) == LVERR_OK);
        CHECK(count == 7 && !memcmp(bytes,"payload",7));
    }
    lString32 invalid = Utf8ToUnicode(argv[1]) + lString32("/invalid-utf8.zip");
    CHECK(LVOpenArchieve(LVOpenFileStream(invalid.c_str(),LVOM_READ)).isNull());
    const char *indexedArchives[] = {"indexed.zip", "indexed-local-headers.zip"};
    for (int i = 0; i < 2; ++i) {
        lString32 file = Utf8ToUnicode(argv[1]) + "/" + indexedArchives[i];
        LVContainerRef archive = LVOpenArchieve(LVOpenFileStream(file.c_str(), LVOM_READ));
        CHECK(!archive.isNull() && archive->GetObjectCount() == 154 - i);
        CHECK(entryEquals(archive, U"Тест/章节-é.xhtml", "first"));
        // Scan enough entries to make the following lookups use the index.
        for (int lookup = 0; lookup < 8; ++lookup)
            CHECK(archive->OpenStream(U"missing.xhtml", LVOM_READ).isNull());
        CHECK(entryEquals(archive, U"/Тест/章节-é.xhtml", "first"));
        CHECK(entryEquals(archive, U"Тест/章节-É.xhtml", "case"));
        CHECK(archive->OpenStream(U"//Тест/章节-é.xhtml", LVOM_READ).isNull());
        CHECK(!lStr_cmp(archive->GetObjectInfo(152)->GetName(), U"Тест/章节-é.xhtml"));
    }
    puts("PASS: fragment bounds, ZIP encoding, descriptors and first duplicate before and after indexing");
}
