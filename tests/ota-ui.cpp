#include "crengine.h"
#include "crgui.h"
#include "testutil.h"
#include <cstring>
#include <cstdlib>
#undef CHECK
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::exit(1); } } while (0)
#include <unistd.h>
#include <string>
#define FLASHDIR "/tmp/cr3-ota-ui"
#define CR3_RELEASE_VERSION "1.0.1"
#define OTA_BRANCH_STABLE "master"
#define ICON_WARNING 1
#define ICON_ERROR 2
#define ICON_INFORMATION 3
#define _(s) (s)
lString32 pbSkinFileName, openedCacheFile, currentCacheDir, crCurrentLang;
static std::string device="632", firmware="U632.6.11.2001", response, result;
static bool online=true;
static int dialogs=0, requests=0, downloads=0, timers=0, progress=0;
static void (*timerCallback)() = NULL;
static void (*dismissCallback)(int) = NULL;
lString32 getPbModelNumber() { return lString32(device.c_str()); }
const char *GetSoftwareVersion() { return firmware.c_str(); }
bool pbNetwork(const char *) { return online; }
void Dialog(int, const char *, const char *text, const char *button, const char *, void (*callback)(int)) {
 result=text; ++dialogs; dismissCallback=callback; CHECK(!strcmp(button,"OK"));
}
void OpenProgressbar(int, const char *, const char *, int, void (*)(int)) { ++progress; }
void CloseProgressbar() { --progress; }
void UpdateProgressbar(const char *, int) {}
void OTA_downloadSuccessful() { CHECK(false); }
class web {
public:
 static std::string get(const std::string &) { ++requests;return response; }
 static bool getBinary(const std::string &, const std::string &) { ++downloads;return false; }
};
#include "../cr3gui/src/ota_fw6.h"
bool OTA_update(const char *branch) { return OTA_updateFW6(branch); }
void SetHardTimer(const char *, void (*callback)(), int delay) { ++timers;timerCallback=callback;CHECK(delay>0); }
#include "../cr3gui/src/ota_dispatch.h"
#include "../cr3gui/src/ota_menu.h"
class OTATestScreen : public CRGUIScreenBase {
public:
 OTATestScreen() : CRGUIScreenBase(1072,1448,false) { _canvas=LVRef<LVDrawBuf>(createCanvas(1072,1448)); }
 LVDrawBuf *createCanvas(int w,int h) { return new LVGrayDrawBuf(w,h,8); }
 void update(const lvRect &,bool) {}
};
class OTATestMenu : public CRMenu {
public:
 using CRMenu::CRMenu;
 using CRMenu::onItemSelect;
};
int main() {
 response="1.0.1\n"+std::string(64,'a')+"\n";
 OTATestScreen screen;
 CRGUIWindowManager wm(&screen);
 OTATestMenu menu(&wm,NULL,1,lString32("Settings"),LVImageSourceRef(),LVFontRef(),LVFontRef());
 menu.addItem(new CROTAUpdateMenuItem(&menu,123,"OTA Update"));
 CHECK(menu.onItemSelect(0,0));
 // Selection is consumed: no close/destroy and no native dialog during redraw.
 CHECK(menu.getItems().length()==1 && timers==1 && dialogs==0 && requests==0);
 CHECK(menu.onItemSelect(0,0));CHECK(timers==1); // repeated taps coalesce
 CHECK(!otaPendingBranch || !strcmp(otaPendingBranch,"master"));
 timerCallback();
 CHECK(dialogs==1 && result=="You have the latest version." && progress==0);
 CHECK(dismissCallback);dismissCallback(1);
 CHECK(menu.getItems().length()==1);
 device="631"; OTA_scheduleUpdate("master"); timerCallback();
 CHECK(dialogs==2 && result.find("Model: 631")!=std::string::npos && requests==1);
 device="632"; online=false;OTA_scheduleUpdate("master");timerCallback();
 CHECK(dialogs==3 && result.find("network")!=std::string::npos);
 online=true;response="<html>Error</html>";OTA_scheduleUpdate("master");timerCallback();
 CHECK(dialogs==4 && result.find("update information")!=std::string::npos && progress==0);
 response="1.0.2\n"+std::string(64,'a')+"\n";OTA_scheduleUpdate("master");timerCallback();
 CHECK(dialogs==5 && downloads==1 && result.find("unchanged")!=std::string::npos && progress==0);
 puts("PASS: OTA menu stays open, dispatch waits for redraw, repeated taps coalesce, results require OK and errors remain readable");
}
