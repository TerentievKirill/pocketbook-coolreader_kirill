#include "pb_file_associations.h"
#include "lvstreamutils.h"
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <map>
#include <string>
#include <sys/file.h>
#include <unistd.h>

namespace pbassoc {
namespace {

constexpr char reader[] = "cr3-pb.app";

class ConfigDirectoryLock {
    int _fd = -1;
public:
    explicit ConfigDirectoryLock(const lString32 &filename)
    {
        lString32 directory = LVExtractPath(filename);
        if (directory.empty())
            directory = lString32(".");
        if (!LVDirectoryExists(directory) && !LVCreateDirectory(directory) && !LVDirectoryExists(directory))
            return;
        _fd = open(UnicodeToUtf8(directory).c_str(), O_RDONLY | O_DIRECTORY);
    }
    ~ConfigDirectoryLock() { if (_fd >= 0) close(_fd); }
    bool lock() const { return _fd >= 0 && flock(_fd, LOCK_EX) == 0; }
    ConfigDirectoryLock(const ConfigDirectoryLock &) = delete;
    ConfigDirectoryLock &operator=(const ConfigDirectoryLock &) = delete;
};

struct Format {
    Group group;
    const char *extension;
    const char *label;
    const char *icon;
};

// Use firmware metadata where available. These values are for formats that
// have no entry in either configuration.
constexpr Format formats[] = {
    {Group::FictionBook, "fb2", "@FB2_file", "ICON_FB2"},
    {Group::FictionBook, "fb3", "@FB3_file", "ICON_FB2"},
    {Group::FictionBook, "fb2.zip", "@FB2_file", "ICON_FB2"},
    {Group::FictionBook, "fbz", "@FB2_file", "ICON_FB2"},
    {Group::Epub, "epub", "@EPUB_file", "ICON_EPUB"},
    {Group::Documents, "doc", "@DOC_file", "ICON_DOC"},
    {Group::Documents, "docx", "@DOCX_file", "ICON_DOCX"},
    {Group::Documents, "odt", "@ODT_file", "ICON_DOC"},
    {Group::Documents, "rtf", "@RTF_file", "ICON_RTF"},
    {Group::Web, "htm", "@HTML_file", "ICON_HTM"},
    {Group::Web, "html", "@HTML_file", "ICON_HTML"},
    {Group::Web, "xhtml", "@HTML_file", "ICON_HTML"},
    {Group::Web, "chm", "@Z_HTML_file", "ICON_CHM"},
    {Group::Palm, "mobi", "@PRC_file", "ICON_PRC"},
    {Group::Palm, "prc", "@PRC_file", "ICON_PRC"},
    {Group::Palm, "pdb", "@PRC_file", "ICON_PRC"},
    {Group::Text, "txt", "@Text_file", "ICON_TXT"},
    {Group::Text, "tcr", "@TCR_file", "ICON_TCR"},
    {Group::Text, "pml", "@Text_file", "ICON_TXT"},
};

bool readConfig(const lString32 &path, std::string &contents)
{
    FILE *file = std::fopen(UnicodeToUtf8(path).c_str(), "rb");
    if (!file)
        return errno == ENOENT;
    char buffer[4096];
    size_t length;
    while ((length = std::fread(buffer, 1, sizeof(buffer), file)) > 0)
        contents.append(buffer, length);
    const bool ok = !std::ferror(file);
    std::fclose(file);
    return ok && contents.find('\0') == std::string::npos;
}

std::string normalized(std::string field)
{
    const auto start = field.find_first_not_of(" \t\r");
    if (start == std::string::npos)
        return {};
    field = field.substr(start, field.find_last_not_of(" \t\r") - start + 1);
    for (char &ch : field)
        if (ch >= 'A' && ch <= 'Z')
            ch += 'a' - 'A';
    return field;
}

struct Association {
    std::string extension;
    size_t appsStart;
    size_t appsEnd;
};

bool parse(const std::string &line, Association &association)
{
    const size_t start = line.compare(0, 3, "\xef\xbb\xbf") == 0 ? 3 : 0;
    const size_t typeEnd = line.find(':', start);
    if (typeEnd == std::string::npos)
        return false;
    association.extension = normalized(line.substr(start, typeEnd - start));
    if (association.extension.empty() || association.extension[0] == '#' ||
        association.extension[0] == ';')
        return false;
    const size_t labelEnd = line.find(':', typeEnd + 1);
    if (labelEnd == std::string::npos)
        return false;
    const size_t indexEnd = line.find(':', labelEnd + 1);
    if (indexEnd == std::string::npos)
        return false;
    association.appsStart = indexEnd + 1;
    association.appsEnd = line.find(':', association.appsStart);
    return association.appsEnd != std::string::npos;
}

void promote(std::string &line, const Association &association)
{
    std::string apps = reader;
    bool alreadyFirst = false;
    int count = 0;
    size_t start = association.appsStart;
    while (start < association.appsEnd) {
        size_t end = line.find(',', start);
        if (end == std::string::npos || end > association.appsEnd)
            end = association.appsEnd;
        const std::string entry = line.substr(start, end - start);
        const std::string name = normalized(entry);
        if (name == reader) {
            alreadyFirst = alreadyFirst || (start == association.appsStart && entry == reader);
            ++count;
        } else if (!name.empty()) {
            apps += ',' + entry;
        }
        start = end + 1;
    }
    if (!alreadyFirst || count != 1)
        line.replace(association.appsStart, association.appsEnd - association.appsStart, apps);
}

// Supply a line without its terminator, retaining the terminator exactly in the caller.
size_t lineEnd(const std::string &text, size_t start)
{
    const size_t end = text.find('\n', start);
    return end == std::string::npos ? text.size() : end;
}

}

Result makeDefault(Group group, const lString32 &userConfig, const lString32 &systemConfig)
{
    // Lock the directory so a rename of the config cannot invalidate the lock.
    ConfigDirectoryLock lock(userConfig);
    if (!lock.lock())
        return Result::Error;

    std::map<std::string, const Format *> selected;
    for (const auto &format : formats)
        if (group == Group::All || group == format.group)
            selected.emplace(format.extension, &format);

    std::string current;
    if (!readConfig(userConfig, current))
        return Result::Error;

    auto missing = selected;
    std::string updated;
    for (size_t start = 0; start < current.size();) {
        const size_t end = lineEnd(current, start);
        std::string line = current.substr(start, end - start);
        Association association;
        if (parse(line, association) && selected.count(association.extension)) {
            promote(line, association);
            missing.erase(association.extension);
        }
        updated += line;
        if (end < current.size())
            updated += '\n';
        start = end + 1;
    }

    if (!missing.empty()) {
        // Copy only the needed overrides. Other formats inherit their firmware rules.
        std::string stock;
        if (!readConfig(systemConfig, stock))
            return Result::Error;
        std::map<std::string, std::string> stockRules;
        for (size_t start = 0; start < stock.size();) {
            const size_t end = lineEnd(stock, start);
            std::string line = stock.substr(start, end - start);
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (start == 0 && line.compare(0, 3, "\xef\xbb\xbf") == 0)
                line.erase(0, 3);
            Association association;
            if (parse(line, association) && missing.count(association.extension)) {
                promote(line, association);
                stockRules.emplace(association.extension, line);
            }
            start = end + 1;
        }
        const size_t newline = current.find('\n');
        const char *eol = newline > 0 && newline < current.size() && current[newline - 1] == '\r'
            ? "\r\n" : "\n";
        if (!updated.empty() && updated != "\xef\xbb\xbf" && updated.back() != '\n')
            updated += eol;
        for (const auto &entry : missing) {
            const auto rule = stockRules.find(entry.first);
            if (rule != stockRules.end()) {
                updated += rule->second;
            } else {
                const Format &format = *entry.second;
                updated += entry.first + ':' + format.label + ":1:" + reader + ':' + format.icon;
            }
            updated += eol;
        }
    }

    if (updated == current)
        return Result::AlreadyDefault;
    LVStreamRef data = LVCreateStringStream(lString8(updated.c_str(), updated.size()));
    return LVWriteFileAtomic(userConfig, data, true) ? Result::Updated : Result::Error;
}

}
