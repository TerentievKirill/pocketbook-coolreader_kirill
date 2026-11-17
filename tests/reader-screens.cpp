#include "../cr3gui/src/mainwnd.h"
#include "../cr3gui/src/bmkdlg.h"
#include "../cr3gui/src/recentdlg.h"
#include "cri18n.h"
#include "testutil.h"
#include <inkview.h>
#include <cstdio>
#include <cstdlib>
#include <string>
#undef CHECK
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); exit(1); } } while (0)
static const char *model = "PB629";
extern "C" char *GetDeviceModel() { return const_cast<char *>(model); }
extern "C" char *GetSoftwareVersion() { return const_cast<char *>("U629.6.11.2020"); }
class Screen : public CRGUIScreenBase {
public:
    Screen(int w,int h) : CRGUIScreenBase(w,h,false) { _canvas=LVRef<LVDrawBuf>(createCanvas(w,h)); }
    LVDrawBuf *createCanvas(int w,int h) { return new LVGrayDrawBuf(w,h,8); }
    void update(const lvRect &,bool) {}
    bool isTouchSupported() { return true; }
};
static void preview(Screen &screen,const std::string &path) {
    auto b=screen.getCanvas(); FILE *f=fopen(path.c_str(),"wb");CHECK(f);
    fprintf(f,"P5\n%d %d\n255\n",b->GetWidth(),b->GetHeight());
    for(int y=0;y<b->GetHeight();++y) for(int x=0;x<b->GetWidth();++x) fputc(b->GetPixel(x,y),f);
    fclose(f);
}
int main(int argc,char **argv) {
    CHECK(argc==6);InitFontManager(lString8());
    CHECK(fontMan->RegisterFont(lString8(argv[1])));CHECK(fontMan->RegisterFont(lString8(argv[2])));
    auto *russian=new CRMoFileTranslator();CHECK(russian->openMoFile(Utf8ToUnicode(lString8(argv[4]))));
    CRI18NTranslator::setTranslator(russian);
    for(int size=0;size<2;++size) {
        int w=size?1072:758,h=size?1448:1024;model=size?"PB632":"PB629";
#ifdef CR3_SHARED_PACKAGE
        const int documentFontSize = size ? 36 : 25;
#else
        const int documentFontSize = 36;
#endif
        Screen screen(w,h);CRGUIWindowManager wm(&screen);CHECK(wm.loadSkin(lString32(argv[3])));
        const int keys[]={0,0,0,0};
        for (const char *name : {"menu","bookmarks","browse","dialog"}) wm.getAccTables().add(name,keys);
        auto *reader=new V3DocViewWin(&wm,lString32());reader->setRect(screen.getRect());
        reader->getDocView()->setCallback(NULL);
        lString8 xml("<FictionBook><description><title-info><book-title>Long title &amp; &lt;metadata&gt; Привет</book-title><author><first-name>Test</first-name><last-name>Author</last-name></author></title-info></description><body><section>");
        for(int i=0;i<70;++i) xml << "<p>Reader screen test. Книга для проверки оформления.</p>";
        xml << "</section></body></FictionBook>";
        CHECK(reader->getDocView()->LoadDocument(LVCreateStringStream(xml),U"reader-screens.fb2"));
        reader->getDocView()->getDocProps()->setString(DOC_PROP_FILE_PATH,lString32("/books/Very long folder & metadata/<literal>/another folder/"));
        wm.activateWindow(reader);
        reader->showAboutDialog();
        auto *info=dynamic_cast<CRViewDialog *>(wm.getTopVisibleWindow());CHECK(info && info!=reader);
        CHECK(info->getSkinName()==U"#settings");
        CHECK(info->getDocView()->getFontSize()==documentFontSize);
        lString32 content=info->getDocView()->getDocument()->getRootNode()->getText();
        CHECK(content.pos(lString32("Long title & <metadata> Привет"))>=0);
        CHECK(content.pos(lString32("/books/Very long folder & metadata/<literal>/another folder/"))>=0);
        for(int landscape=0;landscape<2;++landscape) {
            wm.reconfigure(landscape?h:w,landscape?w:h,landscape?CR_ROTATE_ANGLE_90:CR_ROTATE_ANGLE_0);
            wm.update(true,true);
            lvRect title,client,status;CHECK(info->getTitleRect(title));CHECK(info->getClientRect(client));CHECK(info->getStatusRect(status));
            CHECK(title.bottom<=client.top && client.bottom<=status.top && status.bottom==screen.getRect().bottom);
            CHECK(title.height()>70 && status.height()>40 && client.height()>200);
            preview(screen,std::string(argv[5])+"/book-info-"+(size?"632":"629")+(landscape?"-landscape.pgm":"-portrait.pgm"));
        }
        CHECK(info->getDocView()->getPageCount()>1);
        int page=info->getDocView()->getCurPage();
        lvRect footer;info->getStatusRect(footer);
        info->onTouchEvent(footer.right-25,(footer.top+footer.bottom)/2,CRTOUCH_DOWN);
        info->onTouchEvent(footer.right-25,(footer.top+footer.bottom)/2,CRTOUCH_UP);wm.processPostedEvents();
        CHECK(info->getDocView()->getCurPage()>page);
        lvRect header;info->getTitleRect(header);
        info->onTouchEvent(header.right-25,(header.top+header.bottom)/2,CRTOUCH_DOWN);
        info->onTouchEvent(header.right-25,(header.top+header.bottom)/2,CRTOUCH_UP);wm.processPostedEvents();CHECK(wm.getTopVisibleWindow()==reader);
        reader->showHelpDialog();
        auto *help=dynamic_cast<CRViewDialog *>(wm.getTopVisibleWindow());CHECK(help && help!=reader && help->getSkinName()==U"#settings");
        CHECK(help->getDocView()->getFontSize()==documentFontSize);wm.update(true,true);
        help->onCommand(DCMD_BUTTON_PRESSED,BTN_CLOSE);CHECK(wm.getTopVisibleWindow()==reader);
        reader->setAccelerators(wm.getAccTables().get("browse"));reader->showKeymapDialog();
        auto *shortcuts=dynamic_cast<CRViewDialog *>(wm.getTopVisibleWindow());CHECK(shortcuts && shortcuts!=reader && shortcuts->getSkinName()==U"#settings");
        wm.update(true,true);shortcuts->onCommand(DCMD_BUTTON_PRESSED,BTN_SETTINGS_DONE);CHECK(wm.getTopVisibleWindow()==reader);
        reader->getDocView()->doCommand(DCMD_BOOKMARK_SAVE_N, 1);
        lvRect rc=screen.getRect();
        auto *bookmarks=new CRBookmarkMenu(&wm,reader->getDocView(),8,rc,true);
        wm.activateWindow(bookmarks);wm.update(true,true);CHECK(bookmarks->isActionMenu() && bookmarks->getSkinName()==U"#settings");
        preview(screen,std::string(argv[5])+"/bookmarks-"+(size?"632":"629")+".pgm");
        bookmarks->onCommand(DCMD_BUTTON_PRESSED,BTN_SETTINGS_DONE);wm.processPostedEvents();CHECK(wm.getTopVisibleWindow()==reader);
        reader->showCitesMenu();
        auto *cites=dynamic_cast<CRMenu *>(wm.getTopVisibleWindow());CHECK(cites && cites->isActionMenu());
        wm.update(true,true);cites->onCommand(DCMD_BUTTON_PRESSED,BTN_SETTINGS_DONE);wm.processPostedEvents();CHECK(wm.getTopVisibleWindow()==reader);
        auto *previous=new CRFileHistRecord();previous->setTitle(lString32(U"Previous book · Предыдущая книга"));previous->setAuthor(lString32("Test Author"));
        reader->getDocView()->getHistory()->getRecords().add(previous);
        auto *recent=new CRRecentBooksMenu(&wm,reader->getDocView(),8,rc);
        wm.activateWindow(recent);wm.update(true,true);CHECK(recent->isActionMenu() && recent->getSkinName()==U"#settings");
        preview(screen,std::string(argv[5])+"/recent-books-"+(size?"632":"629")+".pgm");
        recent->onCommand(DCMD_BUTTON_PRESSED,BTN_CLOSE);wm.processPostedEvents();CHECK(wm.getTopVisibleWindow()==reader);
        reader->showSettingsMenu();
        auto *settings=dynamic_cast<CRMenu *>(wm.getTopVisibleWindow());CHECK(settings && !settings->isActionMenu());
        auto *controls=static_cast<CRMenu *>(settings->getItems()[3]);controls->onSelect();
        CRMenuItem *tap=NULL;
        for(int i=0;i<controls->getItems().length();++i)
            if(controls->getItems()[i]->getLabel()==lString32(_("Tap action"))) tap=controls->getItems()[i];
        CHECK(tap);tap->onSelect();
        auto *zones=dynamic_cast<CRMenu *>(wm.getTopVisibleWindow());CHECK(zones && zones->settingsRoot()==settings && zones->getSkinName()==U"#settings");
        wm.update(true,true);preview(screen,std::string(argv[5])+"/tap-zones-"+(size?"632":"629")+".pgm");
        lvRect cells;zones->getClientRect(cells);
        lvPoint cell(cells.left+cells.width()/6,cells.top+cells.height()/6);
        CHECK(zones->onClientTouch(cell,CRTOUCH_UP));
        auto *actions=dynamic_cast<CRMenu *>(wm.getTopVisibleWindow());CHECK(actions && actions->settingsRoot()==settings && !actions->isActionMenu());
        wm.update(true,true);actions->onCommand(DCMD_BUTTON_PRESSED,BTN_CLOSE);
        zones->onCommand(DCMD_BUTTON_PRESSED,BTN_CLOSE);CHECK(wm.getTopVisibleWindow()==controls);
        settings->onCommand(DCMD_BUTTON_PRESSED,BTN_CLOSE);CHECK(wm.getTopVisibleWindow()==reader);
        wm.closeWindow(reader);
    }
    CRI18NTranslator::setTranslator(NULL);ShutdownFontManager();
    puts("PASS: actual Book info, escaped metadata, Russian text, portrait/landscape, paging, Back/Read, bookmarks and recent-book shared design");
}
