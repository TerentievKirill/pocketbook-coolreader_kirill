#include "crengine.h"
#include "crgui.h"
#include "testutil.h"
#include <cstring>
#include <cstdlib>
#undef CHECK
#define CHECK(condition) do { if (!(condition)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); std::exit(1); } } while (0)
#include <unistd.h>
#include <string>
#include <fstream>
#include <sys/stat.h>
#define FLASHDIR "/tmp/cr3-ota-ui"
#define CR3_RELEASE_VERSION "1.0.1"
#define OTA_BRANCH_STABLE "master"
#define ICON_WARNING 1
#define ICON_ERROR 2
#define ICON_INFORMATION 3
#define _(s) (s)
lString32 pbSkinFileName, openedCacheFile, currentCacheDir, crCurrentLang;
static std::string lastCheckUrl,lastDownloadUrl;
static std::string device="632", firmware="U632.6.11.2001", response, result, leftButton, rightButton, payload;
static bool online=true, failDownload=false, failCheck=false, unknownLength=false;
static int dialogs=0, requests=0, downloads=0, timers=0, progress=0, percent=-1, restarts=0;
static void (*weakCallback)() = NULL;
static int weakDelay=0, wifiHelpers=0;
static bool autoEnabled=true, promptSafe=true, canRun=true;
static time_t fakeNow=1760000000;
static time_t nowMock() { return fakeNow; }
#define OTA_BACKGROUND_TEST_CLOCK nowMock
bool pbAutoUpdateEnabled() { return autoEnabled; }
bool pbCanRunBackgroundUpdate() { return canRun; }
bool pbCanShowUpdatePrompt() { return promptSafe; }
static void (*restoreCallback)() = NULL;
static int restores=0;
void pbRestoreUpdateScreen() { ++restores; }
static void (*timerCallback)() = NULL;
static void (*dismissCallback)(int) = NULL;
static void (*cancelCallback)(int) = NULL;
lString32 getPbModelNumber() { return lString32(device.c_str()); }
const char *GetSoftwareVersion() { return firmware.c_str(); }
bool pbNetwork(const char *) { ++wifiHelpers;return online; }
void Dialog(int, const char *, const char *text, const char *button1, const char *button2, void (*callback)(int)) {
 result=text;leftButton=button1;rightButton=button2 ? button2 : "";++dialogs;dismissCallback=callback;
}
void OpenProgressbar(int, const char *, const char *text, int n, void (*callback)(int)) {
 ++progress;CHECK(progress==1);result=text;percent=n;cancelCallback=callback;
}
void CloseProgressbar() { --progress;CHECK(progress==0); }
void UpdateProgressbar(const char *text, int n) { result=text;percent=n; }
void SetHardTimer(const char *, void (*callback)(), int delay) { ++timers;if(callback==pbRestoreUpdateScreen)restoreCallback=callback;else timerCallback=callback;CHECK(delay>0); }
void SetWeakTimer(const char *,void (*callback)(),int delay) { weakCallback=callback;weakDelay=delay; }
void ClearTimer(void (*callback)()) { if(timerCallback==callback) timerCallback=NULL;if(weakCallback==callback)weakCallback=NULL; }
void OTA_DL_dialog_handler(int) { ++restarts; }
class WebTransfer {
 bool check=true, ok=false; int polls=0;
 std::string text, failure, output;
public:
 bool start(const std::string &url,const std::string &path,size_t) {
  check=path.empty();output=path;if(check)lastCheckUrl=url;else lastDownloadUrl=url;
  if(check) { ++requests;text=response;ok=!failCheck;failure="The update server returned HTTP 503."; }
  else { ++downloads;ok=!failDownload;failure="Could not resolve host.";std::ofstream f(path,std::ios::binary);f<<payload; }
  return true;
 }
 bool poll() { ++polls;return check || polls>=2; }
 bool succeeded() const { return ok; }
 const std::string &body() const { return text; }
 const std::string &error() const { return failure; }
 long long received() const { return polls<2 ? 25 : 100; }
 long long total() const { return unknownLength ? 0 : 100; }
 void cancel() { ok=false;if(!output.empty())unlink(output.c_str()); }
};
#include "../cr3gui/src/ota_fw6.h"
#include "../cr3gui/src/ota_background.h"
bool OTA_update(const char *branch) { return OTA_updateFW6(branch); }
void OTA_showVersion() { OTA_showVersionFW6(); }
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
 using CRMenu::CRMenu; using CRMenu::onItemSelect;
};
static void tick() { CHECK(timerCallback);auto callback=timerCallback;timerCallback=NULL;callback(); }
static void weakTick() { CHECK(weakCallback);auto callback=weakCallback;weakCallback=NULL;callback(); }
static std::string releaseText(const std::string &version,const std::string &checksum) {
#ifdef CR3_SHARED_PACKAGE
 return "CR3-OTA-SHARED-1\npocketbook-armv7-softfp\n"+version+"\n"+checksum+"\n632 U632.6.11.2001\n634 U634.6.10.3425\n700 U700.6.10.3209\n743G U743g.6.11.1683\n";
#else
 return version+"\n"+checksum+"\n";
#endif
}
static void checkUpdate() { OTA_scheduleUpdate("master");tick();if(fw6ota::state==fw6ota::Checking)tick(); }
int main(int argc,char **argv) {
 CHECK(argc==2);
 CHECK(otaRemoveTree(FLASHDIR));CHECK(!mkdir(FLASHDIR,0755));CHECK(!mkdir(FLASHDIR "/system",0755));CHECK(!mkdir(FLASHDIR "/system/share",0755));
 response=releaseText("1.0.1",std::string(64,'a'));
 OTATestScreen screen;CRGUIWindowManager wm(&screen);
 OTATestMenu menu(&wm,NULL,1,lString32("Settings"),LVImageSourceRef(),LVFontRef(),LVFontRef());
 menu.addItem(new CROTAUpdateMenuItem(&menu,123,"Check for updates"));
 menu.addItem(new CRVersionMenuItem(&menu,124,"Version"));
 CHECK(menu.onItemSelect(0,0));CHECK(menu.getItems().length()==2 && timers==1 && dialogs==0 && requests==0);
 CHECK(menu.onItemSelect(0,0));CHECK(timers==1);tick();CHECK(dialogs==0 && progress==1);tick();
 CHECK(result.find("latest version")!=std::string::npos && progress==0 && leftButton=="OK");
 int beforeRestore=restores;dismissCallback(1);CHECK(restoreCallback);
 restoreCallback();restoreCallback=NULL;CHECK(restores==beforeRestore+1);
 CHECK(menu.getItems().length()==2);
 CHECK(menu.getItems()[1]->getSubmenuValue()==lString32("1.0.1"));
 CHECK(menu.onItemSelect(1,0));tick();CHECK(result=="CoolReader 1.0.1" && leftButton=="OK");
 dismissCallback(1);CHECK(restoreCallback);restoreCallback();restoreCallback=NULL;
 CHECK(menu.getItems().length()==2);
 device="631";checkUpdate();CHECK(result.find("Model: 631")!=std::string::npos && requests==1);
 device="632";online=false;checkUpdate();CHECK(result.find("Wi-Fi")!=std::string::npos);online=true;
 failCheck=true;checkUpdate();CHECK(result.find("503")!=std::string::npos && progress==0);failCheck=false;
 response="<html>Error</html>";checkUpdate();CHECK(result.find("invalid release")!=std::string::npos);
 std::ifstream input(std::string(argv[1])+"/ota-ui-success.zip",std::ios::binary);
 payload.assign(std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>());CHECK(!payload.empty());
 unsigned char hash[32];SHA256((const unsigned char*)payload.data(),payload.size(),hash);
 char hex[65];for(int i=0;i<32;++i)sprintf(hex+2*i,"%02x",hash[i]);
 response=releaseText("1.0.2",hex);
 checkUpdate();CHECK(leftButton=="Download" && rightButton=="Cancel" && downloads==0);
 CHECK(result.find("Installed: 1.0.1")!=std::string::npos && result.find("Available: 1.0.2")!=std::string::npos);
 dismissCallback(2);CHECK(fw6ota::state==fw6ota::Idle && downloads==0);
 checkUpdate();dismissCallback(1);CHECK(downloads==0);tick();CHECK(downloads==1 && cancelCallback);tick();
 CHECK(percent==25 && result.find("25%")!=std::string::npos);cancelCallback(1);
 CHECK(fw6ota::state==fw6ota::Idle && result.find("cancelled")!=std::string::npos && progress==0 && !timerCallback);
 CHECK(access(FLASHDIR "/.cr3-ota.zip",F_OK)!=0);
 unknownLength=true;checkUpdate();dismissCallback(1);tick();tick();
 CHECK(percent==0 && result.find("KB")!=std::string::npos && result.find('%')==std::string::npos);
 cancelCallback(1);unknownLength=false;
 OTA_scheduleUpdate("master");tick();CHECK(progress==1 && cancelCallback);cancelCallback(1);
 CHECK(fw6ota::state==fw6ota::Idle && !timerCallback && progress==0);
 failDownload=true;checkUpdate();dismissCallback(1);tick();tick();tick();
 CHECK(result.find("resolve host")!=std::string::npos && progress==0);failDownload=false;
 // Bad checksums never create a ready marker.
 response=releaseText("1.0.2",std::string(64,'a'));checkUpdate();dismissCallback(1);tick();tick();tick();
 CHECK(result.find("Checksum mismatch")!=std::string::npos && access(FW6_OTA_STAGE "/.ready",F_OK)!=0);
 response=releaseText("1.0.2",hex);checkUpdate();dismissCallback(1);tick();tick();tick();
 CHECK(leftButton=="Restart now" && rightButton=="Later" && progress==0 && restarts==0);
 CHECK(access(FW6_OTA_STAGE "/.ready",F_OK)==0);dismissCallback(2);CHECK(restarts==0 && fw6ota::state==fw6ota::Idle);
 int previousRequests=requests;checkUpdate();CHECK(requests==previousRequests && leftButton=="Restart now");
 dismissCallback(1);CHECK(restarts==1);
 // Automatic checks run silently, only while active and enabled.
 CHECK(!mkdir(FLASHDIR "/system/config",0755));CHECK(!mkdir(FLASHDIR "/system/config/cr3",0755));
 int beforeRequests=requests,beforeDialogs=dialogs,beforeDownloads=downloads,beforeWifi=wifiHelpers;
 OTA_backgroundActive(true);CHECK(weakCallback && weakDelay==30000);
 autoEnabled=false;weakTick();CHECK(requests==beforeRequests);autoEnabled=true;
 promptSafe=false;weakTick();CHECK(requests==beforeRequests);promptSafe=true;
 response=releaseText("1.0.1",std::string(64,'a'));
 weakTick();CHECK(requests==beforeRequests+1 && progress==0);weakTick();
 CHECK(dialogs==beforeDialogs && wifiHelpers==beforeWifi && downloads==beforeDownloads);
 weakTick();CHECK(requests==beforeRequests+1); // daily rate limit
 fakeNow+=86401;response="broken";weakTick();weakTick();CHECK(dialogs==beforeDialogs);
 CHECK(fw6background::nextCheck==fakeNow+1800);
 fakeNow+=1801;failCheck=true;weakTick();weakTick();CHECK(dialogs==beforeDialogs);failCheck=false;
 fakeNow+=1801;response=releaseText("1.0.3",std::string(64,'a'));weakTick();promptSafe=false;weakTick();
 CHECK(dialogs==beforeDialogs && !fw6background::candidate.empty());promptSafe=true;weakTick();
 CHECK(dialogs==beforeDialogs+1 && leftButton=="Download" && rightButton=="Later");
 CHECK(result.find("1.0.3")!=std::string::npos && downloads==beforeDownloads && wifiHelpers==beforeWifi);
 dismissCallback(2);CHECK(fw6ota::state==fw6ota::Idle);
 // Snooze and daily check times survive closing/reopening the application.
 OTA_backgroundActive(false);CHECK(!weakCallback && !fw6background::transfer);
 fw6background::loaded=false;fw6background::nextCheck=0;fw6background::promptedAt=0;fw6background::promptedVersion.clear();
 OTA_backgroundActive(true);CHECK(fw6background::promptedVersion=="1.0.3");
 beforeRequests=requests;weakTick();CHECK(requests==beforeRequests && dialogs==beforeDialogs+1);
 // Even a forced recheck cannot nag about the same version within a day.
 fw6background::nextCheck=0;weakTick();weakTick();CHECK(dialogs==beforeDialogs+1);
 fakeNow+=86401;weakTick();weakTick();CHECK(dialogs==beforeDialogs+2);dismissCallback(2);
 // Disabling checks or hiding/suspending the reader aborts a pending check.
 fw6background::nextCheck=0;weakTick();CHECK(fw6background::transfer);autoEnabled=false;weakTick();
 CHECK(!fw6background::transfer && dialogs==beforeDialogs+2);autoEnabled=true;
 fw6background::nextCheck=0;weakTick();CHECK(fw6background::transfer);OTA_backgroundActive(false);
 CHECK(!fw6background::transfer && !weakCallback);
 OTA_backgroundActive(true);fw6background::nextCheck=0;weakTick();CHECK(fw6background::transfer);
 canRun=false;weakTick();CHECK(!fw6background::transfer);canRun=true;
 // Manual actions take priority over an in-flight automatic check.
 fw6background::nextCheck=0;weakTick();CHECK(fw6background::transfer);
 OTA_scheduleUpdate("master");tick();CHECK(!fw6background::transfer && fw6ota::state==fw6ota::RestartPrompt);
 dismissCallback(2);OTA_backgroundActive(false);
 // Once the user accepts an automatic prompt, errors use the visible manual flow.
 response=releaseText("1.0.4",std::string(64,'a'));
 OTA_backgroundActive(true);fw6background::nextCheck=0;weakTick();weakTick();
 CHECK(rightButton=="Later");failDownload=true;dismissCallback(1);tick();tick();tick();
 CHECK(result.find("resolve host")!=std::string::npos && leftButton=="OK" && progress==0);
 failDownload=false;OTA_backgroundActive(false);
#ifdef CR3_SHARED_PACKAGE
 // All registered readers use the identical channel and download URL.
 CHECK(otaRemoveTree(FW6_OTA_STAGE));
 const char *models[]={"632","634","700","743G"};
 const char *firmwares[]={"U632.6.11.2001","U634.6.10.3425","U700.6.10.3209","U743g.6.11.1683"};
 for(int i=0;i<4;++i) {
  device=models[i];firmware=firmwares[i];response=releaseText("1.0.2",hex);
  checkUpdate();CHECK(fw6ota::state==fw6ota::Confirming);
  CHECK(lastCheckUrl==FW6_OTA_URL "ota/pocketbook-armv7-softfp.txt");
  dismissCallback(1);tick();
  CHECK(lastDownloadUrl==FW6_OTA_URL "downloads/coolreader-shared/1.0.2/coolreader-pocketbook-armv7-softfp.zip");
  cancelCallback(1);CHECK(fw6ota::state==fw6ota::Idle);
 }
 device="632";firmware=firmwares[0];
 int beforeInvalidDownloads=downloads;
 response=releaseText("1.0.2",hex);response.replace(response.find("pocketbook-armv7-softfp"),std::string("pocketbook-armv7-softfp").size(),"pocketbookhf");
 checkUpdate();CHECK(fw6ota::state==fw6ota::Idle && downloads==beforeInvalidDownloads);
 response="1.0.2\n"+std::string(hex)+"\n";checkUpdate();
 CHECK(fw6ota::state==fw6ota::Idle && downloads==beforeInvalidDownloads);
 response=releaseText("1.0.2",hex);response.replace(response.find(firmwares[0]),strlen(firmwares[0]),"U632.6.9.1");
 checkUpdate();CHECK(fw6ota::state==fw6ota::Idle && downloads==beforeInvalidDownloads);
#endif
 CHECK(otaRemoveTree(FLASHDIR));
 puts("PASS: retained menu, asynchronous check, confirmation, progress, cancel, HTTP/network/checksum errors, restart now/later, daily background checks, quiet failures, deferred/snoozed prompts, preference and standby lifecycle");
}
