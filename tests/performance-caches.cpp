#include "crengine.h"
#include "lvdocview.h"
#include "../crengine/src/lvfont/lvfontglyphcache.h"
#include "../crengine/src/lvfont/lvfontmetadatacache.h"
#include "../cr3gui/src/pbscreenupdate.h"
#include "../cr3gui/src/pbprefetch.h"
#include "testutil.h"
#include <string>
#include <unistd.h>
lString32 pbSkinFileName, openedCacheFile, currentCacheDir;

static int glyphs() {
    int size = offsetof(LVFontGlyphCacheItem, bmp) + 64;
    LVFontGlobalGlyphCache global(3 * size);
    LVFontLocalGlyphCache local(&global);
    auto add = [&](int ch) {
        LVFontGlyphCacheItem *item = LVFontGlyphCacheItem::newItem(&local, ch, 8, 8, 8, 64);
        local.put(item);
    };
    for (int repeat = 0; repeat < 50; ++repeat) {
        local.clear();
        add('a'); add('b'); add('c');
        CHECK(local.get('a') && local.get('b') && local.get('c'));
        // Access the oldest entry, then overflow: b, not a, must be evicted.
        CHECK(local.get('a'));
        add('d');
        CHECK(local.get('a') && !local.get('b') && local.get('c') && local.get('d'));
        global.clear();
        CHECK(!local.get('a') && !local.get('c') && !local.get('d'));
    }
    puts("PASS: glyph LRU promotes the oldest hit; repeated local/global clears retain full capacity");
    return 0;
}

static int screen() {
    LVGrayDrawBuf packedSource(16, 2, 4);
    for (int x = 0; x < 16; ++x) packedSource.FillRect(x, 0, x+1, 2, x * 0x101010);
    for (int width = 1; width <= 7; ++width) {
        unsigned char packed[16]; memset(packed, 0xa5, sizeof(packed));
        pbPackGray4(packedSource, packed, 3, 0, width, 2);
        for (int y = 0; y < 2; ++y) for (int x = 0; x < width; ++x) {
            int shift = x % 2 ? 0 : 4;
            CHECK(((packed[y * ((width+1)/2) + x/2] >> shift) & 15) == x + 3);
        }
        CHECK(packed[((width+1)/2) * 2] == 0xa5);
    }
    for (int bits : {1, 2, 3, 4, 8}) {
        LVGrayDrawBuf canvas(28, 12, bits), front(28, 12, bits);
        canvas.Clear(0xffffff); front.Clear(0xffffff);
        lUInt32 paper = front.GetPixel(0, 0);
        canvas.FillRect(22, 4, 23, 5, 0); // Outside even the packed-byte update bounds.
        canvas.FillRect(8, 4, 9, 5, 0);
        canvas.FillRect(13, 6, 14, 7, 0x808080);
        bool gray;
        lvRect dirty(7, 3, 14, 8);
        lvRect changed = pbSyncScreenRegion(canvas, front, dirty, gray);
        CHECK(!changed.isEmpty() && changed.top == 4 && changed.bottom <= 7);
        CHECK(front.GetPixel(22, 4) == paper);
        CHECK(front.GetPixel(8, 4) == canvas.GetPixel(8, 4));
        CHECK(front.GetPixel(13, 6) == canvas.GetPixel(13, 6));
        CHECK(pbSyncScreenRegion(canvas, front, dirty, gray).isEmpty());
        // Empty and clipped rectangles are safe at packed-byte boundaries.
        CHECK(pbSyncScreenRegion(canvas, front, lvRect(-8, -5, -1, 2), gray).isEmpty());
        canvas.FillRect(27, 11, 28, 12, 0);
        CHECK(!pbSyncScreenRegion(canvas, front, lvRect(27, 11, 100, 100), gray).isEmpty());
        CHECK(front.GetPixel(27, 11) == canvas.GetPixel(27, 11));
    }
    // Removing an old gray pixel needs grayscale just as adding one does.
    for (int bits : {2, 3, 4, 8}) {
        LVGrayDrawBuf canvas(16, 4, bits), front(16, 4, bits);
        front.Clear(0x808080); canvas.Clear(0xffffff);
        bool gray = false;
        CHECK(!pbSyncScreenRegion(canvas, front, lvRect(4, 0, 8, 4), gray).isEmpty());
        CHECK(gray);
    }
    puts("PASS: narrow/clipped screen synchronization, packed pixels, unchanged rows and gray-to-white transitions");
    return 0;
}

static int pages(const char *font) {
    InitFontManager(lString8::empty_str);
    CHECK(fontMan->RegisterFont(lString8(font)));
    {
        LVDocView view(8);
        view.setDefaultFontFace(lString8("DejaVu Sans"));
        view.Resize(600, 800); view.overrideVisiblePageCount(1);
        std::string xml = "<FictionBook><body><section>";
        for (int i = 0; i < 250; ++i) xml += "<p>Nearby page cache: text must remain unchanged when direction reverses.</p>";
        xml += "</section></body></FictionBook>";
        CHECK(view.LoadDocument(LVCreateStringStream(lString8(xml.c_str())), U"cache.fb2"));
        view.Render(); CHECK(view.getPageCount() > 5);
        view.goToPage(2);
        auto current = view.getPageImage(0)->getDrawBufRef();
        CHECK(view.getCurPage() == 2); // Explicit navigation beats a pending restore.
        CHECK(view.prepareAdjacentPage(-1));
        auto previous = view.getPageImage(-1)->getDrawBufRef();
        CHECK(view.prepareAdjacentPage(1));
        auto next = view.getPageImage(1)->getDrawBufRef();
        CHECK(view.getPageImage(0)->getDrawBuf() == current.get());
        CHECK(view.getPageImage(-1)->getDrawBuf() == previous.get());
        view.goToPage(3);
        CHECK(view.getPageImage(0)->getDrawBuf() == next.get());
        CHECK(view.prepareAdjacentPage(1));
        CHECK(view.getPageImage(-1)->getDrawBuf() == current.get());
        view.goToPage(2);
        CHECK(view.getPageImage(0)->getDrawBuf() == current.get());
        CHECK(view.prepareAdjacentPage(-1));
        CHECK(view.getPageImage(1)->getDrawBuf() == next.get());
        view.goToPage(3); view.getPageImage(0);
        int targetY = (*view.getPageList())[2]->start;
        view.requestRender();
        view.SetPos(targetY); view.getPageImage(0);
        CHECK(view.getCurPage() == 2); // Scroll-position commands also beat stale restores.
        PBPrefetchDirection direction;
        direction.observe(&view, 3); direction.observe(&view, 2);
        CHECK(direction.direction() == -1);
        direction.observe(&view, 2); CHECK(direction.direction() == -1);
        direction.observe(&view, 1); CHECK(direction.direction() == -1);
        direction.observe(&view, 20); CHECK(direction.direction() == 1);
        direction.observe(&direction, 2); CHECK(direction.direction() == 1);
    }
    ShutdownFontManager();
    puts("PASS: previous/current/next images survive both reading directions; jumps and new books reset prediction");
    return 0;
}

static int metadata(const char *font, const char *directory) {
    lString32 path = Utf8ToUnicode(directory) + "/cached-font.ttf";
    lString32 registry = Utf8ToUnicode(directory) + "/font-metadata.ini";
    auto copyFont = [&]() { return LVWriteFileAtomic(path, LVOpenFileStream(font, LVOM_READ), false); };
    CHECK(copyFont());
    LVDeleteFile(registry);
    lUInt32 hash = 0;
    int count = 0;
    for (int pass = 0; pass < 5; ++pass) {
        if (pass == 2) CHECK(copyFont()); // Same size/content, different inode/timestamps.
        if (pass == 3) {
            CRPropRef damaged = LVCreatePropsContainer();
            LVStreamRef file = LVOpenFileStream(registry.c_str(), LVOM_READ);
            CHECK(damaged->loadFromStream(file.get()));
            damaged->setString("font.0.face.0.name", "Wrong font name");
            LVStreamRef bytes = LVCreateMemoryStream();
            CHECK(damaged->saveToStream(bytes.get()) && LVWriteFileAtomic(registry, bytes, false));
        }
        InitFontManager(lString8::empty_str);
        fontMan->BeginFontMetadataCache(registry);
        CHECK(fontMan->RegisterFont(UnicodeToUtf8(path)));
        CHECK(fontMan->GetFontMetadataCacheHits() == (pass == 1 || pass == 4 ? 1 : 0));
        if (pass == 0) { count = fontMan->GetFontCount(); hash = fontMan->GetFontListHash(-1); }
        CHECK(fontMan->GetFontCount() == count && fontMan->GetFontListHash(-1) == hash);
        auto face = fontMan->GetFont(32, 400, false, css_ff_sans_serif, lString8("DejaVu Sans"));
        CHECK(!face.isNull() && face->getGlyph('A'));
        lString8 before = LVFontMetadataCache::fingerprint(UnicodeToUtf8(registry));
        CHECK(fontMan->SaveFontMetadataCache());
        if (pass == 1 || pass == 4)
            CHECK(before == LVFontMetadataCache::fingerprint(UnicodeToUtf8(registry)));
        face.Clear(); ShutdownFontManager();
    }
    CHECK(LVDeleteFile(path));
    InitFontManager(lString8::empty_str);
    fontMan->BeginFontMetadataCache(registry);
    CHECK(!fontMan->RegisterFont(UnicodeToUtf8(path)) && fontMan->GetFontCount() == 0);
    ShutdownFontManager();
    puts("PASS: cold/warm font registry, actual glyph loading, unchanged hash, replaced/deleted fonts, checksum recovery and no redundant writes");
    return 0;
}

int main(int argc, char **argv) {
    if (argc != 3) return 1;
    CHECK(glyphs() == 0); CHECK(screen() == 0);
    CHECK(pages(argv[1]) == 0); CHECK(metadata(argv[1], argv[2]) == 0);
    return 0;
}
