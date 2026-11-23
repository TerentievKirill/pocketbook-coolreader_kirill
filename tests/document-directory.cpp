#include "crengine.h"
#include "lvdocview.h"
#include "testutil.h"
#include <string>

lString32 pbSkinFileName, openedCacheFile, currentCacheDir;

static int testDocument(const lString32 &filename, const lString32 &cache)
{
    CHECK(ldomDocCache::init(cache, 16 * 1024 * 1024));
    std::string firstPage;
    int pageCount = 0;
    for (bool cached : {false, true}) {
        LVDocView view(8);
        view.Resize(600, 800);
        view.overrideVisiblePageCount(1);
        view.setDefaultFontFace(lString8("DejaVu Sans"));
        CRPropRef prefs = LVCreatePropsContainer();
        prefs->setInt(PROP_STATUS_LINE, 2);
        prefs->setInt(PROP_MIN_FILE_SIZE_TO_CACHE, 32 * 1024);
        view.propsUpdateDefaults(prefs);
        view.propsApply(prefs);
        view.setStyleSheet(lString8("html, body, p { display: block; }"));
        CHECK(view.LoadDocument(filename.c_str()));
        CHECK(view.getDocFormat() == doc_format_html);
        CHECK(view.getDocument()->isTocFromCacheValid() == cached);

        LVDocImageRef page = view.getPageImage(0);
        CHECK(!page.isNull());
        ldomNode *text = view.getDocument()->getElementById(U"styled");
        CHECK(text && !text->getStyle().isNull());
        CHECK(text->getStyle()->color.type == css_val_color && text->getStyle()->color.value == 0x345678);
        LVImageSourceRef image = view.getDocument()->getObjectImageSource(U"assets/изображение.png");
        CHECK(!image.isNull() && image->GetWidth() == 1 && image->GetHeight() == 1);
        CHECK(view.getDocumentFileStream(U"assets").isNull()); // A directory is not a book or resource.
        CHECK(view.getDocumentFileStream(U"assets/missing.png").isNull());

        LVDrawBuf *buf = page->getDrawBuf();
        std::string pixels;
        for (int y = 0; y < buf->GetHeight(); ++y)
            pixels.append(reinterpret_cast<const char *>(buf->GetScanLine(y)), buf->GetRowSize());

        if (!cached) {
            const lString32 lateFile = LVExtractPath(filename) + "new-neighbor.txt";
            CHECK(!LVOpenFileStream(lateFile.c_str(), LVOM_WRITE).isNull());
            LVContainerRef dir = view.getDocument()->getContainer();
            CHECK(!dir.isNull());
            bool found = false;
            for (int i = 0; i < dir->GetObjectCount(); ++i)
                if (lString32(dir->GetObjectInfo(i)->GetName()) == U"new-neighbor.txt")
                    found = true;
            CHECK(LVDeleteFile(lateFile));
            CHECK(found); // The first explicit listing sees files added after open.
            pageCount = view.getPageCount();
            CHECK(pageCount > 1);
            firstPage.swap(pixels);
            CHECK(view.updateCache() == CR_DONE);
        } else {
            CHECK(view.getPageCount() == pageCount);
            CHECK(pixels == firstPage);
        }
    }
    ldomDocCache::close();
    return 0;
}

int main(int argc, char **argv)
{
    CHECK(argc == 4);
    InitFontManager(lString8::empty_str);
    CHECK(fontMan->RegisterFont(lString8(argv[1])));
    CHECK(testDocument(Utf8ToUnicode(argv[2]), Utf8ToUnicode(argv[3])) == 0);
    ShutdownFontManager();
    puts("PASS: cold and cached document loads preserve relative images, nested stylesheets and first-page pixels");
    return 0;
}
