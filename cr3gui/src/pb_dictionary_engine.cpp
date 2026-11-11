#include <inkview.h>
#include "pb_dictionary_engine.h"
#include "stardict.h"
#include <map>
#include <set>
#include <vector>
#include <dlfcn.h>
#ifndef CR3_NATIVE_DICTIONARY_LIBRARY
#define CR3_NATIVE_DICTIONARY_LIBRARY "libdictionary.so"
#endif
namespace {
// Firmware 6 exposes the current C interface in libdictionary. Keep the old
// InkView entry points as a fallback for firmware without that interface.
struct NativeAPI {
    char **(*list)() = NULL;
    int (*open)(const char *) = NULL;
    void (*close)() = NULL;
    int (*exact)(const char *,char **,char **) = NULL;
    int (*lookup)(const char *,char **,char **) = NULL;
    void *(*object)() = NULL;
    // These are buffer-based APIs, unlike the legacy InkView char** wrappers.
    int (*next)(void *,char *,size_t,char *,size_t) = NULL;
    int (*previous)(void *,char *,size_t,char *,size_t) = NULL;
    NativeAPI() {
        void *lib=dlopen(CR3_NATIVE_DICTIONARY_LIBRARY,RTLD_LAZY|RTLD_LOCAL);if(!lib)return;
        list=reinterpret_cast<decltype(list)>(dlsym(lib,"pb_dictionary_list_all"));
        open=reinterpret_cast<decltype(open)>(dlsym(lib,"pb_dictionary_open"));
        close=reinterpret_cast<decltype(close)>(dlsym(lib,"pb_dictionary_close"));
        exact=reinterpret_cast<decltype(exact)>(dlsym(lib,"pb_dictionary_lookup_word_exact"));
        lookup=reinterpret_cast<decltype(lookup)>(dlsym(lib,"pb_dictionary_lookup_word"));
        object=reinterpret_cast<decltype(object)>(dlsym(lib,"pb_dictionary_object"));
        next=reinterpret_cast<decltype(next)>(dlsym(lib,"pb_dictionary_lookup_next"));
        previous=reinterpret_cast<decltype(previous)>(dlsym(lib,"pb_dictionary_lookup_previous"));
        if(!list||!open||!close||!exact||!lookup||!object||!next||!previous)list=NULL;
        // The process may keep dictionary result pointers; retain the module.
    }
};
NativeAPI &native(){static NativeAPI api;return api;}
std::vector<std::string> names;
std::vector<char *> pointers;
std::map<std::string,std::string> paths;
std::unique_ptr<stardict::Dictionary> dictionary;
std::string resultWord,resultText;
bool nativeOpen=false;
int result(bool found,char **word,char **translation,int status) {
    *word=found?const_cast<char*>(resultWord.c_str()):NULL;
    *translation=found?const_cast<char*>(resultText.c_str()):NULL;
    return found?status:0;
}
int nativeStatus(int status,char **word,char **translation) {
    if((status==1||status==2)&&*word&&**word)return status;
    *word=NULL;*translation=NULL;return 0;
}
int nativeMove(bool forward,char **word,char **translation) {
    auto &api=native();
    if(!api.list)return nativeStatus(forward?LookupNext(word,translation):LookupPrevious(word,translation),word,translation);
    void *object=api.object();if(!object){*word=NULL;*translation=NULL;return 0;}
    // SDIC articles are bounded by the format's 16-bit record size. Allow room
    // for the firmware's HTML expansion without truncating candidate articles.
    static std::vector<char> wordBuffer(4096),textBuffer(1024*1024);
    wordBuffer.front()=textBuffer.front()='\0';
    int status=(forward?api.next:api.previous)(object,wordBuffer.data(),wordBuffer.size(),textBuffer.data(),textBuffer.size());
    wordBuffer.back()=textBuffer.back()='\0';
    *word=wordBuffer.data();*translation=textBuffer.data();
    return nativeStatus(status,word,translation);
}
}
char **pbEnumDictionaries() {
    names.clear();paths.clear();pointers.clear();std::set<std::string> used;
    if(char **list=native().list?native().list():EnumDictionaries())for(unsigned i=0;list[i];++i){names.emplace_back(list[i]);used.insert(list[i]);}
    for(const char *root:{FLASHDIR "/system/dictionaries",FLASHDIR "/applications/koreader/data/dict"})
        for(auto &item:stardict::discover(root)) {
            std::string label=item.name+" (StarDict)",base=label;unsigned n=2;
            while(used.count(label))label=base+" "+std::to_string(n++);
            used.insert(label);names.push_back(label);paths[label]=item.path;
        }
    for(auto &name:names)pointers.push_back(const_cast<char*>(name.c_str()));pointers.push_back(NULL);
    return pointers.data();
}
void pbCloseDictionary() {
    dictionary.reset();resultWord.clear();resultText.clear();
    if(nativeOpen){if(native().list)native().close();else CloseDictionary();nativeOpen=false;}
}
bool pbOpenDictionary(const char *name) {
    // Copy before closing: callers may pass a string owned by a current result.
    std::string label=name?name:"";pbCloseDictionary();
    auto found=paths.find(label);
    if(found==paths.end())return nativeOpen=native().list?native().open(label.c_str())==1:OpenDictionary(label.c_str());
    std::unique_ptr<stardict::Dictionary> next(new stardict::Dictionary());
    if(!next->open(found->second))return false;
    dictionary=std::move(next);return true;
}
bool pbUsingStarDict(){return bool(dictionary);}
int pbLookupWordExact(char *query,char **word,char **translation) {
    if(!dictionary)return nativeStatus(native().list?native().exact(query,word,translation):LookupWordExact(query,word,translation),word,translation);
    return result(dictionary->lookup(query?query:"",true,resultWord,resultText),word,translation,2);
}
int pbLookupWord(char *query,char **word,char **translation) {
    if(!dictionary)return nativeStatus(native().list?native().lookup(query,word,translation):LookupWord(query,word,translation),word,translation);
    return result(dictionary->lookup(query?query:"",false,resultWord,resultText),word,translation,1);
}
int pbLookupNext(char **word,char **translation) {
    return dictionary?result(dictionary->move(1,resultWord,resultText),word,translation,1):nativeMove(true,word,translation);
}
int pbLookupPrevious(char **word,char **translation) {
    return dictionary?result(dictionary->move(-1,resultWord,resultText),word,translation,1):nativeMove(false,word,translation);
}
