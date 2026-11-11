#ifndef CR3_STARDICT_INSTALL_H
#define CR3_STARDICT_INSTALL_H
#include <memory>
#include <string>
class StarDictInstallation {
    class Impl;
    std::unique_ptr<Impl> impl;
public:
    StarDictInstallation(const std::string &archive,const std::string &directory,const std::string &name);
    ~StarDictInstallation();
    bool poll();
    bool succeeded() const;
    const std::string &error() const;
};
#endif
