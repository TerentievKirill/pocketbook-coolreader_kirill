#include "../cr3gui/src/stardict.h"
#include "../cr3gui/src/stardict_install.h"
#include "crengine.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#undef CHECK
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);exit(1);}}while(0)
lString32 pbSkinFileName, openedCacheFile, currentCacheDir, crCurrentLang;
static std::string file(const std::string &p){std::ifstream f(p,std::ios::binary);return std::string((std::istreambuf_iterator<char>(f)),{});}
int main(int argc,char **argv) {
    CHECK(argc==2);std::string root=argv[1],word,body;
    CHECK(stardict::preview("<b title='a > b'>house</b> &amp; cat").find("house & cat")!=std::string::npos);
    for(const char *name:{"plain","compressed","wide"}) {
        stardict::Dictionary d;CHECK(d.open(root+"/stardict-fixtures/"+name+"/test.ifo"));
        CHECK(d.lookup("HOUSE",true,word,body)&&word=="house"&&body.find("a building")!=std::string::npos);
        CHECK(d.lookup("houses",true,word,body)&&word=="house");
        CHECK(d.lookup("ДОМА",true,word,body)&&word=="дом"&&body.find("жилище")!=std::string::npos);
        CHECK(d.lookup("kočky",true,word,body)&&word=="kočka");
        CHECK(d.lookup("слова",true,word,body)&&word=="слово");
        CHECK(d.lookup("duplicate",true,word,body)&&body.find("first")!=std::string::npos&&body.find("second")!=std::string::npos);
        CHECK(!d.lookup("not-in-this-dictionary",true,word,body));
        CHECK(d.lookup("house",true,word,body));CHECK(d.move(1,word,body));CHECK(d.move(-1,word,body)&&word=="house");
        CHECK(d.lookup("markup",true,word,body)&&body.find("<tag>")!=std::string::npos);
        for(int i=0;i<1000;++i)CHECK(d.lookup(i%2?"houses":"ДОМА",true,word,body));
    }
    for(const char *name:{"bad-size","bad-offset","bad-synonym","truncated-index","bad-order","bad-version","truncated-dictzip"}) {
        stardict::Dictionary d;CHECK(!d.open(root+"/stardict-fixtures/"+name+"/test.ifo"));
    }
    stardict::Dictionary fields;CHECK(fields.open(root+"/stardict-fixtures/fields/test.ifo"));
    CHECK(fields.lookup("word",true,word,body)&&body.find("phonetic")!=std::string::npos&&body.find("&lt;meaning&gt;")!=std::string::npos);
    stardict::Dictionary typed;CHECK(typed.open(root+"/stardict-fixtures/typed/test.ifo"));
    CHECK(typed.lookup("word",true,word,body)&&body.find("<b>rich</b>")!=std::string::npos&&body.find("plain")!=std::string::npos);
    CHECK(stardict::discover(root+"/stardict-fixtures/plain").size()==1);
    std::string directory=root+"/stardict-install";mkdir(directory.c_str(),0755);
    for(const char *name:{"good","traversal","duplicate","incomplete"}) {
        std::string target=std::string(name)+".stardict";
        StarDictInstallation install(root+"/stardict-"+name+".zip",directory,target);
        for(unsigned i=0;!install.poll();++i)CHECK(i<100);
        CHECK(install.succeeded()==(std::string(name)=="good"));
        struct stat st;if(std::string(name)!="good")CHECK(lstat((directory+"/"+target).c_str(),&st));
    }
    CHECK(access((root+"/escaped.ifo").c_str(),F_OK));
    {StarDictInstallation repeated(root+"/stardict-good.zip",directory,"good.stardict");while(!repeated.poll()){}CHECK(!repeated.succeeded());}
    {StarDictInstallation cancel(root+"/stardict-cancel.zip",directory,"cancel.stardict");bool done=cancel.poll();if(done)fprintf(stderr,"Cancellation setup: %s\n",cancel.error().c_str());CHECK(!done);}
    CHECK(access((directory+"/cancel.stardict").c_str(),F_OK));
    CHECK(access((directory+"/.coolreader-stardict-unpack").c_str(),F_OK));
    CHECK(file(directory+"/good.stardict/test.ifo").find("StarDict")!=std::string::npos);
    puts("PASS: StarDict plain/dictzip, compressed/64-bit indexes, Unicode, alternate forms, multiple definitions, typed fields, browsing and safe atomic archive installation");
}
