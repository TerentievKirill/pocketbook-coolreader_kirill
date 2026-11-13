#include "../cr3gui/src/settings.h"
#include "../cr3gui/src/reader_menu.h"
#include "lvdocviewprops.h"
#include "cri18n.h"
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
CRMenu *pbCreateDictionaryDownloadsMenu(CRMenu *parent,int id) {
    return new CRMenu(parent->getWindowManager(),parent,id,"Dictionaries","",LVFontRef(),LVFontRef());
}
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
    int applied=0, lastCommand=0, lastParam=0;
    Manager(CRGUIScreen *screen) : CRGUIWindowManager(screen) {}
    void postCommand(int command,int param=0) {
        if (command==MCMD_SETTINGS || command==DCMD_PAGEDOWN) { lastCommand=command; lastParam=param; }
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
    CHECK(root->isSettingsEditor() && !root->isActionMenu() && root->getItems().length()==7);
    CHECK(root->getItems()[0]->getLabel()=="Font & text");
    CHECK(root->getItems()[5]->getLabel()=="Dictionaries");
    CHECK(root->getItems()[6]->getLabel()=="About & updates");
    CHECK(child(root,6)->getItems().length()==3);
    CHECK(!root->settingsChanged());
    for (int rotated=0;rotated<2;++rotated) {
        wm.reconfigure(rotated?1448:1072,rotated?1072:1448,rotated?CR_ROTATE_ANGLE_90:CR_ROTATE_ANGLE_0);
        root->reconfigure(CRGUI_CONFIGURE_FLAG_SCREEN_ORIENTATION);
        wm.update(true,true);
        CHECK(root->getPageCount()<=2);
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
    // Long-press MAIN_MENU uses the same layout, with immediate commands and
    // no staged settings, while retaining configured tools and parameters.
    CRGUIAcceleratorTableRef shortcuts(new CRGUIAcceleratorTable());
    CRGUIAcceleratorTableRef configured(new CRGUIAcceleratorTable());
    const int legacy[]={PB_CMD_MAIN_MENU,MCMD_ABOUT,MCMD_RECENT_BOOK_LIST,MCMD_CITES_LIST,MCMD_CITE,
        DCMD_SAVE_HISTORY,DCMD_SAVE_TO_CACHE,MCMD_HELP,MCMD_HELP_KEYS};
    for (int i=0;i<9;++i) configured->add(i+1,0,legacy[i],0);
    configured->add(10,0,MCMD_BOOKMARK_LIST,0);
    configured->add(11,0,DCMD_PAGEDOWN,10);
    for (int rotated=0;rotated<2;++rotated) {
        wm.reconfigure(rotated?1448:1072,rotated?1072:1448,rotated?CR_ROTATE_ANGLE_90:CR_ROTATE_ANGLE_0);
        CRMenu *reader=createReaderMenu(&wm,shortcuts,configured,true);
        wm.activateWindow(reader); wm.update(true,true);
        CHECK(reader->isActionMenu() && !reader->settingsChanged());
        CHECK(reader->getItems().length()==6 && reader->getPageCount()==1);
        CHECK(reader->getItems()[0]->getId()==MCMD_SETTINGS && reader->getItems()[0]->getLabel()=="Settings");
        CHECK(reader->getItems()[2]->getId()==MCMD_BOOKMARK_LIST_GO_MODE);
        CHECK(configured->length()==11);
        preview(screen,std::string(out)+(rotated?"/reader-menu-landscape.pgm":"/reader-menu-portrait.pgm"));
        lvRect rows; CHECK(reader->getClientRect(rows));
        reader->onTouchEvent(rows.left+70,rows.top+reader->getItemHeight()/2,CRTOUCH_DOWN);
        reader->onTouchEvent(rows.left+70,rows.top+reader->getItemHeight()/2,CRTOUCH_UP);
        wm.processPostedEvents();
        CHECK(wm.lastCommand==MCMD_SETTINGS && wm.lastParam==0 && wm.applied==1 && wm.getWindowCount()==0);
        CHECK(original->getIntDef(PROP_FONT_SIZE)==42);
        reader=createReaderMenu(&wm,shortcuts,configured,true);
        wm.activateWindow(reader); wm.update(true,true);
        CHECK(reader->onCommand(MCMD_SELECT_6));
        CRMenu *more=static_cast<CRMenu *>(wm.getTopVisibleWindow());
        CHECK(more!=reader && more->getLabel()=="More tools" && more->isActionMenu());
        CHECK(more->findItem(DCMD_SAVE_HISTORY) && more->findItem(DCMD_SAVE_TO_CACHE) && more->findItem(MCMD_HELP_KEYS));
        CHECK(more->findItem(MCMD_RECENT_BOOK_LIST));
        CHECK(more->findItem(MCMD_BOOKMARK_LIST)->getLabel()=="Save a bookmark");
        wm.update(true,true);
        preview(screen,std::string(out)+(rotated?"/reader-tools-landscape.pgm":"/reader-tools-portrait.pgm"));
        CRMenuItem *custom=more->findItem(DCMD_PAGEDOWN); CHECK(custom);
        CHECK(custom->onSelect()==1);
        CHECK(wm.lastCommand==DCMD_PAGEDOWN && wm.lastParam==10 && wm.getWindowCount()==0);
        wm.processPostedEvents();
        reader=createReaderMenu(&wm,shortcuts,configured,false);
        wm.activateWindow(reader); wm.update(true,true);
        CHECK(reader->getTitleRect(header)); int last=wm.lastCommand;
        reader->onTouchEvent(header.right-50,header.top+header.height()/2,CRTOUCH_DOWN);
        reader->onTouchEvent(header.right-50,header.top+header.height()/2,CRTOUCH_UP);
        wm.processPostedEvents();
        CHECK(wm.getWindowCount()==0 && wm.lastCommand==last && wm.applied==1);
    }
    puts("PASS: actual grouped settings, portrait/landscape, staged values, pickers, Back, Cancel and Done; long-press reader menu, Settings touch route, retained tools, custom parameters and Read");
    return 0;
}
// Test the shipped ru.mo with the same menu path used on PB634.
static int checkRussianSettings(const char *skin,const char *ruMo) {
    CRMoFileTranslator *translator = new CRMoFileTranslator();
    CHECK(translator->openMoFile(Utf8ToUnicode(ruMo)));
    CRI18NTranslator::setTranslator(translator);
    crCurrentLang=U"ru";
    CHECK(lString32(_("Settings")) == Utf8ToUnicode("Настройки"));
    CHECK(CRI18NTranslator::translate(NULL)[0] == '\0');
    // Untranslated labels must never read past the end of a sorted .mo catalog.
    CHECK(lString8(CRI18NTranslator::translate("zzzzzz-untranslated-menu-label")) == "zzzzzz-untranslated-menu-label");
    CHECK(lString8(CRI18NTranslator::translate("")).empty());
    fprintf(stderr, "Russian menu regression: translator loaded and null labels safe\n"); fflush(stderr);
    fprintf(stderr,"RU 1: create screen\n"); fflush(stderr);
    {
    TouchScreen screen;
    Manager wm(&screen);
    fprintf(stderr,"RU 2: load skin\n"); fflush(stderr);
    CHECK(wm.loadSkin(lString32(skin)));
    fprintf(stderr,"RU 3: skin loaded\n"); fflush(stderr);
    CRPropRef props=LVCreatePropsContainer();
    props->setString(PROP_FONT_FACE,"DejaVu Sans");
    props->setInt(PROP_FONT_SIZE,42);
    fprintf(stderr,"RU 4: build localized settings\n"); fflush(stderr);
    CRSettingsMenu *settings=makeMenu(wm,props);
    fprintf(stderr,"RU 5: settings built\n"); fflush(stderr);
    CHECK(settings->getLabel()==Utf8ToUnicode("Настройки"));
    CHECK(settings->getItems().length()==7);
    fprintf(stderr,"RU rendering menu\n"); fflush(stderr);
    wm.update(true,true);
    fprintf(stderr,"RU menu rendered\n"); fflush(stderr);
    CRMenu *fontMenu=child(settings,0);
    fprintf(stderr,"RU activate fonts\n"); fflush(stderr);
    wm.activateWindow(fontMenu);
    fprintf(stderr,"RU rendering menu\n"); fflush(stderr);
    wm.update(true,true);
    fprintf(stderr,"RU menu rendered\n"); fflush(stderr);
    CHECK(fontMenu->onCommand(DCMD_BUTTON_PRESSED, BTN_CLOSE));
    CHECK(wm.getTopVisibleWindow()==settings);
    CRMenu *advancedMenu=child(settings,3);
    fprintf(stderr,"RU activate advanced\n"); fflush(stderr);
    wm.activateWindow(advancedMenu);
    fprintf(stderr,"RU rendering menu\n"); fflush(stderr);
    wm.update(true,true);
    fprintf(stderr,"RU menu rendered\n"); fflush(stderr);
    CHECK(advancedMenu->onCommand(DCMD_BUTTON_PRESSED, BTN_CLOSE));
    CHECK(wm.getTopVisibleWindow()==settings);
    fprintf(stderr,"RU rotate\n"); fflush(stderr);
    wm.reconfigure(1448,1072,CR_ROTATE_ANGLE_90);
    settings->reconfigure(CRGUI_CONFIGURE_FLAG_SCREEN_ORIENTATION);
    fprintf(stderr,"RU rendering menu\n"); fflush(stderr);
    wm.update(true,true);
    fprintf(stderr,"RU menu rendered\n"); fflush(stderr);
    }
    fprintf(stderr,"RU 6: UI manager destroyed\n"); fflush(stderr);
    CRI18NTranslator::setTranslator(NULL);
    fprintf(stderr,"RU 7: translator cleared\n"); fflush(stderr);
    puts("PASS: Russian ru.mo loads; localized Settings renders groups and rotates");
    crCurrentLang=U"en";
    return 0;
}
int main(int argc,char **argv) {
    CHECK(argc==6);
    InitFontManager(lString8()); crCurrentLang=lString32("en");
    CHECK(fontMan->RegisterFont(lString8(argv[1])));
    CHECK(fontMan->RegisterFont(lString8(argv[2])));
    int result=check(argv[3],argv[4]);
    if (result==0) result=checkRussianSettings(argv[3],argv[5]);
    ShutdownFontManager(); return result;
}
