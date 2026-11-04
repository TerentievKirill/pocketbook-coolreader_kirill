#include "crengine.h"
#include "lvdocview.h"
#include "lvxmlutils.h"
#include "testutil.h"
#include "../cr3gui/src/pb_device_profile.h"
#include "../cr3gui/src/settings.h"
#include <string>
lString32 openedCacheFile;
lString32 currentCacheDir;
lString32 pbSkinFileName;
DECL_DEF_CR_FONT_SIZES;

int main(int argc, char **argv) {
    CHECK(argc == 3);
    CHECK(pbdevice::modelNumber("PocketBook 632 (PB632)") == "632");
    CHECK(pbdevice::modelNumber(" PB743g ") == "743G");
    CHECK(pbdevice::modelNumber(NULL).empty());
    CHECK(pbdevice::identify("PocketBook 634") != NULL);
    CHECK(!pbdevice::qualified("PB634", "U634.6.9.850"));
    CHECK(!pbdevice::qualified("PB1030", "U1030.6.11.1437"));
    CHECK(!pbdevice::qualified("PB632", NULL));
    CHECK(InitFontManager(lString8::empty_str));
    CHECK(fontMan->RegisterFont(lString8(argv[1])));
    for (const pbdevice::Profile &profile : pbdevice::profiles) {
        CHECK(pbdevice::qualified(profile.model, profile.firmware));
        CRPropRef defaults = LVCreatePropsContainer();
        LVStreamRef stream = LVOpenFileStream(argv[2], LVOM_READ);
        CHECK(!stream.isNull() && defaults->loadFromStream(stream.get()));
        pbdevice::applyDefaults(defaults, profile);
        CHECK(defaults->getStringDef(PROP_FONT_FACE) == U"DejaVu Sans");
        CHECK(defaults->getIntDef(PROP_FONT_SIZE) == 42);
        CHECK(defaults->getIntDef(PROP_PAGE_MARGIN_LEFT) == 24);
        CHECK(defaults->getIntDef(PROP_PAGE_MARGIN_RIGHT) == 24);
        CHECK(defaults->getStringDef(PROP_HYPHENATION_DICT) == U"@algorithm");
        for (int rotation = 0; rotation < 2; ++rotation) {
            LVDocView view(8);
            LVArray<int> sizes(cr_font_sizes, sizeof(cr_font_sizes) / sizeof(int));
            view.setFontSizes(sizes, true);
            view.setDefaultFontFace(lString8("DejaVu Sans"));
            view.Resize(rotation ? profile.height : profile.width, rotation ? profile.width : profile.height);
            CRPropRef preferences = LVClonePropsContainer(defaults);
            view.propsUpdateDefaults(preferences);
            view.propsApply(preferences);
            std::string text = "<FictionBook><body><section><title><p>Shared device test</p></title>";
            for (int i=0; i<80; ++i) text += "<p>Reading on a shared PocketBook package. Привет!</p>";
            text += "</section></body></FictionBook>";
            LVStreamRef book = LVCreateMemoryStream(const_cast<char *>(text.data()), text.size(), true);
            CHECK(view.LoadDocument(book, U"shared.fb2"));
            view.Render();
            CHECK(view.getPageCount() > 0 && view.getFontSize() == 42);
            LVDrawBuf *pixels = view.getPageImage(0)->getDrawBuf();
            bool painted = false;
            for (int y=0; y<pixels->GetHeight() && !painted; y+=4)
                for (int x=0; x<pixels->GetWidth(); x+=4)
                    if (pixels->GetPixel(x,y) != pixels->GetPixel(0,0)) { painted=true; break; }
            CHECK(painted);
        }
        CRPropRef saved = LVClonePropsContainer(defaults);
        LVStreamRef user = LVCreateStringStream(lString8("crengine.font.size=49\ncrengine.page.margin.right=0\n"));
        CHECK(saved->loadFromStream(user.get()));
        pbdevice::applyDefaults(saved, profile);
        CHECK(saved->getIntDef(PROP_FONT_SIZE) == 49 && saved->getIntDef(PROP_PAGE_MARGIN_RIGHT) == 0);
        LVStreamRef book = LVCreateStringStream(lString8("crengine.font.size=32\n"));
        CHECK(saved->loadFromStream(book.get()));
        CHECK(saved->getIntDef(PROP_FONT_SIZE) == 32);
        printf("PASS: shared defaults, saved preferences, portrait/landscape rendering and firmware admission for %s\n", profile.model);
    }
    ShutdownFontManager();
    return 0;
}
