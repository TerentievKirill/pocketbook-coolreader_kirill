#include "crengine.h"
#include "crgui.h"
#include "testutil.h"
#include <string>
#include <fstream>
#include <cstdlib>
#include <sys/stat.h>
#include <unistd.h>
#include <openssl/sha.h>
#define FLASHDIR "/tmp/cr3-dictionary-download-test"
#define _(s) (s)
#define ICON_INFORMATION 1
#define ICON_ERROR 2
#undef CHECK
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);exit(1); } } while(0)
lString32 pbSkinFileName, openedCacheFile, currentCacheDir, crCurrentLang;
static bool online = true, failDownload = false, failStart = false;
static int wifi = 0, progress = 0, dialogs = 0, restores = 0;
static std::string payload, result, url, button;
static void (*timer)() = NULL, (*restoreTimer)() = NULL;
static void (*answer)(int) = NULL, (*cancelDownload)(int) = NULL;
static void dictionaryRestore();
bool pbNetwork(const char *) { ++wifi; return online; }
void pbRestoreUpdateScreen() { ++restores; }
void SetHardTimer(const char *, void (*fn)(), int delay) {
    CHECK(delay > 0); if (fn == dictionaryRestore) restoreTimer = fn; else timer = fn;
}
void ClearTimer(void (*fn)()) { if (fn == timer) timer = NULL; }
void Dialog(int, const char *, const char *text, const char *left, const char *, void (*fn)(int)) {
    result = text; button = left; answer = fn; ++dialogs;
}
void OpenProgressbar(int, const char *, const char *, int, void (*fn)(int)) {
    CHECK(++progress == 1); cancelDownload = fn;
}
void CloseProgressbar() { CHECK(--progress == 0); }
void UpdateProgressbar(const char *, int percent) { CHECK(percent >= 0 && percent <= 100); }
class WebTransfer {
    bool success = true;
    int polls = 0;
public:
    bool start(const std::string &source, const std::string &path, size_t maximum) {
        url = source; success = !failDownload;
        CHECK(maximum >= payload.size());
        std::ofstream f(path, std::ios::binary); f << payload;
        return !failStart;
    }
    bool poll() { return ++polls > 1; }
    bool succeeded() const { return success; }
    const std::string &error() const { static std::string text = "Test network failure"; return text; }
    long long received() const { return payload.size(); }
    void cancel() { success = false; }
};
#include "../cr3gui/src/dictionary_download.h"
#include "../cr3gui/src/dictionary_download_ui.h"
#include "../cr3gui/src/stardict.h"
static void tick() { CHECK(timer); auto fn = timer; timer = NULL; fn(); }
static void writeFile(const std::string &path, const std::string &data) { std::ofstream file(path,std::ios::binary); file << data; CHECK(file.good()); }
static std::string readFile(const std::string &path) { std::ifstream file(path,std::ios::binary); return std::string((std::istreambuf_iterator<char>(file)),{}); }
static std::string hash(const std::string &data) {
    unsigned char digest[32]; SHA256(reinterpret_cast<const unsigned char *>(data.data()),data.size(),digest);
    char text[65]; for (int i=0;i<32;++i) snprintf(text+2*i,3,"%02x",digest[i]); return text;
}
class Screen : public CRGUIScreenBase {
public:
    Screen() : CRGUIScreenBase(1072,1448,false) { _canvas=LVRef<LVDrawBuf>(createCanvas(1072,1448)); }
    LVDrawBuf *createCanvas(int w,int h) { return new LVGrayDrawBuf(w,h,8); }
    void update(const lvRect &,bool) {}
    bool isTouchSupported() { return true; }
};
static int check(const char *font, const char *skin, const char *out) {
    mkdir(FLASHDIR,0755); mkdir(FLASHDIR "/system",0755); mkdir(dictui::directory,0755);
    std::string target = std::string(dictui::directory)+"/fixture.dic";
    unlink(target.c_str()); unlink(dictui::temporary);
    payload = "SDIC" + std::string(200,'x');
    std::string checksum = hash(payload), error;
    cr3dict::Entry entry = {"English → Czech", "fixture.dic", "https://example.org/fixture.dic", checksum.c_str(), "CC BY-SA 4.0", payload.size()};
    CHECK(cr3dict::prepare(dictui::directory,entry,error));
    writeFile(dictui::temporary,payload.substr(0,100));
    CHECK(!cr3dict::install(dictui::temporary,dictui::directory,entry,error)); CHECK(access(target.c_str(),F_OK));
    writeFile(dictui::temporary,"HTML"+payload.substr(4));
    CHECK(!cr3dict::install(dictui::temporary,dictui::directory,entry,error));
    std::string corrupt = payload; corrupt.back()='y'; writeFile(dictui::temporary,corrupt);
    CHECK(!cr3dict::install(dictui::temporary,dictui::directory,entry,error));
    writeFile(dictui::temporary,payload); CHECK(cr3dict::install(dictui::temporary,dictui::directory,entry,error));
    CHECK(readFile(target)==payload && access(dictui::temporary,F_OK));
    writeFile(dictui::temporary,payload); CHECK(!cr3dict::install(dictui::temporary,dictui::directory,entry,error));
    CHECK(!cr3dict::prepare(dictui::directory,entry,error)); CHECK(readFile(target)==payload);
    unlink(target.c_str()); unlink(dictui::temporary);

    Screen screen; CRGUIWindowManager wm(&screen);
    CHECK(wm.loadSkin(lString32(skin)));
    LVFontRef valueFont = fontMan->GetFont(32,400,false,css_ff_sans_serif,lString8("DejaVu Sans"));
    CRMenu *parent=new CRMenu(&wm,NULL,1,lString32("Settings"),LVImageSourceRef(),LVFontRef(),valueFont);
    parent->setSkinName(lString32("#settings"));
    parent->enableSettingsEditor(LVCreatePropsContainer(),lString32("Dictionaries"));
    CRMenu *downloads = pbCreateDictionaryDownloadsMenu(parent,2); parent->addItem(downloads); wm.activateWindow(parent);
    CHECK(downloads->getItems().length()==4 && downloads->getLabel()=="Dictionaries");
    CHECK(downloads->onSelect()==1 && wm.getTopVisibleWindow()==downloads);
    CRMenu *listing=static_cast<CRMenu*>(downloads->getItems()[0]);
    CHECK(listing->getItems().length()==4 && listing->getLabel()=="English dictionaries");
    CHECK(listing->onSelect()==1 && wm.getTopVisibleWindow()==listing);
    for (int landscape=0;landscape<2;++landscape) {
        wm.reconfigure(landscape?1448:1072,landscape?1072:1448,landscape?CR_ROTATE_ANGLE_90:CR_ROTATE_ANGLE_0);
        listing->reconfigure(CRGUI_CONFIGURE_FLAG_SCREEN_ORIENTATION); wm.update(true,true);
        std::string path=std::string(out)+(landscape?"/dictionary-landscape.pgm":"/dictionary-portrait.pgm");
        FILE *f=fopen(path.c_str(),"wb"); CHECK(f);
        LVDrawBuf *canvas=screen.getCanvas().get();
        fprintf(f,"P5\n%d %d\n255\n",canvas->GetWidth(),canvas->GetHeight());
        for(int y=0;y<canvas->GetHeight();++y) for(int x=0;x<canvas->GetWidth();++x) fputc(canvas->GetPixel(x,y),f);
        fclose(f);
    }
    // All languages have original StarDict downloads and optional PocketBook versions.
    for (int i=0;i<4;++i) {
        CRMenu *language=static_cast<CRMenu*>(downloads->getItems()[i]);
        CHECK(language->getItems().length()==(i==2?2:4));
        CRMenu *versions=static_cast<CRMenu*>(language->getItems()[language->getItems().length()-1]);
        CHECK(versions->getLabel()=="PocketBook versions");
        CHECK(versions->getItems().length()==(i==2?1:3));
    }
    CHECK(dictview::name(lString32("ReaderTools en-ru"))==lString32("English → Russian"));
    CHECK(dictview::name(lString32("reader.dict UK (StarDict)"))=="Ukrainian definitions");
    CHECK(dictview::name(lString32("Русский-English FreeDict+WikDict dictionary (ru-en) (StarDict)"))==lString32("Russian → English"));
    CHECK(dictview::name(lString32("My custom dictionary"))=="My custom dictionary");
    CHECK(dictview::entrySummary(lString32("/ ˈmed.sɪn /, / ˈmed.ɪ.sɪn / noun лекарство"))==lString32("noun лекарство"));
    CHECK(dictview::entrySummary(lString32("/dev/null is a device"))==lString32("/dev/null is a device"));
    CHECK(dictview::entrySummary(lString32("/ pronunciation /"))==lString32("/ pronunciation /"));
    const int dimensions[][2]={{1072,1448},{1448,1072},{800,600}};
    for (int d=0;d<3;++d) {
        int width=dimensions[d][0],height=dimensions[d][1];
        LVGrayDrawBuf canvas(width,height,8); canvas.FillRect(0,0,width,height,0xffffff);
        int top=height/2;
        int titleHeight=dictview::titleHeight(lvRect(0,top,width,height));
        lvRect title(0,top,width,top+titleHeight),body(0,top+titleHeight,width,height);
        dictview::Header header(title); dictview::Entries entries(body);
        for (int i=0;i<7;++i) {
            const lvRect &button=header.buttons[i];
            CHECK(button.width()>0 && button.height()>=(width>=1000?72:56));
            CHECK(header.hit((button.left+button.right)/2,(button.top+button.bottom)/2)==i);
        }
        CHECK(header.hit(width/2,top-1)==-1);
        CHECK(entries.count>0 && entries.area.bottom==entries.previous.top);
        for (int i=0;i<entries.count;++i) {
            lvRect row=entries.row(i);
            CHECK(row.height()>=60);
            CHECK(entries.hit(width-2,(row.top+row.bottom)/2,entries.count)==i);
        }
        CHECK(entries.hit(width/2,entries.next.top+1,entries.count)==-1);
        CHECK(entries.hit(width/2,entries.row(0).top,0)==-1);
        CRWindowSkinRef window=wm.getSkin()->getWindowSkin(U"#dict");
        dictview::header(canvas,window->getTitleSkin(),title,lString32("English → Russian"),4,true);
        for (int i=0;i<entries.count;++i) {
            lvRect row=entries.row(i);
            dictview::entry(canvas,window->getClientSkin(),row,lString32(i==0?"medicine":"medicine ball"),
                lString32(i==0?"noun · лекарство, медикамент; медицина":"noun · медбол, набивной мяч"),i==0);
        }
        dictview::navigation(canvas,window->getClientSkin(),entries);
        std::string path=std::string(out)+"/dictionary-lookup-"+std::to_string(width)+".pgm";
        FILE *image=fopen(path.c_str(),"wb"); CHECK(image);
        fprintf(image,"P5\n%d %d\n255\n",width,height);
        for(int y=0;y<height;++y) for(int x=0;x<width;++x) fputc(canvas.GetPixel(x,y),image);
        fclose(image);
    }
    CRDictionaryDownloadItem item(parent,0,cr3dict::catalog[0]);
    CHECK(item.onSelect()==1 && dialogs==0 && wifi==0 && pbDictionaryDownloadBusy());
    CHECK(item.onSelect()==1); tick(); CHECK(dialogs==1 && button=="Download" && wifi==0);
    answer(2); CHECK(!pbDictionaryDownloadBusy() && wifi==0 && progress==0);
    dictionarySchedule(entry); tick(); answer(1); CHECK(wifi==0); tick();
    CHECK(wifi==1 && progress==1); tick(); CHECK(progress==1); tick(); CHECK(progress==1); tick();
    CHECK(progress==0 && !pbDictionaryDownloadBusy() && result.find("installed")!=std::string::npos);
    CHECK(readFile(target)==payload && access(dictui::temporary,F_OK));
    CRDictionaryDownloadItem installedItem(parent,1,entry); CHECK(installedItem.getSubmenuValue()=="Installed");
    dictionarySchedule(entry); tick(); CHECK(result.find("Ready to use offline")!=std::string::npos && wifi==1);
    unlink(target.c_str());
    online=false; dictionarySchedule(entry); tick(); answer(1); tick();
    CHECK(result.find("Wi-Fi")!=std::string::npos && progress==0 && !pbDictionaryDownloadBusy()); online=true;
    failDownload=true; dictionarySchedule(entry); tick(); answer(1); tick(); tick(); tick();
    CHECK(result.find("network failure")!=std::string::npos && progress==0 && access(target.c_str(),F_OK) && access(dictui::temporary,F_OK)); failDownload=false;
    failStart=true; dictionarySchedule(entry); tick(); answer(1); tick();
    CHECK(progress==0 && access(dictui::temporary,F_OK) && !pbDictionaryDownloadBusy()); failStart=false;
    dictionarySchedule(entry); tick(); answer(1); pbCancelDictionaryDownload(false); CHECK(pbDictionaryDownloadBusy()); tick();
    CHECK(progress==1); cancelDownload(1); CHECK(progress==0 && access(dictui::temporary,F_OK) && !timer);
    dictionarySchedule(entry); tick(); answer(1); tick(); pbCancelDictionaryDownload();
    CHECK(progress==0 && !timer && !pbDictionaryDownloadBusy() && access(dictui::temporary,F_OK));
    dictionarySchedule(entry); tick(); answer(1); pbCancelDictionaryDownload(); CHECK(!timer && !pbDictionaryDownloadBusy());
    answer(1); CHECK(!timer); // stale confirmation after exit must not start a download
    dictionarySchedule(entry); tick(); answer(1); tick(); tick(); tick();
    CHECK(dictui::state==dictui::Verifying); cancelDownload(1);
    CHECK(progress==0 && !timer && access(target.c_str(),F_OK) && access(dictui::temporary,F_OK));
    payload.back()='y'; dictionarySchedule(entry); tick(); answer(1); tick(); tick(); tick(); tick();
    CHECK(result.find("verified")!=std::string::npos && progress==0 && access(target.c_str(),F_OK));
    payload=readFile(std::string(out)+"/stardict-good.zip");checksum=hash(payload);
    cr3dict::Entry star={"Test StarDict","ui.stardict","https://example.org/dictionary.zip",checksum.c_str(),"CC BY-SA 4.0",payload.size(),"stardict",4096};
    dictionarySchedule(star);tick();answer(1);
    for(unsigned i=0;pbDictionaryDownloadBusy();++i){CHECK(i<100);tick();}
    CHECK(progress==0 && result.find("installed")!=std::string::npos);
    auto found=stardict::discover(std::string(dictui::directory)+"/ui.stardict");CHECK(found.size()==1);
    stardict::Dictionary installed;std::string word,html;
    CHECK(installed.open(found[0].path)&&installed.lookup("houses",true,word,html)&&word=="house");
    // The parent owns the submenu, and the window manager owns active windows.
    // Detach before asking the manager to destroy the active child.
    parent->destroyMenu();
    puts("PASS: language-based downloads, installation/cancel/failure lifecycle, readable names, lookup layout and whole-row touch in three screen sizes");
    return 0;
}
int main(int argc,char **argv) {
    CHECK(argc==4); InitFontManager(lString8()); crCurrentLang=lString32("en");
    CHECK(fontMan->RegisterFont(lString8(argv[1])));
    int result=check(argv[1],argv[2],argv[3]); ShutdownFontManager(); return result;
}
