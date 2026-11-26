#include "cri18n.h"
#include "testutil.h"
#include <string>

class TestTranslator : public CRMoFileTranslator {
public:
    using CRMoFileTranslator::add;
    using CRMoFileTranslator::sort;
    using CRMoFileTranslator::getText;
    int checkEntries() {
        CHECK(_list.length() > 0);
        for (int i = 0; i < _list.length(); ++i)
            CHECK(!strcmp(getText(_list[i]->src.c_str()), _list[i]->dst.c_str()));
        // A valid, missing key beyond the catalog's final entry.
        lString8 missing = _list[_list.length() - 1]->src;
        missing += "~missing";
        CHECK(getText(missing.c_str()) == missing.c_str());
        return 0;
    }
};

int main(int argc, char **argv) {
    CHECK(argc == 2);
    // Exercise every hit and gap at multiple sizes, including empty/singleton
    // catalogs and both odd/even search ranges.
    for (int count = 0; count <= 40; ++count) {
        TestTranslator translator;
        for (int i = count - 1; i >= 0; --i) {
            char key[20];
            sprintf(key, "key-%03d", i * 2);
            translator.add(lString8(key), lString8("translated"));
        }
        translator.sort();
        CHECK(!strcmp(translator.getText(NULL), ""));
        CHECK(!strcmp(translator.getText(""), ""));
        for (int i = 0; i <= count * 2 + 1; ++i) {
            char key[20];
            sprintf(key, "key-%03d", i);
            const char *result = translator.getText(key);
            if (i % 2 == 0 && i / 2 < count)
                CHECK(!strcmp(result, "translated"));
            else
                CHECK(result == key);
        }
        CHECK(!strcmp(translator.getText("zzzz"), "zzzz"));
    }
    TestTranslator russian;
    CHECK(russian.openMoFile(Utf8ToUnicode(lString8(argv[1]))));
    CHECK(russian.checkEntries() == 0);
    CHECK(!strcmp(russian.getText("Check for updates"), "Проверить обновления"));
    CHECK(!strcmp(russian.getText("English dictionaries"), "Словари английского языка"));
    CHECK(!strcmp(russian.getText("Restart now"), "Перезапустить"));
    CHECK(!strcmp(russian.getText("[Algorithmic Hyphenation]"), "[Алгоритмические переносы]"));
    CHECK(strstr(russian.getText("Downloading $1: $2 KB"), "$1"));
    CHECK(strstr(russian.getText("Downloading $1: $2 KB"), "$2"));
    puts("PASS: catalog hits and missing keys, including the Russian catalog upper boundary");
    return 0;
}
