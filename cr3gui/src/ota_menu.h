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
