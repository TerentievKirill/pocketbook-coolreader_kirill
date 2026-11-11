#ifndef CR3_READER_MENU_H
#define CR3_READER_MENU_H
#include "mainwnd.h"
#include "cr3pocketbook.h"
#include "cri18n.h"
#include <set>
#include <utility>

class CRReaderCommandItem : public CRMenuItem {
    lString32 _hint;
    int _parameter;
public:
    CRReaderCommandItem(CRMenu *menu,int command,int parameter,const char *label,const char *hint)
        : CRMenuItem(menu,command,label,LVImageSourceRef(),LVFontRef()), _hint(hint), _parameter(parameter) {}
    virtual lString32 getSubmenuValue() { return _hint; }
    virtual int onSelect() {
        _menu->closeAllMenu(getId(),_parameter);
        return 1;
    }
};
class CRReaderMenu : public CRMenu {
    lString32 _hint;
public:
    CRReaderMenu(CRGUIWindowManager *wm,CRMenu *parent,const char *label,const char *hint,CRGUIAcceleratorTableRef accelerators)
        : CRMenu(wm,parent,0,label,"",LVFontRef(),LVFontRef()), _hint(hint) {
        setSkinName(lString32("#settings")); setAccelerators(accelerators); setFullscreen(true);
        if (!parent) enableActionMenu(lString32("CoolReader"));
    }
    virtual lString32 getSubmenuValue() { return _hint; }
};
inline CRMenu *createReaderMenu(CRGUIWindowManager *wm,CRGUIAcceleratorTableRef accelerators,
                               CRGUIAcceleratorTableRef configured,bool hasHelp) {
    CRReaderMenu *root=new CRReaderMenu(wm,NULL,_("Reader menu"),"",accelerators);
    std::set<std::pair<int,int> > used;
    auto add=[&](CRMenu *menu,int command,int parameter,const char *label,const char *hint) {
        if (!used.insert(std::make_pair(command,parameter)).second) return;
        menu->addItem(new CRReaderCommandItem(menu,command,parameter,label,hint));
    };
    add(root,MCMD_SETTINGS,0,_("Settings"),_("Font, page layout, controls and dictionaries"));
    add(root,PB_CMD_CONTENTS,0,_("Table of contents"),_("Jump to a chapter"));
    add(root,MCMD_BOOKMARK_LIST_GO_MODE,0,_("Bookmarks"),_("Return to a saved place"));
    add(root,MCMD_DICT,0,_("Dictionary"),_("Look up a word"));
    add(root,MCMD_SEARCH,0,_("Search"),_("Find text in this book"));
    CRReaderMenu *more=new CRReaderMenu(wm,root,_("More tools"),_("Pages, notes, recent books and help"),accelerators);
    root->addItem(more);
    add(more,MCMD_GO_PAGE,0,_("Go to page"),_("Enter a page number"));
    add(more,MCMD_RECENT_BOOK_LIST,0,_("Recent books"),_("Switch to a recently opened book"));
    add(more,MCMD_CITES_LIST,0,_("Notes and highlights"),_("Review saved passages"));
    add(more,MCMD_CITE,0,_("Select text"),_("Save a passage from this page"));
    add(more,PB_CMD_MAIN_MENU,0,_("PocketBook home"),_("Return to the device library"));
    if (hasHelp) add(more,MCMD_HELP,0,_("User manual"),_("Help with reading and controls"));
    add(more,MCMD_ABOUT,0,_("About CoolReader"),_("Book and application information"));
    // Preserve configured tools and their parameters, without duplicating the
    // everyday actions above or recursively reopening this menu.
    if (!configured.isNull()) for (int i=0;i<configured->length();++i) {
        const CRGUIAccelerator *item=configured->get(i);
        if (!item->commandId || item->commandId==MCMD_MAIN_MENU) continue;
        const char *label=getCommandName(item->commandId,item->commandParam);
        const char *hint=_("Additional reader action");
        switch (item->commandId) {
        case MCMD_BOOKMARK_LIST: label=_("Save a bookmark"); hint=_("Mark the current page"); break;
        case DCMD_SAVE_HISTORY: label=_("Save reading progress"); hint=_("Save the current place and bookmarks now"); break;
        case DCMD_SAVE_TO_CACHE: label=_("Prepare book for faster opening"); hint=_("Keep a prepared copy for the next reading session"); break;
        case MCMD_HELP_KEYS: label=_("Button shortcuts"); hint=_("See the current button assignments"); break;
        }
        add(more,item->commandId,item->commandParam,label,hint);
    }
    more->reconfigure(0); root->reconfigure(0);
    return root;
}
#endif
