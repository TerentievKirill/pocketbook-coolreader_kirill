#ifndef CR3_READER_UI_H
#define CR3_READER_UI_H
#include "crgui.h"
#ifdef CR3_SHARED_PACKAGE
#include <inkview.h>
#include "pb_device_profile.h"
#endif
inline int readerUiDpi() {
#ifdef CR3_SHARED_PACKAGE
    const pbdevice::Profile *profile = pbdevice::identify(GetDeviceModel());
    if (profile) return profile->dpi;
#endif
    return 300;
}
inline void styleReaderMenu(CRMenu *menu) {
    if (!menu->getWindowManager()->getScreen()->isTouchSupported()) return;
    menu->setSkinName(lString32("#settings"));
    menu->enableActionMenu(lString32("CoolReader"), readerUiDpi());
}
#endif
