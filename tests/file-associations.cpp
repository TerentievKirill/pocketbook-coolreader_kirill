#include "../cr3gui/src/pb_file_associations.h"
#include "testutil.h"
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

lString32 pbSkinFileName, openedCacheFile, currentCacheDir;

static bool write(const std::string &path, const std::string &contents)
{
    std::ofstream file(path, std::ios::binary);
    file << contents;
    file.close();
    return file.good();
}

static std::string read(const std::string &path)
{
    std::ifstream file(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

static bool contains(const std::string &text, const std::string &value)
{
    return text.find(value) != std::string::npos;
}

int main(int argc, char **argv)
{
    using namespace pbassoc;
    CHECK(argc == 2);
    const std::string directory = std::string(argv[1]) + "/file-associations-XXXXXX";
    std::vector<char> temp(directory.begin(), directory.end());
    temp.push_back('\0');
    CHECK(mkdtemp(temp.data()));
    const std::string custom = std::string(temp.data()) + "/extensions.cfg";
    const std::string firmware = std::string(temp.data()) + "/stock.cfg";
    const lString32 userPath = Utf8ToUnicode(custom.c_str());
    const lString32 stockPath = Utf8ToUnicode(firmware.c_str());
    const std::string stock = "epub:@EPUB_file:1:stock-epub.app:ICON_EPUB\n"
                              "fb2:@FB2_file:1:stock-fb.app:ICON_FB2\n"
                              "pdf:@PDF_file:1:stock-pdf.app:ICON_PDF\n"
                              "txt:@Text_file:1:stock-txt.app:ICON_TXT\n";
    const std::string original = "\xef\xbb\xbf# Custom setup\r\n"
        "EPUB:@My_EPUB:2:other.app, cr3-pb.app,third.app,CR3-PB.APP:ICON_CUSTOM:extra\r\n"
        "# unknown:1:2:3:4\r\n"
        "pdf:@Custom_PDF:1:my-pdf.app:ICON_PDF\r\n"
        "zip:@ZIP_file:1:zip-viewer.app:ICON_ZIP";
    CHECK(write(firmware, stock) && write(custom, original));
    CHECK(makeDefault(Group::Epub, userPath, stockPath) == Result::Updated);
    std::string expected = original;
    const std::string oldReaders = "other.app, cr3-pb.app,third.app,CR3-PB.APP";
    expected.replace(expected.find(oldReaders), oldReaders.size(), "cr3-pb.app,other.app,third.app");
    CHECK(read(custom) == expected && read(custom + ".bak") == original);
    CHECK(makeDefault(Group::Epub, userPath, stockPath) == Result::AlreadyDefault);
    CHECK(read(custom + ".bak") == original);

    CHECK(makeDefault(Group::FictionBook, userPath, stockPath) == Result::Updated);
    const std::string fictionBook = read(custom);
    CHECK(fictionBook.compare(0, expected.size(), expected) == 0);
    CHECK(fictionBook[expected.size()] == '\r');
    CHECK(contains(fictionBook, "fb2:@FB2_file:1:cr3-pb.app,stock-fb.app:ICON_FB2\r\n"));
    CHECK(contains(fictionBook, "fb3:@FB3_file:1:cr3-pb.app:ICON_FB2\r\n"));
    CHECK(contains(fictionBook, "fb2.zip:@FB2_file:1:cr3-pb.app:ICON_FB2\r\n"));
    CHECK(!contains(fictionBook, "txt:"));
    CHECK(read(custom + ".bak") == expected);

    CHECK(makeDefault(Group::All, userPath, stockPath) == Result::Updated);
    const std::string all = read(custom);
    CHECK(all.compare(0, fictionBook.size(), fictionBook) == 0);
    CHECK(contains(all, "docx:@DOCX_file:1:cr3-pb.app:ICON_DOCX\r\n"));
    CHECK(contains(all, "html:@HTML_file:1:cr3-pb.app:ICON_HTML\r\n"));
    CHECK(contains(all, "mobi:@PRC_file:1:cr3-pb.app:ICON_PRC\r\n"));
    CHECK(contains(all, "txt:@Text_file:1:cr3-pb.app,stock-txt.app:ICON_TXT\r\n"));
    CHECK(read(firmware) == stock && read(custom + ".bak") == fictionBook);
    CHECK(makeDefault(Group::All, userPath, stockPath) == Result::AlreadyDefault);

    const std::string empty = std::string(temp.data()) + "/new.cfg";
    const lString32 emptyPath = Utf8ToUnicode(empty.c_str());
    CHECK(makeDefault(Group::Epub, emptyPath, Utf8ToUnicode(temp.data())) == Result::Error);
    CHECK(read(empty).empty());
    CHECK(makeDefault(Group::Epub, emptyPath, stockPath) == Result::Updated);
    CHECK(read(empty) == "epub:@EPUB_file:1:cr3-pb.app,stock-epub.app:ICON_EPUB\n");
    CHECK(read(empty + ".bak").empty());
    CHECK(makeDefault(Group::All, Utf8ToUnicode(temp.data()), stockPath) == Result::Error);
    CHECK(read(custom) == all);

    const std::string batch = std::string(temp.data()) + "/batch.cfg";
    const lString32 batchPath = Utf8ToUnicode(batch.c_str());
    CHECK(write(batch, original));
    CHECK(makeDefault(Group::FictionBook | Group::Epub, batchPath, stockPath) == Result::Updated);
    CHECK(read(batch) == fictionBook && read(batch + ".bak") == original);
    CHECK(makeDefault(Group::FictionBook | Group::Epub, batchPath, stockPath) == Result::AlreadyDefault);
    CHECK(makeDefault(0, batchPath, stockPath) == Result::AlreadyDefault);
    CHECK(read(batch) == fictionBook && read(batch + ".bak") == original);

    const std::string shared = std::string(temp.data()) + "/shared.cfg";
    const lString32 sharedPath = Utf8ToUnicode(shared.c_str());
    const pid_t child = fork();
    CHECK(child >= 0);
    if (child == 0)
        _exit(makeDefault(Group::Epub, sharedPath, stockPath) == Result::Updated ? 0 : 1);
    const auto result = makeDefault(Group::FictionBook, sharedPath, stockPath);
    int status;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    CHECK(result == Result::Updated);
    CHECK(contains(read(shared), "epub:@EPUB_file:1:cr3-pb.app,stock-epub.app:ICON_EPUB\n"));
    CHECK(contains(read(shared), "fb2:@FB2_file:1:cr3-pb.app,stock-fb.app:ICON_FB2\n"));

    puts("PASS: default-reader groups and all formats, preserved reader order and file metadata, backups and errors");
    return 0;
}
