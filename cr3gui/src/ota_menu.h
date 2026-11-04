// A settings action that consumes selection instead of closing its parent menu.
class CROTAUpdateMenuItem : public CRMenuItem {
public:
    CROTAUpdateMenuItem(CRMenu *menu, int command, const char *label)
        : CRMenuItem(menu, command, label, LVImageSourceRef(), LVFontRef()) {}
    virtual int onSelect() {
        OTA_scheduleUpdate("master");
        return 1;
    }
};

static bool versionMenuPending = false;
static void versionMenuShow() { versionMenuPending = false; OTA_showVersion(); }
class CRVersionMenuItem : public CRMenuItem {
public:
    CRVersionMenuItem(CRMenu *menu, int command, const char *label)
        : CRMenuItem(menu, command, label, LVImageSourceRef(), LVFontRef()) {}
    virtual lString32 getSubmenuValue() { return lString32(CR3_RELEASE_VERSION); }
    virtual int onSelect() {
        if (!versionMenuPending) {
            versionMenuPending = true;
            SetHardTimer("Version", versionMenuShow, 150);
        }
        return 1;
    }
};
