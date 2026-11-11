#ifndef PB_DICTIONARY_H_INCLUDED
#define PB_DICTIONARY_H_INCLUDED
#include "lvstring.h"
#include "lvutf8.h"
#include "lvxmlutils.h"
#include <regex.h>
#include <set>
#include <string>

namespace PbDictionary {
// Suggestion rows are plain text, regardless of the dictionary's storage format.
inline lString32 preview(const char *translation) {
    const size_t limit = 32768;
    std::string input(translation ? translation : "", translation ? strnlen(translation, limit) : 0);
    std::string text;
    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
        if (c == 1 || c == 2 || c == 3) continue; // legacy PocketBook style markers
        if (c == '<' && i + 1 < input.size()) {
            size_t start = i + 1;
            if (input[start] == '/') ++start;
            bool tag = start < input.size() && ((input[start] >= 'a' && input[start] <= 'z') ||
                (input[start] >= 'A' && input[start] <= 'Z') || input[start] == '!' || input[start] == '?');
            if (tag) {
                char quote = 0; size_t end = start;
                for (; end < input.size(); ++end) {
                    char next = input[end];
                    if (quote) { if (next == quote) quote = 0; }
                    else if (next == '\'' || next == '"') quote = next;
                    else if (next == '>') break;
                }
                if (end < input.size()) { text += ' '; i = end; continue; }
            }
        }
        text += c;
    }
    lString32 value = Utf8ToUnicode(text.c_str());
    PreProcessXmlString(value, 0);
    value.trim();
    return value.substr(0, 256);
}
// Deferred UI updates outlive native lookup buffers and suggestion menu rows.
class ArticleSnapshot {
    lString8 _word, _translation;
    bool _present = false;
public:
    ArticleSnapshot() {}
    ArticleSnapshot(const char *word, const char *translation) { set(word, translation); }
    void set(const char *word, const char *translation) {
        lString8 nextWord(word ? word : ""), nextTranslation(translation ? translation : "");
        _word = nextWord; _translation = nextTranslation; _present = word != NULL;
    }
    void clear() { _present = false; _word.clear(); _translation.clear(); }
    bool present() const { return _present; }
    const char *word() const { return _word.c_str(); }
    const char *translation() const { return _translation.c_str(); }
};
class RedirectPatterns {
    regex_t patterns[2];
    bool valid[2];
public:
    RedirectPatterns() {
        const char *rules[] = {
            "^[\n\t ]+\\([^\\)]+\\)[\t ]+(to|of|see|from)[\t ]+([a-z]+)[\\. \t\n]+$",
            "^[\n\t ]+\\([^\\)]+\\)[\t ]+([a-z\\.]+[ ])?(to|of|see|from)[\t ]+([a-z]+)[\\. \n\t]+$"
        };
        for (int i = 0; i < 2; ++i) valid[i] = regcomp(&patterns[i], rules[i], REG_ICASE | REG_EXTENDED) == 0;
    }
    ~RedirectPatterns() {
        for (int i = 0; i < 2; ++i) if (valid[i]) regfree(&patterns[i]);
    }
    lString32 find(const char *translation) const {
        if (!translation) return lString32();
        size_t length = strlen(translation);
        for (int i = 0; i < 2; ++i) {
            regmatch_t matches[5];
            if (!valid[i] || regexec(&patterns[i], translation, 5, matches, 0)) continue;
            int capture = i == 0 ? 2 : 3;
            regoff_t first = matches[capture].rm_so, last = matches[capture].rm_eo;
            if (first >= 0 && last > first && static_cast<size_t>(last) <= length)
                return Utf8ToUnicode(lString8(translation + first, last - first));
        }
        return lString32();
    }
private:
    RedirectPatterns(const RedirectPatterns &);
    RedirectPatterns &operator=(const RedirectPatterns &);
};
inline lString32 redirectFor(const char *translation) {
    static RedirectPatterns patterns;
    return patterns.find(translation);
}

class RedirectChain {
    std::set<std::string> visited;
    static std::string key(lString32 word) {
        word.lowercase();
        return UnicodeToUtf8(word).c_str();
    }
public:
    bool canFollow(const lString32 &word) const {
        return !word.empty() && visited.size() < 16 && !visited.count(key(word));
    }
    void add(const lString32 &word) { visited.insert(key(word)); }
};
}
#endif
