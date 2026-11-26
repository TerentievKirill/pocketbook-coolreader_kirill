#include "lvxmlutils.h"
#include "crengine.h"
#include "fb2def.h"
#include "lvdocview.h"
#include "lvrend.h"
#include "testutil.h"
#include "../cr3gui/src/settings.h"
#include "../cr3gui/src/pb_device_profile.h"
#include "../cr3gui/src/pbgrayupdate.h"
#include <stdio.h>
#include <string>
#include <vector>
lString32 pbSkinFileName;
lString32 openedCacheFile;
lString32 currentCacheDir;
DECL_DEF_CR_FONT_SIZES;

static int testPageLookup() {
    const auto linearLookup = [](const LVRendPageList &pages, int y, int direction) {
        for (int i = 0; i < pages.length(); ++i) {
            const auto *page = pages[i];
            if (y < page->start)
                return i > 0 && direction < 0 ? i - 1 : i;
            if (y < page->start + page->height) {
                if (i + 1 < pages.length() && direction > 0)
                    return i + 1;
                return i > 0 && direction < 0 ? i - 1 : i;
            }
        }
        return pages.empty() ? 0 : pages.length() - 1;
    };
    const auto matches = [&](LVRendPageList &pages) {
        for (int y = -10; y <= 120; ++y)
            for (int direction : {-1, 0, 1})
                if (pages.FindNearestPage(y, direction) != linearLookup(pages, y, direction))
                    return false;
        return true;
    };

    LVRendPageList pages;
    CHECK(matches(pages));
    pages.add(new LVRendPageInfo(12)); // Cover.
    CHECK(matches(pages));
    pages.add(new LVRendPageInfo(14, 8, pages.length()));
    pages.add(new LVRendPageInfo(20, 24, pages.length()));
    auto *continuedNote = new LVRendPageInfo(44, 0, pages.length());
    continuedNote->footnotes.add(LVPageFootNoteInfo(100, 10));
    pages.add(continuedNote);
    pages.add(new LVRendPageInfo(44, 0, pages.length()));
    pages.add(new LVRendPageInfo(50, 20, pages.length()));
    pages.add(new LVRendPageInfo(56, 5, pages.length()));
    pages.add(new LVRendPageInfo(42, 10, pages.length()));
    pages.add(new LVRendPageInfo(82, 12, pages.length()));
    CHECK(matches(pages));

    SerialBuf saved(0, true);
    CHECK(pages.serialize(saved));
    const int count = pages.length();
    pages.clear();
    CHECK(matches(pages));
    for (int i = 0; i < count; ++i)
        pages.add(new LVRendPageInfo(i * 10 + 5, 8, i));
    CHECK(matches(pages));
    saved.setPos(0);
    CHECK(pages.deserialize(saved));
    CHECK(pages.length() == count && pages[3]->footnotes.length() == 1 && matches(pages));
    puts("PASS: page lookup directions, gaps, overlapping pages, footnote continuations, and deserialization");
    return 0;
}

static lvRect inkBounds(LVDrawBuf *buf) {
    lvRect ink(buf->GetWidth(), buf->GetHeight(), 0, 0);
    lUInt32 paper = buf->GetPixel(0, 0);
    for (int y = 0; y < buf->GetHeight(); ++y)
        for (int x = 0; x < buf->GetWidth(); ++x)
            if (buf->GetPixel(x, y) != paper) {
                if (x < ink.left) ink.left = x;
                if (y < ink.top) ink.top = y;
                if (x+1 > ink.right) ink.right = x+1;
                if (y+1 > ink.bottom) ink.bottom = y+1;
            }
    return ink;
}
static int testMarginPainting() {
    LVDocView margins(8);
    margins.setDefaultFontFace(lString8("DejaVu Sans"));
    margins.Resize(1072, 1448);
    CRPropRef props = LVCreatePropsContainer();
    props->setInt(PROP_STATUS_LINE, 2);
    props->setInt(PROP_FONT_SIZE, 32);
    props->setBool(PROP_FLOATING_PUNCTUATION, false);
    props->setHex(PROP_BACKGROUND_COLOR, 0xffffff);
    props->setHex(PROP_FONT_COLOR, 0);
    const char *keys[] = {PROP_PAGE_MARGIN_LEFT, PROP_PAGE_MARGIN_TOP,
                         PROP_PAGE_MARGIN_RIGHT, PROP_PAGE_MARGIN_BOTTOM};
    for (int i = 0; i < 4; ++i) props->setInt(keys[i], 20);
    margins.propsApply(props);
    margins.setStyleSheet(lString8("body,p { display: block; margin: 0; padding: 0; text-indent: 0; text-align: left; }"));
    std::string xml = "<FictionBook><body><section><p>";
    for (int i = 0; i < 2000; ++i) xml += "Margin painting test. ";
    xml += "</p></section></body></FictionBook>";
    LVStreamRef stream = LVCreateMemoryStream(const_cast<char *>(xml.data()), xml.size(), true);
    CHECK(margins.LoadDocument(stream, U"margins.fb2"));
    margins.Render();
    lvRect before = inkBounds(margins.getPageImage(0)->getDrawBuf());
    // Match the frontend's old/new preferences delta, changing only margins.
    CRPropRef changed = LVClonePropsContainer(props);
    for (int i = 0; i < 4; ++i) changed->setInt(keys[i], 100);
    margins.propsApply(props ^ changed);
    lvRect after = inkBounds(margins.getPageImage(0)->getDrawBuf());
    CHECK(after.left - before.left >= 75 && after.left - before.left <= 85);
    CHECK(after.top - before.top == 80);
    CHECK(after.right <= 1072-100 && after.bottom <= 1448-100);
    CHECK(before.right > after.right && before.bottom > after.bottom);
    CHECK(margins.getPageMargins() == lvRect(100,100,100,100));
    puts("PASS: margin-only settings delta immediately changes all four painted page bounds");
    return 0;
}

static std::string pagePixels(LVDocView &view) {
    LVDocImageRef image = view.getPageImage(0);
    LVDrawBuf *buf = image->getDrawBuf();
    std::string pixels;
    for (int y = 0; y < buf->GetHeight(); ++y)
        pixels.append((const char *)buf->GetScanLine(y), buf->GetRowSize());
    return pixels;
}

static int testAnnotationPainting() {
    for (int flags : {BLOCK_RENDERING_FLAGS_DEFAULT, BLOCK_RENDERING_FLAGS_LEGACY}) {
        LVDocView view(8);
        view.setDefaultFontFace(lString8("DejaVu Sans"));
        view.Resize(600, 800);
        view.setShowCover(false);
        CRPropRef prefs = LVCreatePropsContainer();
        prefs->setInt(PROP_STATUS_LINE, 2);
        prefs->setInt(PROP_RENDER_BLOCK_RENDERING_FLAGS, flags);
        prefs->setInt(PROP_HIGHLIGHT_COMMENT_BOOKMARKS, highlight_mode_solid);
        view.propsApply(prefs);
        view.setStyleSheet(lString8("body,section,p { display: block; } p { padding: 9px 12px; }"));
        const lString8 book("<FictionBook><body><section><p id='text'>"
                            "Highlighted words and bookmarked words on one page."
                            "</p></section></body></FictionBook>");
        CHECK(view.LoadDocument(LVCreateStringStream(book), U"annotations.fb2"));
        CHECK(!view.getPageImage(0).isNull());
        ldomNode *paragraph = view.getDocument()->getElementById(U"text");
        CHECK(paragraph && paragraph->getChildCount() == 1);
        ldomNode *text = paragraph->getChildNode(0);
        CHECK(text->isText());
        CHECK(view.goToPage(view.getBookmarkPage(ldomXPointer(text, 0)), false));
        const auto plain = pagePixels(view);
        ldomXRange highlight(ldomXPointer(text, 0), ldomXPointer(text, 11));
        highlight.setFlags(1);
        view.selectRange(highlight);
        const auto highlighted = pagePixels(view);
        CHECK(highlighted != plain);
        ldomXRange note(ldomXPointer(text, 22), ldomXPointer(text, 32));
        CRBookmark *bookmark = view.saveRangeBookmark(note, bmkt_comment, U"A saved annotation");
        CHECK(bookmark && pagePixels(view) != highlighted);
        view.clearSelection();
        CHECK(pagePixels(view) != plain);
        CHECK(view.removeBookmark(bookmark));
        CHECK(pagePixels(view) == plain);
    }
    puts("PASS: plain text, selections and bookmarks paint correctly with enhanced and legacy geometry");
    return 0;
}

static int testFontWeight() {
    LVDocView view(8);
    view.setDefaultFontFace(lString8("DejaVu Sans"));
    view.Resize(600, 800);
    CRPropRef preferences = LVCreatePropsContainer();
    preferences->setInt(PROP_STATUS_LINE, 2);
    preferences->setInt(PROP_FONT_SIZE, 32);
    preferences->setInt(PROP_FONT_BASE_WEIGHT, 400);
    preferences->setInt(PROP_FONT_WEIGHT_EMBOLDEN, 0);
    view.propsUpdateDefaults(preferences);
    view.propsApply(preferences);
    view.setStyleSheet(lString8("body,section,p { display: block; } strong { font-weight: bold; } em { font-style: italic; }"));
    lString8 xml("<FictionBook><body><section><p>Font weight: Привет! Regular, <strong>bold</strong>, <em>italic</em>.</p></section></body></FictionBook>");
    CHECK(view.LoadDocument(LVCreateStringStream(xml), U"weight.fb2"));
    const char *strengths[] = {"0", "0.25", "0.5", "0.75", "1", "1.25", "1.5", "1.75"};
    const int weights[] = {400, 475, 550, 625, 700, 775, 850, 925};
    std::vector<std::string> images;
    for (unsigned i = 0; i < sizeof(weights)/sizeof(weights[0]); ++i) {
        // Match the menu's settings-only delta, including its old base weight.
        CRPropRef changed = LVClonePropsContainer(preferences);
        changed->setString(PROP_FONT_WEIGHT_EMBOLDEN, strengths[i]);
        view.propsApply(preferences ^ changed);
        preferences = changed;
        CHECK(LVRendGetBaseFontWeight() == weights[i]);
        CHECK(view.propsGetCurrent()->getIntDef(PROP_FONT_BASE_WEIGHT) == weights[i]);
        images.push_back(pagePixels(view));
        for (unsigned j = 0; j < i; ++j) CHECK(images[i] != images[j]);

        // Saved fractional choices must survive validation and a full reload.
        CRPropRef current = LVClonePropsContainer(view.propsGetCurrent());
        LVStreamRef saved = LVCreateMemoryStream(NULL, 0, false, LVOM_READWRITE);
        CHECK(current->saveToStream(saved.get()));
        CHECK(saved->SetPos(0) == 0);
        LVStreamRef readable = LVCreateMemoryStream(saved);
        CHECK(!readable.isNull());
        CRPropRef loaded = LVCreatePropsContainer();
        CHECK(loaded->loadFromStream(readable.get()));
        view.propsUpdateDefaults(loaded);
        CHECK(loaded->getIntDef(PROP_FONT_BASE_WEIGHT) == weights[i]);
        CHECK(loaded->getStringDef(PROP_FONT_WEIGHT_EMBOLDEN) == lString32(strengths[i]));
        view.propsApply(loaded);
        CHECK(LVRendGetBaseFontWeight() == weights[i]);
        CHECK(pagePixels(view) == images[i]);
    }
    // Key toggles still return to the original Normal/Bold appearances.
    view.doCommand((LVDocCmd)DCMD_TOGGLE_BOLD, 0);
    CHECK(LVRendGetBaseFontWeight() == 400 && pagePixels(view) == images[0]);
    view.doCommand((LVDocCmd)DCMD_TOGGLE_BOLD, 0);
    CHECK(LVRendGetBaseFontWeight() == 700 && pagePixels(view) == images[4]);
    puts("PASS: quarter-step font weights paint distinct text, survive reloads and preserve Normal/Bold toggles");
    return 0;
}
static int testFlibustaEpubFootnotes(const char *epubFile, const char *epubCss) {
    LVDocView view(8);
    view.setDefaultFontFace(lString8("DejaVu Sans"));
    view.Resize(1072, 1448);
    view.setStyleSheet(UnicodeToUtf8(LVReadTextFile(lString32(epubCss))));
    CRPropRef prefs = LVCreatePropsContainer();
    prefs->setBool(PROP_FOOTNOTES, true);
    view.propsApply(prefs);
    CHECK(view.LoadDocument(epubFile));
    view.Render();
    int fragments = 0;
    LVRendPageList *pages = view.getPageList();
    for (int i = 0; i < pages->length(); ++i)
        fragments += (*pages)[i]->footnotes.length();
    printf("Flibusta EPUB regression: %d page-footnote fragments\n", fragments);
    CHECK(fragments >= 2); // separate span and p notes, not chapter comments
    puts("PASS: Flibusta-style EPUB span/p notes render in page footers");
    return 0;
}

static int testLargeGroupedFootnotes(const char *epubFile, const char *epubCss) {
    LVDocView view(8);
    view.setDefaultFontFace(lString8("DejaVu Sans"));
    view.Resize(1072, 1448);
    view.setStyleSheet(UnicodeToUtf8(LVReadTextFile(lString32(epubCss))));
    CRPropRef prefs = LVCreatePropsContainer();
    prefs->setBool(PROP_FOOTNOTES, true);
    view.propsApply(prefs);
    CHECK(view.LoadDocument(epubFile));
    view.Render();
    int fragments = 0;
    LVRendPageList *pages = view.getPageList();
    for (int i = 0; i < pages->length(); ++i)
        fragments += (*pages)[i]->footnotes.length();
    printf("Tolstoy-shaped EPUB: %d footnote fragments on %d pages\n", fragments, pages->length());
    CHECK(fragments >= 999);
    puts("PASS: 999 references to notes in 464/507/28 nested-span groups");
    return 0;
}

static int testRendering(const char *defaultsFile, const char *epubFile, const char *fb2Css, const char *flibustaFile, const char *epubCss, const char *largeNotesEpub) {
    LVDocView view(8);
    view.setDefaultFontFace(lString8("DejaVu Sans"));
    view.Resize(1072, 1448);
    view.createDefaultDocument(lString32("Startup test"), lString32("No book loaded yet"));
    view.Render();
    if (view.getPageCount() < 1) return 3;
    puts("PASS: startup placeholder document rendered");
    std::string xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><FictionBook xmlns=\"http://www.gribuser.ru/xml/fictionbook/2.0\"><description><title-info><book-title>Test</book-title><lang>ru</lang></title-info></description><body><section><title><p>Test</p></title>";
    for (int i=0; i<50; ++i)
        xml += "<p>Привет! Проверка чтения книги. Text for paragraph layout and page rendering.</p>";
    xml += "</section></body></FictionBook>";
    LVStreamRef stream = LVCreateMemoryStream(const_cast<char *>(xml.data()), xml.size(), true);
    if (!view.LoadDocument(stream, U"regression.fb2")) return 4;
    view.Render();
    if (view.getPageCount() < 1) return 5;
    printf("PASS: Cyrillic FB2 rendered, %d pages\n", view.getPageCount());
    // Warming must preserve position and produce exactly the on-demand bitmap.
    view.goToPage(0);
    CHECK(!view.prepareAdjacentPage(1)); // Current page has not been painted.
    view.getPageImage(0);
    CHECK(view.getPageCount() > 1);
    CHECK(view.prepareAdjacentPage(1));
    CHECK(view.getCurPage() == 0);
    CHECK(!view.prepareAdjacentPage(1)); // Already warm, no repeated work.
    view.goToPage(1);
    LVDocImageRef warm = view.getPageImage(0);
    LVDrawBuf *warmBuf = warm->getDrawBuf();
    std::string pixels;
    for (int y = 0; y < warmBuf->GetHeight(); ++y)
        pixels.append((const char *)warmBuf->GetScanLine(y), warmBuf->GetRowSize());
    warm.Clear();
    view.clearImageCache();
    LVDocImageRef cold = view.getPageImage(0);
    LVDrawBuf *coldBuf = cold->getDrawBuf();
    for (int y = 0; y < coldBuf->GetHeight(); ++y)
        CHECK(!memcmp(pixels.data() + y * coldBuf->GetRowSize(),
                      coldBuf->GetScanLine(y), coldBuf->GetRowSize()));
    cold.Clear();
    view.goToPage(view.getPageCount() - 1);
    view.getPageImage(0);
    CHECK(!view.prepareAdjacentPage(1));
    CHECK(!view.prepareAdjacentPage(0));
    view.requestRender();
    CHECK(!view.prepareAdjacentPage(1)); // Never start layout from idle work.
    view.getPageImage(0); // Finish layout/restore its saved reading position first.
    view.goToPage(0);
    view.getPageImage(0);
    CHECK(view.getCurPage() == 0);
    CHECK(!view.prepareAdjacentPage(-1));
    puts("PASS: idle prefetch preserves position/pixels and skips invalid or cold layouts");
    CHECK(!view.getPageHeaderPages(0, view.getPageCount(), true).empty());
    CHECK(!view.getCurrentChapterName().empty());
    CHECK(view.getSectionBoundsPages().length() >= 1);
    CHECK(view.currentPageIsBookmarked() == NULL);
    CHECK(!view.removeCurrentPageShortcutBookmark());
    // A fresh installation must retain the captured font/layout after validation
    // and render successfully with the font supplied by the target firmware.
    CRPropRef defaults = LVCreatePropsContainer();
    LVStreamRef config = LVOpenFileStream(defaultsFile, LVOM_READ);
    CHECK(!config.isNull() && defaults->loadFromStream(config.get()));
#ifdef CR3_SHARED_PACKAGE
    pbdevice::applyDefaults(defaults, *pbdevice::identify("PB632"));
#endif
    CRPropRef preferences = LVClonePropsContainer(defaults);
    LVArray<int> sizes(cr_font_sizes, sizeof(cr_font_sizes) / sizeof(int));
    view.setFontSizes(sizes, true);
    view.propsUpdateDefaults(preferences);
    const char *keys[] = {PROP_FONT_FACE, PROP_FONT_SIZE, PROP_INTERLINE_SPACE,
                         PROP_PAGE_MARGIN_LEFT, PROP_PAGE_MARGIN_RIGHT,
                         PROP_PAGE_MARGIN_TOP, PROP_PAGE_MARGIN_BOTTOM,
                         PROP_HYPHENATION_DICT};
    for (unsigned i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
        CHECK(preferences->getStringDef(keys[i]) == defaults->getStringDef(keys[i]));
    view.propsApply(preferences);
    view.Render();
    CHECK(view.getPageCount() > 0 && view.getFontSize() == defaults->getIntDef(PROP_FONT_SIZE));
    CHECK(testMarginPainting() == 0);
    // Every value exposed by the PocketBook margin menu must survive loading.
    const int marginValues[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,25,30,35,40,45,50,60,70,80,90,100,110,120,130,140,150,160,170,180,190,200,250,300};
    const char *marginKeys[] = {PROP_PAGE_MARGIN_LEFT, PROP_PAGE_MARGIN_TOP,
                                PROP_PAGE_MARGIN_RIGHT, PROP_PAGE_MARGIN_BOTTOM};
    for (unsigned i = 0; i < sizeof(marginValues)/sizeof(marginValues[0]); ++i) {
        CRPropRef margins = LVCreatePropsContainer();
        for (int j = 0; j < 4; ++j) margins->setInt(marginKeys[j], marginValues[i]);
        view.propsUpdateDefaults(margins);
        for (int j = 0; j < 4; ++j) CHECK(margins->getIntDef(marginKeys[j]) == marginValues[i]);
    }
    // Legacy PocketBook modes must keep the status line visible at Top (0)
    // and hidden at Off (2), both through settings and the direct key toggle.
    CHECK(defaults->getIntDef(PROP_STATUS_LINE, -1) == 0);
    CHECK(view.getPageheaderPosition() == 1 && view.getPageHeaderHeight() > 0);
    CHECK(view.getPageHeaderInfo() & PGHDR_PAGE_NUMBER);
    CRPropRef status = LVCreatePropsContainer();
    status->setInt(PROP_STATUS_LINE, 2);
    view.propsApply(status);
    CHECK(view.getPageheaderPosition() == 0 && view.getPageHeaderHeight() == 0);
    status->setInt(PROP_STATUS_LINE, 0);
    view.propsApply(status);
    CHECK(view.getPageheaderPosition() == 1 && view.getPageHeaderHeight() > 0);
    lvRect header;
    view.getPageHeaderRectangle(1, header);
    CHECK(header.height() > 0 && header.top < view.GetHeight() / 2);
    view.setStatusMode(2, true, true, true, true, true, true, true);
    CHECK(view.getPageHeaderHeight() == 0);
    view.setStatusMode(0, true, true, true, true, true, true, true);
    CHECK(view.getPageHeaderHeight() > 0);
    view.setStatusMode(1, true, true, true, true, true, true, true);
    CHECK(view.getPageheaderPosition() == 2 && view.getPageHeaderHeight() > 0);
    view.setStatusMode(0, true, true, true, true, true, true, true);
    CRPropRef fresh = LVCreatePropsContainer();
    view.propsUpdateDefaults(fresh);
    CHECK(fresh->getIntDef(PROP_STATUS_LINE, -1) == 0);
    puts("PASS: legacy PocketBook status modes, settings and key toggle");
    // Saved preferences override the package; missing keys retain defaults.
    LVStreamRef user = LVCreateStringStream(lString8("crengine.font.size=32\ncrengine.page.margin.left=20\n"));
    CHECK(preferences->loadFromStream(user.get()));
    CHECK(preferences->getIntDef(PROP_FONT_SIZE) == 32);
    CHECK(preferences->getIntDef(PROP_PAGE_MARGIN_LEFT) == 20);
    CHECK(preferences->getStringDef(PROP_FONT_FACE) == defaults->getStringDef(PROP_FONT_FACE));
    CHECK(preferences->getStringDef(PROP_PAGE_MARGIN_RIGHT) == defaults->getStringDef(PROP_PAGE_MARGIN_RIGHT));
    puts("PASS: reader defaults render with firmware font; user preferences take precedence");
    CRPropRef legacyWeight = LVCreatePropsContainer();
    legacyWeight->setInt(PROP_FONT_WEIGHT_EMBOLDEN, 1);
    view.propsApply(legacyWeight);
    CHECK(view.propsGetCurrent()->getIntDef(PROP_FONT_BASE_WEIGHT) == 700);
    legacyWeight->setInt(PROP_FONT_WEIGHT_EMBOLDEN, 0);
    view.propsApply(legacyWeight);
    CHECK(view.propsGetCurrent()->getIntDef(PROP_FONT_BASE_WEIGHT) == 400);
    CHECK(testFontWeight() == 0);
    CHECK(testAnnotationPainting() == 0);
    CHECK(Utf8ToUnicode("😀").length() == 1);
    // Exercise the shipped stylesheet: upstream now requires explicit CSS
    // hints to collect FB2 notes/comments into the referencing page footer.
    for (int enabled = 0; enabled <= 1; ++enabled) {
        LVDocView notes(8);
        notes.setDefaultFontFace(lString8("DejaVu Sans"));
        notes.Resize(1072, 1448);
        notes.setStyleSheet(UnicodeToUtf8(LVReadTextFile(lString32(fb2Css))));
        CRPropRef notePrefs = LVClonePropsContainer(preferences);
        notePrefs->setBool(PROP_FOOTNOTES, enabled != 0);
        notes.propsApply(notePrefs);
        std::string fb2 = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><FictionBook xmlns=\"http://www.gribuser.ru/xml/fictionbook/2.0\" xmlns:l=\"http://www.w3.org/1999/xlink\"><description><title-info><book-title>Footnotes</book-title></title-info></description><body><section><p>Text with note <a l:href=\"#n1\" type=\"note\">1</a> and comment <a l:href=\"#c1\" type=\"note\">2</a>.</p></section></body><body name=\"notes\"><section id=\"n1\"><title><p>1</p></title><p>Footnote at the bottom.</p></section></body><body name=\"comments\"><section id=\"c1\"><title><p>2</p></title><p>Comment at the bottom.</p></section></body></FictionBook>";
        LVStreamRef noteStream = LVCreateMemoryStream(const_cast<char *>(fb2.data()), fb2.size(), true);
        CHECK(notes.LoadDocument(noteStream, U"footnotes.fb2"));
        notes.Render();
        int fragments = 0;
        LVRendPageList *pages = notes.getPageList();
        for (int i = 0; i < pages->length(); ++i)
            fragments += (*pages)[i]->footnotes.length();
        CHECK(enabled ? fragments == 2 : fragments == 0);
        CHECK(!notes.getPageImage(0).isNull());
    }
    puts("PASS: FB2 notes/comments appear in page footers; disabling footnotes is respected");
    {
        LVDocView sections(8);
        sections.setDefaultFontFace(lString8("DejaVu Sans"));
        sections.Resize(1072, 1448);
        sections.setStyleSheet(UnicodeToUtf8(LVReadTextFile(lString32(fb2Css))));
        sections.propsApply(LVClonePropsContainer(preferences));
        lString8 fb2(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<FictionBook xmlns=\"http://www.gribuser.ru/xml/fictionbook/2.0\">"
            "<stylesheet type=\"text/css\">section.floating {float: left; width: 8em}</stylesheet>"
            "<description><title-info><book-title>Sections</book-title></title-info></description>"
            "<body><title><p>Book title</p></title>\n"
            "<section id=\"first\"><title><p>First</p></title><p>Text</p></section>\n"
            "<section id=\"parent\"><title><p>Parent</p></title>\n"
            "<section id=\"child1\"><title><p>Child 1</p></title><p>Text</p></section>\n"
            "<section id=\"child2\"><p>Untitled child</p></section></section>\n"
            "<section id=\"float\" class=\"floating\"><title><p>Float</p></title><p>Text</p></section>\n"
            "<section id=\"after\"><title><p>After the float</p></title><p>Text</p></section>");
        for (int i = 0; i < 32; ++i)
            fb2 += "<section><p>Text</p></section>";
        fb2 += "</body><body name=\"notes\"><title><p>Notes</p></title>"
               "<section id=\"note\"><p>Note</p></section></body></FictionBook>";
        CHECK(sections.LoadDocument(LVCreateStringStream(fb2), U"sections.fb2"));
        CHECK(sections.getDocFormat() == doc_format_fb2);
        ldomDocument *book = sections.getDocument();
        LVTocItem *toc = book->getToc();
        const lString32 base(U"/FictionBook[1]/body[1]");
        CHECK(toc->getChildCount() == 37);
        CHECK(toc->getChild(0)->getPath() == base + U"/section[1]");
        LVTocItem *parent = toc->getChild(1);
        CHECK(parent->getPath() == base + U"/section[2]" && parent->getChildCount() == 2);
        CHECK(parent->getChild(0)->getPath() == parent->getPath() + U"/section[1]");
        CHECK(parent->getChild(1)->getPath() == parent->getPath() + U"/section[2]");
        CHECK(parent->getChild(1)->getName().empty());
        CHECK(toc->getChild(2)->getPath() == base + U"/section[3]");
        ldomNode *floatSection = book->getElementById(U"float");
        CHECK(floatSection && floatSection->getParentNode()->getNodeId() == el_floatBox);
        CHECK(toc->getChild(3)->getPath() == base + U"/section[3]");
        CHECK(toc->getChild(3)->getXPointer().getNode() == book->getElementById(U"after"));
        CHECK(toc->getChild(36)->getPath() == U"/FictionBook[1]/body[2]");

        // Cache records contain paths with no live pointers. Compare the bulk
        // resolver with ordinary XPath resolution on the same DOM.
        sections.Render();
        CHECK(book->getDOMVersionRequested() >= DOM_VERSION_WITH_NORMALIZED_XPOINTERS);
        const std::vector<lString32> paths = {
            base + U"/section[3]",
            base + U"/section[4]",
            base + U"/section[2]/section[2]",
            base + U"/section[36]/p[1]/text()[1].2",
            base + U"/section[100]",
            lString32(U"#after"),
            lString32(U"/FictionBook[1]/body[2]")
        };
        toc->clear();
        for (const auto &path : paths)
            toc->addChild(U"Destination", ldomXPointer(), path);
        CHECK(sections.getToc() == toc && toc->hasValidPageNumbers());
        for (int i = 0; i < toc->getChildCount(); ++i) {
            const auto expected = book->createXPointer(paths[i]);
            auto *item = toc->getChild(i);
            CHECK(item->getPath() == paths[i]);
            CHECK(item->getPage() == (expected.isNull() ? -1 : sections.getBookmarkPage(expected)));
            const auto resolved = item->getXPointer();
            CHECK(resolved.getNode() == expected.getNode() && resolved.getOffset() == expected.getOffset());
        }
    }
    puts("PASS: FB2 TOC path creation and bulk resolution preserve nested and float-wrapped sections");
    {
        LVDocView sections(8);
        sections.setDefaultFontFace(lString8("DejaVu Sans"));
        sections.Resize(1072, 1448);
        sections.setStyleSheet(UnicodeToUtf8(LVReadTextFile(lString32(fb2Css))));
        sections.propsApply(LVClonePropsContainer(preferences));
        lString8 fb2(
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
            "<FictionBook xmlns=\"http://www.gribuser.ru/xml/fictionbook/2.0\">"
            "<description><title-info><book-title>Paint ranges</book-title></title-info></description>"
            "<body><title><p>Book title</p></title>");
        for (int i = 0; i < 32; ++i)
            fb2 += "<section><title><p>Section</p></title><p>Text</p></section>";
        fb2 += "</body><body name=\"notes\"><section><p>Note</p></section></body></FictionBook>";
        CHECK(sections.LoadDocument(LVCreateStringStream(fb2), U"paint-ranges.fb2"));
        sections.Render();
        ldomDocument *book = sections.getDocument();
        ldomNode *main = book->nodeFromXPath(U"/FictionBook/body[1]");
        ldomNode *notes = book->nodeFromXPath(U"/FictionBook/body[2]");
        CHECK(main && notes && main->getChildCount() == 33 && notes->getChildCount() == 1);
        auto setBounds = [](ldomNode *node, int y, int overflow = 0, int height = 40) {
            RenderRectAccessor rect(node);
            rect.setX(0);
            rect.setY(y);
            rect.setWidth(200);
            rect.setHeight(height);
            rect.setTopOverflow(0);
            rect.setBottomOverflow(overflow);
            rect.setFlags(0);
        };
        setBounds(main, 0, 0, 3300);
        for (int i = 0; i < main->getChildCount(); ++i) {
            auto *child = main->getChildNode(i);
            child->setRendMethod(erm_final);
            setBounds(child, 100 * i);
        }
        setBounds(notes->getChildNode(0), 0);
        auto paintRangeIs = [&](ldomNode *body, int y, int expectedFirst, int expectedEnd) {
            int first = 0, end = body->getChildCount();
            book->getChapterPaintRange(body, -y, 10, first, end);
            return first == expectedFirst && end == expectedEnd;
        };
        CHECK(paintRangeIs(main, 1910, 19, 20));
        CHECK(paintRangeIs(main, 1940, 20, 20));
        CHECK(paintRangeIs(notes, 1910, 0, 1));
        auto *earlier = main->getChildNode(18);
        auto *later = main->getChildNode(19);
        CHECK(main->elementFromPoint(lvPoint(10, 1910), PT_DIR_EXACT) == later);
        CHECK(main->elementFromPoint(lvPoint(10, 1890), PT_DIR_SCAN_FORWARD_LOGICAL_FIRST) == later);
        CHECK(main->elementFromPoint(lvPoint(10, 1890), PT_DIR_SCAN_BACKWARD_LOGICAL_FIRST) == earlier);
        earlier->setRendMethod(erm_block);
        setBounds(earlier, 1800, 100);
        auto *descendant = earlier->getChildNode(0);
        descendant->setRendMethod(erm_final);
        setBounds(descendant, 100);
        CHECK(paintRangeIs(main, 1910, 18, 20)); // Keep both passes of the overflow group.
        CHECK(main->elementFromPoint(lvPoint(10, 1910), PT_DIR_EXACT) == descendant);
        earlier->setRendMethod(erm_inline);
        setBounds(earlier, 1800);
        setBounds(descendant, 1910);
        CHECK(main->elementFromPoint(lvPoint(10, 1910), PT_DIR_SCAN_FORWARD_LOGICAL_FIRST) == descendant);
        earlier->setRendMethod(erm_block);
        main->insertChildElement(el_floatBox);
        CHECK(paintRangeIs(main, 1910, 0, 34)); // Direct floats need the complete sibling traversal.
    }
    puts("PASS: FB2 paint ranges and point lookup preserve gaps, overflow, notes and unusual children");
    LVDocView epub(8);
    epub.setDefaultFontFace(lString8("DejaVu Sans"));
    epub.Resize(1072, 1448);
    CHECK(epub.LoadDocument(epubFile));
    epub.Render();
    CHECK(epub.getPageCount() > 0 && epub.getTitle() == lString32("EPUB regression"));
    LVDocImageRef painted = epub.getPageImage(0);
    CHECK(!painted.isNull() && painted->getDrawBuf()->GetWidth() > 0);
    puts("PASS: UTF-32 supplementary characters and EPUB CSS/image page painting");
    ldomDocument *doc = epub.getDocument();
    ldomNode *first = doc->getElementById(U"_doc_fragment_0");
    ldomNode *second = doc->getElementById(U"_doc_fragment_1");
    CHECK(first && first->getText().pos(U"Проверка EPUB") >= 0 && !first->hasAttribute(attr_NonLinear));
    CHECK(second && second->getText().pos(U"Nonlinear appendix.") >= 0 && second->hasAttribute(attr_NonLinear));
    CHECK(doc->getRootNode()->getText().pos(U"Excluded chapter.") < 0);
    CHECK(!doc->getElementById(U"_doc_fragment_2"));
    LVTocItem *toc = epub.getToc();
    CHECK(toc->getChildCount() == 1 && toc->getChild(0)->getName() == U"Chapter");
    puts("PASS: EPUB spine order, first duplicate ID, skipped entries, nonlinear appendix and NCX lookup");

    CHECK(toc->hasValidPageNumbers() && epub.getToc() == toc);
    LVTocItem *chapter = toc->getChild(0);
    LVTocItem *appendix = chapter->addChild(U"Appendix", ldomXPointer(second, 0), lString32::empty_str);
    CHECK(!toc->hasValidPageNumbers());
    CHECK(epub.getToc() == toc && toc->hasValidPageNumbers());
    CHECK(appendix->getPage() == epub.getBookmarkPage(appendix->getXPointer()));
    const lString32 appendixPath = appendix->getPath();
    appendix->setXPointer(ldomXPointer(first, 0));
    CHECK(!toc->hasValidPageNumbers() && appendix->getPath() != appendixPath);
    CHECK(epub.getToc() == toc && appendix->getPage() == epub.getBookmarkPage(appendix->getXPointer()));
    appendix->setXPointer(ldomXPointer());
    CHECK(!toc->hasValidPageNumbers());
    CHECK(epub.getToc() == toc && appendix->getPage() == -1 && appendix->getPercent() == -1);
    chapter->clear();
    CHECK(!toc->hasValidPageNumbers() && chapter->getChildCount() == 0);
    CHECK(epub.getToc() == toc && toc->hasValidPageNumbers());

    epub.Resize(640, 900);
    CHECK(epub.getToc() == toc); // Opening the TOC must resolve the pending reflow.
    const int pageAfterReflow = epub.getBookmarkPage(chapter->getXPointer());
    CHECK(toc->hasValidPageNumbers() && chapter->getPage() == pageAfterReflow);
    doc->buildAlternativeToc();
    CHECK(!toc->hasValidPageNumbers() && toc->hasAlternativeTocFlag());
    CHECK(epub.getToc() == toc && toc->hasValidPageNumbers() && toc->hasAlternativeTocFlag());
    CHECK(toc->getChildCount() > 0);
    puts("PASS: TOC page numbering follows pending reflow, nested edits and alternative TOC rebuilds");

    ldomNode *body = first->getParentNode();
    CHECK(body == second->getParentNode() && body->getChildCount() == 2);
    auto setBounds = [](ldomNode *node, int y, int topOverflow, int bottomOverflow) {
        RenderRectAccessor rect(node);
        rect.setY(y);
        rect.setHeight(40);
        rect.setTopOverflow(topOverflow);
        rect.setBottomOverflow(bottomOverflow);
    };
    setBounds(first, 100, 0, 0);
    setBounds(second, 200, 0, 0);
    auto paintRangeIs = [&](int y, int expectedFirst, int expectedEnd) {
        int from = 0, to = 2;
        doc->getChapterPaintRange(body, -y, 10, from, to);
        return from == expectedFirst && to == expectedEnd;
    };
    CHECK(paintRangeIs(210, 1, 2));
    CHECK(paintRangeIs(140, 1, 1)); // A fragment ending at the viewport start is offscreen.
    setBounds(first, 100, 0, 100);
    CHECK(paintRangeIs(210, 0, 2)); // Keep the whole two-pass group.
    setBounds(first, 100, 0, 0);
    setBounds(second, 200, 150, 0);
    CHECK(paintRangeIs(160, 1, 2)); // A later fragment can overflow upward.
    second->setRendMethod(erm_table_row);
    CHECK(paintRangeIs(300, 0, 2)); // Preserve the legacy table-row exceptions.
    puts("PASS: EPUB paint ranges, overflow groups, render-rectangle changes and table fallback");

    // Synthetic chapter boxes let us check gaps and overflow independently of CSS.
    CHECK(BLOCK_RENDERING_N(body, ENHANCED) && body->getRendMethod() == erm_block);
    while (body->getChildCount() < 32)
        body->insertChildElement(el_DocFragment);
    auto setBox = [](ldomNode *node, int y, int height, int overflow = 0) {
        RenderRectAccessor rect(node);
        rect.setX(0);
        rect.setY(y);
        rect.setWidth(200);
        rect.setHeight(height);
        rect.setTopOverflow(0);
        rect.setBottomOverflow(overflow);
        rect.setFlags(0);
    };
    setBox(body, 0, 3200);
    for (int i = 0; i < body->getChildCount(); ++i) {
        auto *fragment = body->getChildNode(i);
        fragment->setRendMethod(erm_final);
        setBox(fragment, i * 100, 40);
    }
    CHECK(body->elementFromPoint(lvPoint(10, 3001), PT_DIR_EXACT) == body->getChildNode(30));
    CHECK(body->elementFromPoint(lvPoint(10, 2950), PT_DIR_SCAN_FORWARD_LOGICAL_FIRST) == body->getChildNode(30));
    CHECK(body->elementFromPoint(lvPoint(10, 2950), PT_DIR_SCAN_BACKWARD_LOGICAL_FIRST) == body->getChildNode(29));
    auto *earlier = body->getChildNode(2);
    auto *descendant = earlier->insertChildElement(el_p);
    descendant->setRendMethod(erm_final);
    earlier->setRendMethod(erm_block);
    setBox(earlier, 200, 40, 3000);
    setBox(descendant, 2800, 40);
    CHECK(body->elementFromPoint(lvPoint(10, 3001), PT_DIR_EXACT) == descendant);
    earlier->setRendMethod(erm_inline);
    setBox(earlier, 200, 40);
    setBox(descendant, 3000, 40);
    CHECK(body->elementFromPoint(lvPoint(10, 3001), PT_DIR_SCAN_FORWARD_LOGICAL_FIRST) == descendant);
    puts("PASS: EPUB point lookup preserves chapter gaps, reverse scans, overflow and inline fallback");
    CHECK(testFlibustaEpubFootnotes(flibustaFile, epubCss) == 0);
    CHECK(testLargeGroupedFootnotes(largeNotesEpub, epubCss) == 0);
    return 0;
}
int main(int argc, char **argv) {
    setbuf(stdout, NULL);
    CHECK(testPageLookup() == 0);
    // Every packed pixel lane, gray appearing/disappearing, black/white only.
    for (int a = 0; a < 256; ++a) for (int b = 0; b < 256; ++b) {
        bool expected = false;
        for (int p = 0; p < 4; ++p) {
            int x = (a >> (2*p)) & 3, y = (b >> (2*p)) & 3;
            expected |= x != y && ((x % 3) != 0 || (y % 3) != 0);
        }
        CHECK(pbGray2Changed(a, b) == expected);
    }
    if (argc < 11) return 1;
    InitFontManager(lString8::empty_str);
    if (!fontMan->RegisterFont(lString8(argv[1])) || !fontMan->RegisterFont(lString8(argv[2]))) return 2;
    if (!fontMan->RegisterFont(lString8(argv[3]))) return 2;
    HyphMan::initDictionaries(lString32(argv[5]));
    int result = testRendering(argv[4], argv[6], argv[7], argv[8], argv[9], argv[10]);
    HyphMan::uninit();
    ShutdownFontManager();
    return result;
}
