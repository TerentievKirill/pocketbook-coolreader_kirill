#ifndef CR3_PB_BROWSER_H
#define CR3_PB_BROWSER_H
#include <string>
namespace pbweb {
inline std::string encode(const std::string &text) {
    const char *hex="0123456789ABCDEF";
    std::string out;
    for (unsigned char c:text) {
        if ((c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='-' || c=='_' || c=='.' || c=='~') out+=c;
        else { out+='%'; out+=hex[c>>4]; out+=hex[c&15]; }
    }
    return out;
}
inline std::string languageCode(std::string text) {
    size_t end=text.find_first_of("-_");
    if (end!=std::string::npos) text.resize(end);
    if (text.size()!=2 && text.size()!=3) return "";
    for (char &c:text) {
        if (c>='A'&&c<='Z') c+= 'a'-'A';
        if (c<'a'||c>'z') return "";
    }
    if (text=="eng") return "en";
    if (text=="rus") return "ru";
    if (text=="ukr") return "uk";
    if (text=="ces"||text=="cze") return "cs";
    return text;
}
inline std::string searchURL(std::string base,const std::string &bookLanguage,const std::string &uiLanguage,const std::string &word) {
    std::string lang=languageCode(bookLanguage);
    if (lang.empty()) lang=languageCode(uiLanguage);
    if (lang.empty()) lang="en";
    size_t marker=base.find("[LANG]");
    if (marker!=std::string::npos) base.replace(marker,6,lang);
    return base+encode(word);
}
}
#endif
