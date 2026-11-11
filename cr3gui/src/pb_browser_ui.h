#ifndef CR3_PB_BROWSER_UI_H
#define CR3_PB_BROWSER_UI_H
#include <string>
#include <unistd.h>
// Included by the frontend and the regression with fake InkView I/O.
static bool pbBrowserCanLaunch();
static void pbBrowserRestore();
namespace pbweb { static std::string pending; static unsigned generation=0; static bool connecting=false; }
static void pbBrowserLaunch();
static void pbBrowserDismiss(int) { SetHardTimer("Browser_restore",pbBrowserRestore,150); }
static void pbBrowserError(const char *text) {
    Dialog(ICON_WARNING,_("Web lookup"),text,_("OK"),NULL,pbBrowserDismiss);
}
static void pbCancelBrowser(bool includeConnecting=true) {
    if (!includeConnecting && pbweb::connecting) return;
    ClearTimer(pbBrowserLaunch); ClearTimer(pbBrowserRestore); pbweb::pending.clear(); ++pbweb::generation;
}
static void pbBrowserLaunch() {
    std::string url; url.swap(pbweb::pending);
    if (url.empty() || !pbBrowserCanLaunch()) return;
    if (access(PB_BROWSER_BINARY,X_OK)) {
        pbBrowserError(_("The PocketBook browser is not available on this device.")); return;
    }
    unsigned generation=pbweb::generation;
    pbweb::connecting=true;
    int network=NetConnect(NULL);
    pbweb::connecting=false;
    // Wi-Fi setup can reenter the event loop, including closing the reader.
    if (!pbBrowserCanLaunch() || generation!=pbweb::generation) return;
    if (network!=NET_OK) {
        pbBrowserError(_("Couldn't connect to Wi-Fi. The dictionary is still available offline.")); return;
    }
    const char *arguments[]={url.c_str()};
    // The task manager starts or activates the browser and passes its URL.
    // run_script on firmware 6 is a screen helper, not a shell interpreter.
    if (OpenTask(PB_BROWSER_BINARY,1,arguments,TASK_MAKEACTIVE)<0 && pbBrowserCanLaunch())
        pbBrowserError(_("Couldn't open the PocketBook browser. Your dictionary is still open; please try again."));
}
static void pbScheduleBrowser(const char *url) {
    pbCancelBrowser();
    pbweb::pending=url ? url : "";
    if (pbweb::pending.empty()) pbBrowserError(_("Select a word to look up first."));
    else SetHardTimer("Browser_open",pbBrowserLaunch,150);
}
#endif
