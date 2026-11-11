// Included by the PocketBook frontend and the UI regression with fake InkView I/O.
#include "dictionary_download_core.h"
#include "dictionary_ui.h"
#include <memory>
#include <ctime>
namespace dictui {
enum State { Idle, Scheduled, Confirming, Connecting, Downloading, Verifying };
static State state = Idle;
static const cr3dict::Entry *selected = NULL;
static std::unique_ptr<WebTransfer> transfer;
static std::unique_ptr<cr3dict::Installation> installation;
static bool progressOpen = false;
static time_t lastPaint = 0;
static const char *directory = FLASHDIR "/system/dictionaries";
static const char *temporary = FLASHDIR "/system/dictionaries/.coolreader-download.part";
}
static void dictionaryPoll();
static void dictionaryStart();
static void dictionaryOffer();
static void dictionaryRestore() { pbRestoreUpdateScreen(); }
static void dictionaryDismiss(int) { SetHardTimer("Dictionary_restore", dictionaryRestore, 150); }
static void dictionaryResult(int icon, const std::string &message) {
    Dialog(icon, _("Dictionaries"), message.c_str(), _("OK"), NULL, dictionaryDismiss);
}
static void dictionaryReset() {
    ClearTimer(dictionaryPoll); ClearTimer(dictionaryStart); ClearTimer(dictionaryOffer);
    if (dictui::transfer) dictui::transfer->cancel();
    dictui::transfer.reset();
    dictui::installation.reset();
    unlink(dictui::temporary);
    dictui::selected = NULL; dictui::state = dictui::Idle;
    if (dictui::progressOpen) {
        dictui::progressOpen = false; CloseProgressbar(); pbRestoreUpdateScreen();
    }
}
bool pbDictionaryDownloadBusy() { return dictui::state != dictui::Idle; }
void pbCancelDictionaryDownload(bool includeConnecting) {
    if (!includeConnecting && dictui::state == dictui::Connecting) return;
    dictionaryReset();
}
static void dictionaryCancel(int) {
    dictionaryReset(); dictionaryResult(ICON_INFORMATION, _("Download cancelled."));
}
static void dictionaryFail(const std::string &error) {
    dictionaryReset(); dictionaryResult(ICON_ERROR, error);
}
static void dictionaryStart() {
    if (dictui::state != dictui::Connecting || !dictui::selected) return;
    const cr3dict::Entry &entry = *dictui::selected;
    std::string error;
    if (!cr3dict::prepare(dictui::directory, entry, error)) { dictionaryFail(error); return; }
    if (!pbNetwork("connect")) {
        dictionaryFail(_("Couldn't connect to Wi-Fi. Check the reader's network settings and try again.")); return;
    }
    // A task/sleep event during Wi-Fi setup can cancel this request.
    if (dictui::state != dictui::Connecting) return;
    if (unlink(dictui::temporary) && errno != ENOENT) {
        dictionaryFail(_("Could not clear the unfinished download. Check the reader's storage.")); return;
    }
    dictui::state = dictui::Downloading; dictui::lastPaint = 0;
    dictui::transfer.reset(new WebTransfer());
    pbRestoreUpdateScreen();
    OpenProgressbar(ICON_INFORMATION, _("Download dictionary"), entry.label, 0, dictionaryCancel);
    dictui::progressOpen = true;
    if (!dictui::transfer->start(entry.url, dictui::temporary, entry.size)) {
        error = dictui::transfer->error(); dictionaryFail(error); return;
    }
    SetHardTimer("Dictionary_download", dictionaryPoll, 100);
}
static void dictionaryConfirm(int button) {
    if (dictui::state != dictui::Confirming) return;
    if (button != 1) { dictionaryReset(); dictionaryDismiss(button); return; }
    dictui::state = dictui::Connecting;
    SetHardTimer("Dictionary_connect", dictionaryStart, 150);
}
static void dictionaryOffer() {
    if (dictui::state != dictui::Scheduled || !dictui::selected) return;
    const cr3dict::Entry &entry = *dictui::selected;
    if (cr3dict::installed(dictui::directory, entry)) {
        dictionaryReset(); dictionaryResult(ICON_INFORMATION, _("Ready to use offline. Open Dictionary, then tap its name to select this dictionary.")); return;
    }
    dictui::state = dictui::Confirming;
    std::string text = std::string(entry.label) + "\n" + cr3dict::sizeLabel(entry.size) +
        "\n\n" + _("Free download. Connect to Wi-Fi to install; use offline afterwards.") +
        "\n\n" + entry.credit;
    Dialog(ICON_INFORMATION, _("Download dictionary"), text.c_str(), _("Download"), _("Cancel"), dictionaryConfirm);
}
static void dictionarySchedule(const cr3dict::Entry &entry) {
    if (pbDictionaryDownloadBusy()) return;
    dictui::selected = &entry; dictui::state = dictui::Scheduled;
    SetHardTimer("Dictionary_offer", dictionaryOffer, 150);
}
static void dictionaryPoll() {
    if (dictui::state == dictui::Verifying && dictui::installation) {
        if (!dictui::installation->poll()) {
            SetHardTimer("Dictionary_verify", dictionaryPoll, 50); return;
        }
        if (!dictui::installation->succeeded()) {
            std::string error = dictui::installation->error(); dictionaryFail(error); return;
        }
        std::string name = dictui::selected->label;
        dictionaryReset();
        dictionaryResult(ICON_INFORMATION, name + "\n\n" +
            _("Dictionary installed. Open Dictionary and tap its name at the top to select it. No restart is needed."));
        return;
    }
    if (dictui::state != dictui::Downloading || !dictui::transfer) return;
    bool done = dictui::transfer->poll();
    const cr3dict::Entry &entry = *dictui::selected;
    time_t now = time(NULL);
    if (done || now != dictui::lastPaint) {
        unsigned long long bytes = dictui::transfer->received();
        int percent = static_cast<int>(bytes * 100 / entry.size);
        std::string label = std::string(entry.label) + "\n" +
            cr3dict::sizeLabel(bytes) + " / " + cr3dict::sizeLabel(entry.size);
        UpdateProgressbar(label.c_str(), percent > 100 ? 100 : percent);
        dictui::lastPaint = now;
    }
    if (!done) { SetHardTimer("Dictionary_download", dictionaryPoll, 100); return; }
    if (!dictui::transfer->succeeded()) {
        std::string error = dictui::transfer->error(); dictionaryFail(_("Could not download the dictionary.") + std::string("\n") + error); return;
    }
    // Drop the closed transfer before committing: cancel must not remove a saved file.
    dictui::transfer.reset();
    UpdateProgressbar(_("Checking and installing..."), 100);
    dictui::state = dictui::Verifying;
    dictui::installation.reset(new cr3dict::Installation(dictui::temporary, dictui::directory, entry));
    SetHardTimer("Dictionary_verify", dictionaryPoll, 50);
}
class CRDictionaryDownloadItem : public CRMenuItem {
    const cr3dict::Entry &_entry;
public:
    CRDictionaryDownloadItem(CRMenu *parent, int id, const cr3dict::Entry &entry)
        : CRMenuItem(parent, id, Utf8ToUnicode(entry.label), LVImageSourceRef(), LVFontRef()), _entry(entry) {}
    virtual lString32 getSubmenuValue() {
        return cr3dict::installed(dictui::directory, _entry) ? lString32(_("Installed")) :
            Utf8ToUnicode((std::string(_("Download")) + " · " + cr3dict::sizeLabel(_entry.size)).c_str());
    }
    virtual void Draw(LVDrawBuf &buf,lvRect &rc,CRRectSkinRef skin,CRRectSkinRef valueSkin,bool selected) {
        if (_menu && _menu->isSettingsEditor()) CRMenuItem::Draw(buf,rc,skin,valueSkin,selected);
        else { dictview::entry(buf,skin,rc,_label,getSubmenuValue(),selected); _itemDirty=false; }
    }
    virtual int onSelect() { dictionarySchedule(_entry); return 1; }
};
class CRDictionaryDownloadsMenu : public CRMenu {
    const char *_language;
    const char *_format;
public:
    CRDictionaryDownloadsMenu(CRMenu *parent,int id,const char *language=NULL,const char *format="stardict")
        : CRMenu(parent->getWindowManager(),parent,id,
                 !language ? lString32(_("Dictionaries")) : !strcmp(format,"pocketbook") ? lString32(_("PocketBook versions")) :
                    Utf8ToUnicode((std::string(language)+" "+_("dictionaries")).c_str()), "",
                 LVFontRef(),parent->getValueFont(),CRPropRef()), _language(language),_format(format) {
        setSkinName(lString32("#settings")); setAccelerators(parent->getAccelerators());
        if (!language) {
            const char *languages[]={"English","Russian","Ukrainian","Czech"};
            for (int i=0;i<4;++i) addItem(new CRDictionaryDownloadsMenu(this,i,languages[i]));
        } else {
            for (unsigned i=0;i<sizeof(cr3dict::catalog)/sizeof(cr3dict::catalog[0]);++i) {
                const cr3dict::Entry &entry=cr3dict::catalog[i];
                std::string prefix=std::string(language)+" ";
                if (!strcmp(entry.format,format) && !strncmp(entry.label,prefix.c_str(),prefix.size()))
                    addItem(new CRDictionaryDownloadItem(this,i,entry));
            }
            if (!strcmp(format,"stardict")) addItem(new CRDictionaryDownloadsMenu(this,100,language,"pocketbook"));
        }
        reconfigure(0);
    }
    virtual lString32 getSubmenuValue() {
        return lString32(!_language ? _("Free downloads · use offline") : !strcmp(_format,"pocketbook") ?
            _("For other PocketBook apps too") : _("Free · works offline"));
    }
    virtual void drawStatusBar() {
        if (_pages>1) { CRMenu::drawStatusBar(); return; }
        lvRect rc;
        if (!getStatusRect(rc)) return;
        LVDrawBuf &buf=*getWindowManager()->getScreen()->getCanvas();
        buf.FillRect(rc,0xffffff);
        buf.FillRect(rc.left,rc.top,rc.right,rc.top+1,0);
        dictview::label(buf,getSkin()->getTitleSkin(),rc,lString32(_("Downloads install immediately")),26,false,true);
    }
    virtual void Draw(LVDrawBuf &buf,lvRect &rc,CRRectSkinRef skin,CRRectSkinRef valueSkin,bool selected) {
        if (_menu && _menu->isSettingsEditor()) CRMenu::Draw(buf,rc,skin,valueSkin,selected);
        else { dictview::entry(buf,skin,rc,_label,getSubmenuValue(),selected); _itemDirty=false; }
    }
    virtual int onSelect() { getWindowManager()->activateWindow(this); return 1; }
};

CRMenu *pbCreateDictionaryDownloadsMenu(CRMenu *parent, int id) {
    return new CRDictionaryDownloadsMenu(parent, id);
}
