#include "../cr3gui/src/pb_dictionary_engine.h"
#include "../cr3gui/src/pbdictionary.h"
#include "testutil.h"
lString32 pbSkinFileName,openedCacheFile,currentCacheDir,crCurrentLang;
int main() {
    char **names=pbEnumDictionaries();CHECK(names&&names[0]);
    CHECK(pbOpenDictionary("Fixture English-Russian")&&!pbUsingStarDict());
    char *word=NULL,*translation=NULL;
    for(int round=0;round<100;++round) {
        CHECK(pbLookupWordExact(const_cast<char*>("house"),&word,&translation)==2);
        PbDictionary::ArticleSnapshot first(word,translation);
        CHECK(pbLookupNext(&word,&translation)==1&&!strcmp(word,"dog"));
        CHECK(strlen(translation)==12000&&translation[0]=='b');
        PbDictionary::ArticleSnapshot second(word,translation);
        CHECK(pbLookupNext(&word,&translation)==1&&!strcmp(word,"word"));
        CHECK(!strcmp(second.word(),"dog")&&second.translation()[0]=='b');
        CHECK(pbLookupNext(&word,&translation)==0&&!word&&!translation);
        CHECK(pbLookupPrevious(&word,&translation)==1&&!strcmp(word,"dog"));
        CHECK(pbLookupPrevious(&word,&translation)==1&&!strcmp(word,"house"));
        CHECK(pbLookupPrevious(&word,&translation)==0&&!word&&!translation);
        CHECK(!strcmp(first.word(),"house")&&!strcmp(first.translation(),"Exact translation"));
        CHECK(pbLookupWordExact(const_cast<char*>("missing"),&word,&translation)==0&&!word&&!translation);
        CHECK(pbLookupWord(const_cast<char*>("missing"),&word,&translation)==1&&!strcmp(word,"house"));
    }
    pbCloseDictionary();CHECK(pbLookupNext(&word,&translation)==0&&!word&&!translation);
    puts("PASS: firmware navigation ABI, bounded output buffers, changing words, next/previous, missing results and deferred article ownership");
}
