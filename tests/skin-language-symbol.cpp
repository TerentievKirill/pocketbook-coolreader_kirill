#include "crskin.h"
#include "lvfntman.h"
#include "crgui.h"
#include "testutil.h"
#include <cstdio>
#include <string>
#include <vector>

// This is the application's language string. Firmware exports currentLang()
// as a function, so skin code must never import that symbol as a string.
lString32 crCurrentLang;
lString32 openedCacheFile;
lString32 currentCacheDir;
lString32 pbSkinFileName;

class TouchScreen : public CRGUIScreenBase {
public:
    TouchScreen() : CRGUIScreenBase(1072, 1448, false) {
        _canvas = LVRef<LVDrawBuf>(createCanvas(1072, 1448));
    }
    LVDrawBuf *createCanvas(int width, int height) { return new LVGrayDrawBuf(width, height, 8); }
    void update(const lvRect &, bool) {}
    bool isTouchSupported() { return true; }
};

class SettingsMenu : public CRMenu {
public:
    SettingsMenu(CRGUIWindowManager *wm, int itemCount = 30) : CRMenu(wm, NULL, 1, lString32("Settings"),
            LVImageSourceRef(), LVFontRef(), LVFontRef()) {
        setFullscreen(true);
        setSkinName(lString32("#settings"));
        for (int i = 0; i < itemCount; ++i)
            addItem(new CRMenuItem(this, i + 10, lString32("Reading preference"),
                                  LVImageSourceRef(), LVFontRef()));
    }
    bool backHit(int x, int y) {
        lvPoint point(x, y);
        for (int i = 0; i < _controls.length(); ++i)
            if (dynamic_cast<CRButtonControl *>(_controls[i]) && _controls[i]->hitTest(point))
                return true;
        return false;
    }
    CRGUIControl *pagingAt(int x, int y) {
        lvPoint point(x, y);
        for (int i = 0; i < _controls.length(); ++i)
            if (dynamic_cast<CRScrollButtonControl *>(_controls[i]) && _controls[i]->hitTest(point))
                return _controls[i];
        return NULL;
    }
    bool pressBack(bool pressed) {
        lvPoint point(1, 1);
        for (int i = 0; i < _controls.length(); ++i)
            if (dynamic_cast<CRButtonControl *>(_controls[i]) && _controls[i]->hitTest(point))
                return _controls[i]->setSelected(pressed);
        return false;
    }
};

static int checkSettingsHeader(const char *skinFile, const char *previewDirectory) {
    TouchScreen screen;
    CRGUIWindowManager wm(&screen);
    CHECK(wm.loadSkin(lString32(skinFile)));
    SettingsMenu menu(&wm);
    for (int rotated = 0; rotated < 2; ++rotated) {
        wm.reconfigure(rotated ? 1448 : 1072, rotated ? 1072 : 1448,
                       rotated ? CR_ROTATE_ANGLE_90 : CR_ROTATE_ANGLE_0);
        menu.reconfigure(CRGUI_CONFIGURE_FLAG_SCREEN_ORIENTATION);
        menu.setCurPage(0);
        lvRect title, client, footer;
        CHECK(menu.getTitleRect(title) && menu.getClientRect(client) && menu.getScrollRect(footer));
        CHECK(footer.top == client.bottom && footer.bottom == menu.getRect().bottom);
        CHECK(footer.height() == title.height());
        CHECK(abs(title.height() - menu.getItemHeight()) <= 1);
        CHECK(title.height() > 75 && client.top == title.bottom);
        menu.flush();
        CHECK(menu.backHit(title.left + 1, title.top + 1));
        int backWidth = title.height() * 3;
        CHECK(menu.backHit(title.left + backWidth - 1, title.top + 1));
        CHECK(menu.backHit(title.left + backWidth - 1, title.bottom - 1));
        CHECK(!menu.backHit(title.left + backWidth, title.top + 1));
        CHECK(!menu.backHit(title.left + 1, title.bottom));
        LVRef<LVDrawBuf> canvas = screen.getCanvas();
        std::vector<lUInt32> normal;
        for (int y = title.top; y < title.bottom; ++y)
            for (int x = title.left; x < title.left + backWidth; ++x)
                normal.push_back(canvas->GetPixel(x, y));
        CHECK(menu.pressBack(true));
        CHECK(canvas->GetPixel(title.left + 1, title.top + 1) != normal[backWidth + 1]);
        CHECK(menu.pressBack(false));
        unsigned pixel = 0;
        for (int y = title.top; y < title.bottom; ++y)
            for (int x = title.left; x < title.left + backWidth; ++x)
                CHECK(canvas->GetPixel(x, y) == normal[pixel++]);
        // All four sides of Back have a visible, inset border.
        int inset = title.height() / 12;
        CHECK(canvas->GetPixel(title.left + backWidth / 2, title.top + inset) == 0);
        CHECK(canvas->GetPixel(title.left + backWidth / 2, title.bottom - inset - 1) == 0);
        CHECK(canvas->GetPixel(title.left + inset, title.top + title.height() / 2) == 0);
        CHECK(canvas->GetPixel(title.left + backWidth - inset - 1, title.top + title.height() / 2) == 0);

        CRGUIControl *previous = menu.pagingAt(footer.left + 1, footer.top + 1);
        CRGUIControl *next = menu.pagingAt(footer.right - 1, footer.bottom - 1);
        CHECK(previous && next && previous != next);
        CHECK(!previous->isEnabled() && next->isEnabled());
        CHECK(!previous->setSelected(true));
        // Verify full button-sized hit regions and the inert counter gap.
        for (int y : {footer.top, footer.bottom - 1}) {
            CHECK(menu.pagingAt(footer.left, y) == previous);
            CHECK(menu.pagingAt(footer.left + backWidth - 1, y) == previous);
            CHECK(menu.pagingAt(footer.right - backWidth, y) == next);
            CHECK(menu.pagingAt(footer.right - 1, y) == next);
        }
        CHECK(!menu.pagingAt(footer.left + backWidth, footer.top));
        CHECK(!menu.pagingAt(footer.right - backWidth - 1, footer.bottom - 1));
        CHECK(!menu.pagingAt(footer.left, footer.top - 1));
        CHECK(!menu.pagingAt(footer.right - 1, footer.bottom));
        CHECK(next->setSelected(true));
        CHECK(canvas->GetPixel(footer.right - 2, footer.top + 1) == 0);
        CHECK(next->setSelected(false));
        CHECK(canvas->GetPixel(footer.right - 2, footer.top + 1) == 255);

        // The existing touch commands page forwards and backwards, and
        // disabled states remain correct after rebuilding the controls.
        CHECK(menu.onCommand(DCMD_BUTTON_PRESSED, BTN_SCROLL_DOWN));
        CHECK(menu.getCurPage() == 1);
        menu.flush();
        CHECK(menu.pagingAt(footer.left + 1, footer.top + 1)->isEnabled());
        CHECK(menu.pagingAt(footer.right - 1, footer.top + 1)->isEnabled());
        CHECK(menu.onCommand(DCMD_BUTTON_PRESSED, BTN_SCROLL_UP));
        CHECK(menu.getCurPage() == 0);
        menu.setCurPage(menu.getPageCount() - 1);
        menu.flush();
        CHECK(menu.pagingAt(footer.left + 1, footer.top + 1)->isEnabled());
        CHECK(!menu.pagingAt(footer.right - 1, footer.top + 1)->isEnabled());
        menu.setCurPage(0);
        menu.flush();

        std::string path = std::string(previewDirectory) +
                           (rotated ? "/settings-landscape.pgm" : "/settings-portrait.pgm");
        FILE *preview = fopen(path.c_str(), "wb");
        CHECK(preview != NULL);
        lvRect full = menu.getRect();
        fprintf(preview, "P5\n%d %d\n255\n", full.width(), full.height());
        for (int y = full.top; y < full.bottom; ++y)
            for (int x = full.left; x < full.right; ++x)
                fputc(canvas->GetPixel(x, y), preview);
        CHECK(fclose(preview) == 0);
        printf("PASS: %s settings header %d px, rows %d px; outlined Back/Previous/Next targets %d x %d px\n",
               rotated ? "landscape" : "portrait", title.height(), menu.getItemHeight(),
               backWidth, title.height());
    }
    SettingsMenu singlePage(&wm, 3);
    singlePage.reconfigure(CRGUI_CONFIGURE_FLAG_SCREEN_ORIENTATION);
    singlePage.flush();
    lvRect footer;
    CHECK(singlePage.getScrollRect(footer));
    CHECK(!singlePage.pagingAt(footer.left + 1, footer.top + 1)->isEnabled());
    CHECK(!singlePage.pagingAt(footer.right - 1, footer.top + 1)->isEnabled());
    menu.setSkinName(lString32("#main"));
    menu.reconfigure(CRGUI_CONFIGURE_FLAG_SCREEN_ORIENTATION);
    lvRect title;
    CHECK(menu.getTitleRect(title) && title.height() == 75);
    // Native Wi-Fi/progress dialogs can overwrite the menu client framebuffer.
    // A full redraw must repaint every clean row, not just the top/bottom bars.
    SettingsMenu *restored=new SettingsMenu(&wm);
    restored->reconfigure(CRGUI_CONFIGURE_FLAG_SCREEN_ORIENTATION);
    wm.activateWindow(restored);restored->setCurPage(1);
    restored->setCurItem(restored->getTopItem()+1);
    wm.update(true,true);
    int page=restored->getCurPage(), selection=restored->getSelectedItemIndex();
    LVRef<LVDrawBuf> buffer=screen.getCanvas();std::vector<lUInt32> expected;
    for(int y=0;y<buffer->GetHeight();++y)for(int x=0;x<buffer->GetWidth();++x)expected.push_back(buffer->GetPixel(x,y));
    lvRect client;CHECK(restored->getClientRect(client));
    for(int y=client.top+5;y<client.bottom-5;y+=18)
        buffer->FillRect(client.left+10,y,client.right-10,y+3,0x000000);
    wm.update(true,true);
    size_t at=0;
    for(int y=0;y<buffer->GetHeight();++y)for(int x=0;x<buffer->GetWidth();++x)CHECK(buffer->GetPixel(x,y)==expected[at++]);
    CHECK(restored->getCurPage()==page && restored->getSelectedItemIndex()==selection);
    puts("PASS: full menu repaint restores clean client rows after native OTA/network UI, preserving page and selection");

    return 0;
}

int main(int argc, char **argv) {
    CHECK(argc >= 5);
    InitFontManager(lString8());
    crCurrentLang = lString32("en");
    {
        CRRectSkin skin;
        skin.setFontFace(lString32("DejaVu Sans"));
        if (skin.getFontFace() != "DejaVu Sans") return 1;
        crCurrentLang = lString32("zh");
        skin.setFontFace(lString32("DejaVu Sans"));
        if (skin.getFontFace() != "AR PL New Sung") return 2;
    }
    crCurrentLang = lString32("en");
    CHECK(fontMan->RegisterFont(lString8(argv[1])));
    CHECK(fontMan->RegisterFont(lString8(argv[2])));
    int result = checkSettingsHeader(argv[3], argv[4]);
    ShutdownFontManager();
    CHECK(result == 0);
    puts("PASS: skin reads the application language string, including Chinese fallback");
    return 0;
}
