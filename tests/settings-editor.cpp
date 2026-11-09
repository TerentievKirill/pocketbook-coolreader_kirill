#include "../cr3gui/src/settings.h"
#include "lvdocviewprops.h"
#include "lvrend.h"
#include "testutil.h"
#include <inkview.h>
#include <cstdio>
#include <string>
#include <vector>
#undef CHECK
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#condition); std::exit(1); } } while (0)

lString32 pbSkinFileName, openedCacheFile, currentCacheDir, crCurrentLang;
bool forcePartialBwUpdates=false, useDeveloperFeatures=false;
bool isGSensorSupported() { return true; }
bool canUseNewTouchToc() { return true; }
bool isFrontLightSupported() { return true; }
bool isTaskManagerSupported() { return true; }
void OTA_scheduleUpdate(const char *) {}
void OTA_showVersion() {}
const char *getCommandName(int, int) { return "Action"; }
const char *getKeyName(int, int) { return "Key"; }

class TouchScreen : public CRGUIScreenBase {
public:
    TouchScreen() : CRGUIScreenBase(1072,1448,false) {
        _canvas=LVRef<LVDrawBuf>(createCanvas(1072,1448));
    }
    LVDrawBuf *createCanvas(int w,int h) { return new LVGrayDrawBuf(w,h,8); }
    void update(const lvRect &,bool) {}
    bool isTouchSupported() { return true; }
};
class Manager : public CRGUIWindowManager {
public:
    int applied=0;
    Manager(CRGUIScreen *screen) : CRGUIWindowManager(screen) {}
    void postCommand(int command,int param=0) {
        if (command==9000) ++applied;
        else CRGUIWindowManager::postCommand(command,param);
    }
};
static void preview(TouchScreen &screen,const std::string &path) {
    LVRef<LVDrawBuf> canvas=screen.getCanvas();
    FILE *f=fopen(path.c_str(),"wb");
    if (!f) { std::perror(path.c_str()); std::exit(1); }
    fprintf(f,"P5\n%d %d\n255\n",canvas->GetWidth(),canvas->GetHeight());
    for (int y=0;y<canvas->GetHeight();++y)
        for (int x=0;x<canvas->GetWidth();++x) fputc(canvas->GetPixel(x,y),f);
    fclose(f);
}
static CRSettingsMenu *makeMenu(Manager &wm,CRPropRef props) {
    lvRect rc=wm.getScreen()->getRect();
    CRGUIAcceleratorTableRef acc(new CRGUIAcceleratorTable());
    CRSettingsMenu *menu=new CRSettingsMenu(&wm,props,9000,LVFontRef(),acc,rc,lString32("This book"));
    wm.activateWindow(menu);
    return menu;
}
static CRMenu *child(CRMenu *parent,int index) {
    return static_cast<CRMenu *>(parent->getItems()[index]);
}
static int check(const char *skin,const char *out) {
    TouchScreen screen; Manager wm(&screen);
    CHECK(wm.loadSkin(lString32(skin)));
    CRPropRef original=LVCreatePropsContainer();
    original->setString(PROP_FONT_FACE,"DejaVu Sans");
    original->setInt(PROP_FONT_SIZE,42);
    original->setString(PROP_FONT_WEIGHT_EMBOLDEN,"0");
    original->setInt(PROP_EMBEDDED_FONTS,1);
    original->setInt(PROP_INTERLINE_SPACE,100);
    CRPropRef staged=LVClonePropsContainer(original);
    CRSettingsMenu *root=makeMenu(wm,staged);
    CHECK(root->isSettingsEditor() && root->getItems().length()==6);
    CHECK(root->getItems()[0]->getLabel()=="Font & text");
    CHECK(root->getItems()[5]->getLabel()=="About & updates");
    CHECK(child(root,5)->getItems().length()==3);
    CHECK(!root->settingsChanged());
    for (int rotated=0;rotated<2;++rotated) {
        wm.reconfigure(rotated?1448:1072,rotated?1072:1448,rotated?CR_ROTATE_ANGLE_90:CR_ROTATE_ANGLE_0);
        root->reconfigure(CRGUI_CONFIGURE_FLAG_SCREEN_ORIENTATION);
        wm.update(true,true);
        CHECK(root->getPageCount()==1);
        lvRect header,client,footer;
        CHECK(root->getTitleRect(header) && root->getClientRect(client) && root->getScrollRect(footer));
        CHECK(header.bottom==client.top && client.bottom==footer.top);
        preview(screen,std::string(out)+(rotated?"/grouped-landscape.pgm":"/grouped-portrait.pgm"));
        CRMenu *fonts=child(root,0);
        CHECK(fonts->getItems().length()==6);
        wm.activateWindow(fonts); wm.update(true,true);
        CHECK(fonts->getPageCount()==1);
        CHECK(fonts->getRect().width()==screen.getWidth() && fonts->getRect().height()==screen.getHeight());
        preview(screen,std::string(out)+(rotated?"/font-landscape.pgm":"/font-portrait.pgm"));
        // Exercise real button hit targets through the event queue.
        lvRect rows; CHECK(fonts->getClientRect(rows));
        int rowHeight=fonts->getItemHeight();
        int x=rows.right-50, y=rows.top+rowHeight+rowHeight*3/4;
        fonts->onTouchEvent(x,y,CRTOUCH_DOWN);
        fonts->onTouchEvent(x,y,CRTOUCH_UP);
        wm.processPostedEvents();
        CHECK(staged->getIntDef(PROP_FONT_SIZE)==43);
        fonts->onCommand(DCMD_SETTINGS_STEP,2);
        wm.update(false,true);
        // Direct controls change the staged copy, and their bounds are rebuilt.
        CHECK(fonts->onCommand(DCMD_SETTINGS_STEP,1*2+1));
        CHECK(staged->getIntDef(PROP_FONT_SIZE)==43);
        wm.update(false,false);
        CHECK(fonts->onCommand(DCMD_SETTINGS_STEP,1*2));
        CHECK(staged->getIntDef(PROP_FONT_SIZE)==42);
        CHECK(fonts->onCommand(DCMD_SETTINGS_CHOICE,3*2+1));
        CHECK(staged->getIntDef(PROP_EMBEDDED_FONTS)==0);
        CHECK(original->getIntDef(PROP_EMBEDDED_FONTS)==1);
        CHECK(root->settingsChanged());
        fonts->onCommand(DCMD_SETTINGS_CHOICE,3*2);
        CHECK(!root->settingsChanged());
        staged->setInt(PROP_FONT_SIZE,200);
        fonts->onCommand(DCMD_SETTINGS_STEP,2);
        CHECK(staged->getIntDef(PROP_FONT_SIZE)==195);
        staged->setInt(PROP_FONT_SIZE,16);
        fonts->onCommand(DCMD_SETTINGS_STEP,2);
        CHECK(staged->getIntDef(PROP_FONT_SIZE)==16);
        staged->setInt(PROP_FONT_SIZE,300);
        fonts->onCommand(DCMD_SETTINGS_STEP,3);
        CHECK(staged->getIntDef(PROP_FONT_SIZE)==300);
        staged->setInt(PROP_FONT_SIZE,42);
        fonts->requestFullRedraw(); wm.update(false,true);
        std::vector<lUInt32> sample;
        LVRef<LVDrawBuf> canvas=screen.getCanvas();
        int sampleTop=rows.top+(rowHeight+1)*5;
        for(int y=sampleTop;y<rows.bottom;++y)
            for(int x=rows.left;x<rows.right;++x) sample.push_back(canvas->GetPixel(x,y));
        int baseWeight=LVRendGetBaseFontWeight();
        // Font weight is an explicit picker even with a short choice list.
        CHECK(fonts->onCommand(MCMD_SELECT_3));
        CHECK(wm.getTopVisibleWindow()==child(fonts,2));
        CRMenu *weight=child(fonts,2);
        CHECK(weight->onCommand(MCMD_SELECT_6));
        CHECK(staged->getStringDef(PROP_FONT_WEIGHT_EMBOLDEN)=="1.25");
        CHECK(wm.getTopVisibleWindow()==fonts);
        CHECK(fonts->getItems()[5]->isSettingsPreview());
        wm.update(false,true);
        size_t pixel=0; bool different=false;
        for(int y=sampleTop;y<rows.bottom;++y)
            for(int x=rows.left;x<rows.right;++x)
                if(canvas->GetPixel(x,y)!=sample[pixel++]) different=true;
        CHECK(different && LVRendGetBaseFontWeight()==baseWeight);
        staged->setString(PROP_FONT_WEIGHT_EMBOLDEN,"0");
        // Back returns to the group list without applying anything.
        CHECK(fonts->onCommand(DCMD_BUTTON_PRESSED,BTN_CLOSE));
        CHECK(wm.getTopVisibleWindow()==root && wm.applied==0);
    }
    wm.update(true,true);
    lvRect header; CHECK(root->getTitleRect(header));
    root->onTouchEvent(header.left+50,header.top+header.height()/2,CRTOUCH_DOWN);
    root->onTouchEvent(header.left+50,header.top+header.height()/2,CRTOUCH_UP);
    wm.processPostedEvents();
    CHECK(wm.applied==0 && wm.getWindowCount()==0);
    root=makeMenu(wm,LVClonePropsContainer(original));
    CRMenu *fonts=child(root,0);wm.activateWindow(fonts);
    fonts->onCommand(DCMD_SETTINGS_STEP,3);
    wm.update(true,true); CHECK(fonts->getTitleRect(header));
    fonts->onTouchEvent(header.right-50,header.top+header.height()/2,CRTOUCH_DOWN);
    fonts->onTouchEvent(header.right-50,header.top+header.height()/2,CRTOUCH_UP);
    wm.processPostedEvents();
    CHECK(wm.applied==1 && wm.getWindowCount()==0);
    puts("PASS: actual grouped settings, portrait/landscape, staged values, pickers, Back, Cancel and Done");
    return 0;
}
int main(int argc,char **argv) {
    CHECK(argc==5);
    InitFontManager(lString8()); crCurrentLang=lString32("en");
    CHECK(fontMan->RegisterFont(lString8(argv[1])));
    CHECK(fontMan->RegisterFont(lString8(argv[2])));
    int result=check(argv[3],argv[4]);
    ShutdownFontManager(); return result;
}
