#include "reader_ui.h"
//
// C++ Implementation: pb_toc
//

#include <crgui.h>
#include <cri18n.h>
#include <math.h>
#include "viewdlg.h"
#include "mainwnd.h"
#include "cr3pocketbook.h"
#include "pb_toc.h"

bool CRTocMenu::onCommand( int command, int params )
{
    CRLog::trace("CRTocMenu::onCommand(%d, %d)", command, params);
    return CRMenu::onCommand( command, params );
}

CRTocMenu::CRTocMenu( CRGUIWindowManager * wm, CRPropRef newProps, int id,
    CRGUIAcceleratorTableRef menuAccelerators, lvRect &rc, tocentry *tocItems,
    int length, int currentPage )
: CRFullScreenMenu( wm, id, lString32(_("Contents")), 8, rc ),
  props( newProps ),
  _menuAccelerators( menuAccelerators ), _menuItemId(mm_TouchTOC)
{
    CRLog::trace("showTocTouchMenu(): Construct");

    if( !fontAntiAliasingActivated() ) {
        forcePartialBwUpdates = true;
    }

    setSkinName(U"#touch-toc");
    styleReaderMenu(this);

    _fullscreen = true;

    LVFontRef valueFont( fontMan->GetFont(
        VALUE_FONT_SIZE,
        400,
        true,
        css_ff_sans_serif,
        lString8("Arial")
        ) );

    CRMenu * mainMenu = this;
    mainMenu->setAccelerators( _menuAccelerators );

    _selectedItem = 0;

    if ( length ) {

        for (int i = 0; i < length; i++) {
            if( currentPage >= tocItems[i].position ) {
                _selectedItem = i;
            }
        }

        for (int i = 0; i < length; i++) {

            lString32 text(tocItems[i].text);

            // Prepend page number
            // text = U"[" + lString32::itoa(tocItems[i].position) + U"] " + text;

            // Mark current chapter
            if( _selectedItem == i ) {
                text = U">  " + text;
            }

            // Indent level
            for( int level = tocItems[i].level; level > 1; level-- ) {
                text = U"     " + text;
            }

            mainMenu->addItem(new CRMenuItem(
                mainMenu,
                PB_TOC_SAFE_CMD_RANGE + tocItems[i].page,
                UnicodeToUtf8(text).c_str(),
                LVImageSourceRef(),
                LVFontRef()/*,
                lString32::itoa(tocItems[i].position).c_str()*/
                ));
        }
    }

    // Empty contents - shouldn't reach this
    else {
        mainMenu->addItem( new CRMenuItem(
            mainMenu,
            PB_TOC_SAFE_CMD_RANGE + 1,
            _("Empty TOC"),
            LVImageSourceRef(),
            LVFontRef()
            ) );
    }

    reconfigure(0);

    setCurPage( (int)floor(_selectedItem/_pageItems) );

    if( forcePartialBwUpdates ) {
        SetWeakTimer("FullUpdate", FullUpdate, 150);
    }

}
