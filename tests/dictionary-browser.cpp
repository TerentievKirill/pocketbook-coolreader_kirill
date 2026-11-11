#include "crengine.h"
#include "crgui.h"
#include "testutil.h"
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#define _(s) (s)
#define ICON_WARNING 3
#define NET_OK 0
#define TASK_MAKEACTIVE 128
#define PB_BROWSER_BINARY "/tmp/cr3-browser-fixture.app"
lString32 pbSkinFileName,openedCacheFile,currentCacheDir,crCurrentLang;
static bool active=true,closeDuringNetwork=false,cancelDuringNetwork=false,backgroundDuringNetwork=false;
static void pbCancelBrowser(bool includeConnecting);
static int networkResult=NET_OK,taskResult=0,connections=0,launches=0,restores=0,flags=0;
static std::string path,argument,message;
static void (*timer)()=NULL;
static void (*dismiss)(int)=NULL;
static bool pbBrowserCanLaunch() { return active; }
static void pbBrowserRestore() { ++restores; }
static void SetHardTimer(const char *,void (*callback)(),int) { timer=callback; }
static void ClearTimer(void (*callback)()) { if (timer==callback) timer=NULL; }
static void Dialog(int,const char *,const char *text,const char *,const char *,void (*callback)(int)) {
    message=text; dismiss=callback;
}
static int NetConnect(const char *) { ++connections; if (closeDuringNetwork) active=false; if (cancelDuringNetwork) pbCancelBrowser(true); if (backgroundDuringNetwork) pbCancelBrowser(false); return networkResult; }
static int OpenTask(const char *file,int argc,const char *const *argv,int taskFlags) {
    ++launches; path=file; flags=taskFlags;
    argument=argc==1 && argv && argv[0] ? argv[0] : "invalid argument count";
    return taskResult;
}
#include "../cr3gui/src/pb_browser.h"
#include "../cr3gui/src/pb_browser_ui.h"
#include "../cr3gui/src/dictionary_ui.h"
static void tick() { void (*callback)()=timer; timer=NULL; if (callback) callback(); }
int main() {
    // No font manager is initialized: the title indicator must still render.
    LVGrayDrawBuf canvas(100,60,8); canvas.FillRect(0,0,100,60,0xffffff);
    dictview::dropdown(canvas,lvRect(0,0,100,60));
    CHECK(canvas.GetPixel(69,26)==0 && canvas.GetPixel(75,33)==0);
    CHECK(canvas.GetPixel(69,33)==255 && canvas.GetPixel(90,26)==255);
    const char *google="https://www.google.com/search?hl=[LANG]&q=";
    const char *wiki="https://[LANG].wikipedia.org/?search=";
    std::string url=pbweb::searchURL(google,"RU-ru","cs","rock & roll? café");
    CHECK(url=="https://www.google.com/search?hl=ru&q=rock%20%26%20roll%3F%20caf%C3%A9");
    CHECK(pbweb::searchURL(wiki,"rus","en","слово")=="https://ru.wikipedia.org/?search=%D1%81%D0%BB%D0%BE%D0%B2%D0%BE");
    CHECK(pbweb::searchURL(wiki,"bad.host/?","CS-cz","a#b")=="https://cs.wikipedia.org/?search=a%23b");
    CHECK(pbweb::searchURL(wiki,"","invalid","x")=="https://en.wikipedia.org/?search=x");
    CHECK(pbweb::languageCode("UKR")=="uk" && pbweb::languageCode("ces")=="cs");
    { std::ofstream fixture(PB_BROWSER_BINARY); fixture << "fixture"; }
    CHECK(chmod(PB_BROWSER_BINARY,0755)==0);
    std::string original=url;
    pbScheduleBrowser(url.c_str()); url.assign(500,'z');
    CHECK(timer && connections==0 && launches==0); tick();
    CHECK(connections==1 && launches==1 && path==PB_BROWSER_BINARY && argument==original && flags==TASK_MAKEACTIVE);
    CHECK(pbweb::pending.empty() && !timer && active);
    taskResult=1; pbScheduleBrowser("https://en.wikipedia.org/?search=second"); tick();
    CHECK(launches==2 && argument=="https://en.wikipedia.org/?search=second");
    taskResult=-1; pbScheduleBrowser(original.c_str()); tick();
    CHECK(launches==3 && message.find("Couldn't open")!=std::string::npos && active);
    dismiss(1); CHECK(timer); tick(); CHECK(restores==1);
    taskResult=0; networkResult=-1; pbScheduleBrowser(original.c_str()); tick();
    CHECK(launches==3 && message.find("Wi-Fi")!=std::string::npos && active);
    networkResult=NET_OK;
    int before=connections;
    pbScheduleBrowser(original.c_str()); pbCancelBrowser();
    CHECK(!timer && pbweb::pending.empty() && connections==before);
    pbScheduleBrowser(original.c_str()); active=false; tick();
    CHECK(connections==before && launches==3); active=true;
    closeDuringNetwork=true; pbScheduleBrowser(original.c_str()); tick();
    CHECK(connections==before+1 && launches==3); active=true; closeDuringNetwork=false;
    cancelDuringNetwork=true; pbScheduleBrowser(original.c_str()); tick();
    CHECK(connections==before+2 && launches==3); cancelDuringNetwork=false;
    CHECK(unlink(PB_BROWSER_BINARY)==0);
    pbScheduleBrowser(original.c_str()); tick();
    CHECK(message.find("not available")!=std::string::npos && connections==before+2 && launches==3);
    pbScheduleBrowser(""); CHECK(!timer && message.find("Select a word")!=std::string::npos);
    { std::ofstream fixture(PB_BROWSER_BINARY); fixture << "fixture"; }
    CHECK(chmod(PB_BROWSER_BINARY,0755)==0);
    backgroundDuringNetwork=true; pbScheduleBrowser(original.c_str()); tick();
    CHECK(launches==4 && argument==original); backgroundDuringNetwork=false;
    CHECK(unlink(PB_BROWSER_BINARY)==0);
    puts("PASS: font-independent dropdown, encoded Unicode/search punctuation, language fallback, deferred native browser tasks, existing browser activation, Wi-Fi/launch errors and task-switch cancellation");
    return 0;
}
