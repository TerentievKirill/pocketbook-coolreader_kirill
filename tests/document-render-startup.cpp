#include "lvxmlutils.h"
#include "crengine.h"
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
    CHECK(view.LoadDocument(lString32(Utf8ToUnicode(epubFile))));
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

static int testRendering(const char *defaultsFile, const char *epubFile, const char *fb2Css, const char *flibustaFile, const char *epubCss) {
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
    LVDocView epub(8);
    epub.setDefaultFontFace(lString8("DejaVu Sans"));
    epub.Resize(1072, 1448);
    CHECK(epub.LoadDocument(epubFile));
    epub.Render();
    CHECK(epub.getPageCount() > 0 && epub.getTitle() == lString32("EPUB regression"));
    LVDocImageRef painted = epub.getPageImage(0);
    CHECK(!painted.isNull() && painted->getDrawBuf()->GetWidth() > 0);
    puts("PASS: UTF-32 supplementary characters and EPUB CSS/image page painting");
    CHECK(testFlibustaEpubFootnotes(flibustaFile, epubCss) == 0);
    return 0;
}
int main(int argc, char **argv) {
    setbuf(stdout, NULL);
    // Every packed pixel lane, gray appearing/disappearing, black/white only.
    for (int a = 0; a < 256; ++a) for (int b = 0; b < 256; ++b) {
        bool expected = false;
        for (int p = 0; p < 4; ++p) {
            int x = (a >> (2*p)) & 3, y = (b >> (2*p)) & 3;
            expected |= x != y && ((x % 3) != 0 || (y % 3) != 0);
        }
        CHECK(pbGray2Changed(a, b) == expected);
    }
    if (argc < 10) return 1;
    InitFontManager(lString8::empty_str);
    if (!fontMan->RegisterFont(lString8(argv[1])) || !fontMan->RegisterFont(lString8(argv[2]))) return 2;
    if (!fontMan->RegisterFont(lString8(argv[3]))) return 2;
    HyphMan::initDictionaries(lString32(argv[5]));
    int result = testRendering(argv[4], argv[6], argv[7], argv[8], argv[9]);
    HyphMan::uninit();
    ShutdownFontManager();
    return result;
}
