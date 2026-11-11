#ifndef CR3_STARDICT_H
#define CR3_STARDICT_H
#include <memory>
#include <string>
#include <vector>
namespace stardict {
struct Info { std::string name, path; };
std::vector<Info> discover(const std::string &directory);
std::string escape(const std::string &text);
std::string preview(const std::string &html);
class Dictionary {
    class Impl;
    std::unique_ptr<Impl> impl;
public:
    Dictionary();
    ~Dictionary();
    bool open(const std::string &ifo);
    bool lookup(const std::string &query, bool exact, std::string &word, std::string &html);
    bool move(int direction, std::string &word, std::string &html);
    const std::string &error() const;
};
}
#endif
