#include "crengine.h"
#include "lvdocview.h"
#include "testutil.h"
#include <string>

lString32 pbSkinFileName, openedCacheFile, currentCacheDir;

static bool openBook(LVDocView &view, const std::string &paragraphs, int visiblePages = 1)
{
    view.Resize(visiblePages == 1 ? 420 : 840, 480);
    view.setViewMode(DVM_PAGES, visiblePages);
    view.overrideVisiblePageCount(visiblePages);
    view.setShowCover(false);
    view.setStyleSheet(lString8("body, section, p { display: block; margin: 0; padding: 0; } "
                                "p.break { page-break-before: always; }"));
    CRPropRef props = LVCreatePropsContainer();
    props->setInt(PROP_FONT_SIZE, 26);
    props->setInt(PROP_STATUS_LINE, 2);
    view.propsApply(props);
    const std::string book = "<FictionBook><body><section>" + paragraphs +
                             "</section></body></FictionBook>";
    if (!view.LoadDocument(LVCreateStringStream(lString8(book.c_str())), U"search.fb2"))
        return false;
    return !view.getPageImage(0).isNull();
}

static int testNavigation(int visiblePages)
{
    LVDocView view(8);
    CHECK(openBook(view, "<p>needle Needle on page one.</p>"
                         "<p class='break'>A page without a match.</p>"
                         "<p class='break'>Another NeEdLe.</p>"
                         "<p class='break'>needle needle soft&#173;hyphen</p>", visiblePages));
    CHECK(view.getPageCount() == 4);

    const ldomXRangeList &highlights = view.getDocument()->getSelections();
    CHECK(view.searchPages(U"NEEDLE", 0, 1) == 0);
    CHECK(highlights.length() == 2);
    CHECK(view.getMarkedRanges()->length() == 2);
    CHECK(view.searchPages(U"missing", 1, 1, 0) == -1);
    CHECK(view.getCurPage() == 0);
    CHECK(highlights.length() == 2);
    CHECK(view.searchPages(U"needle", 1, 1, 0) == 2);
    CHECK(highlights.length() == 1);
    CHECK(view.searchPages(U"needle", 1, 1, 2) == 3);
    CHECK(view.getCurPage() == (visiblePages == 1 ? 3 : 2));
    CHECK(highlights.length() == 2);
    CHECK(view.searchPages(U"needle", 1, 1, 3) == -1);
    CHECK(view.searchPages(U"needle", -1, 1, 3) == 0);
    CHECK(view.searchPages(U"needle", 1, -1, 0) == -1);
    CHECK(view.searchPages(U"needle", -1, -1, 0) == 3);
    CHECK(highlights.length() == 2);
    CHECK(highlights[0]->getStart().compare(highlights[1]->getStart()) < 0);
    CHECK(view.searchPages(U"needle", 1, -1, 3) == 2);
    CHECK(view.searchPages(U"needle", 1, -1, 2) == 0);

    CHECK(view.searchPages(U"SOFTHYPHEN", -1, -1, 0) == 3);
    CHECK(highlights.length() == 1);
    CHECK(highlights[0]->getEnd().getOffset() - highlights[0]->getStart().getOffset() == 11);
    return 0;
}

static int testPageBoundary()
{
    const int wordCount = 1000;
    const int stride = 10;
    std::string paragraph = "<p>";
    for (int i = 0; i < wordCount; ++i) {
        char word[stride + 1];
        snprintf(word, sizeof(word), "token%04d ", i);
        paragraph += word;
    }
    paragraph += "</p>";

    LVDocView view(8);
    CHECK(openBook(view, paragraph));
    CHECK(view.getPageCount() > 3);
    LVRef<ldomXRange> firstPage = view.getPageDocumentRange(0);
    LVRef<ldomXRange> secondPage = view.getPageDocumentRange(1);
    CHECK(!firstPage.isNull() && !secondPage.isNull());
    const ldomXPointerEx &start = secondPage->getStart();
    CHECK(firstPage->getStart().getNode() == start.getNode());
    const int offset = start.getOffset();
    CHECK(offset > 0 && offset % stride == 0);
    const lString32 text = start.getNode()->getText();
    const lString32 phrase = text.substr(offset - stride, 2 * stride - 1);

    const ldomXRangeList &highlights = view.getDocument()->getSelections();
    CHECK(view.searchPages(phrase, 0, 1) == 0);
    CHECK(highlights.length() == 1);
    CHECK(highlights[0]->getStart().getOffset() == offset - stride);
    CHECK(highlights[0]->getEnd().getOffset() == offset + stride - 1);
    CHECK(view.getMarkedRanges()->length() > 0);
    CHECK(view.searchPages(phrase, 1, 1) == -1);

    const lString32 nextPageWord = text.substr(offset, stride - 1);
    CHECK(view.searchPages(nextPageWord, 0, 1) == 1);
    CHECK(view.searchPages(nextPageWord, 1, -1) == -1);
    CHECK(view.searchPages(phrase, 1, -1) == 0);

    CHECK(view.searchPages(U"token", 0, 1) == 0);
    CHECK(highlights.length() == offset / stride);
    CHECK(highlights[highlights.length() - 1]->getStart().getOffset() == offset - stride);
    CHECK(view.searchPages(U"token", 1, 1) == 1);
    CHECK(highlights[0]->getStart().getOffset() == offset);
    return 0;
}

int main(int argc, char **argv)
{
    CHECK(argc == 2);
    InitFontManager(lString8::empty_str);
    CHECK(fontMan->RegisterFont(lString8(argv[1])));
    CHECK(testNavigation(1) == 0);
    CHECK(testNavigation(2) == 0);
    CHECK(testPageBoundary() == 0);
    ShutdownFontManager();
    puts("PASS: in-book search direction, wraparound, highlights, spreads and page boundaries");
    return 0;
}
