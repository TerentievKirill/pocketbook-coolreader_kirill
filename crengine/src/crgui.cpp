/*******************************************************

   CoolReader Engine

   lvxml.cpp:  XML parser implementation

   (c) Vadim Lopatin, 2000-2006
   This source code is distributed under the terms of   GNU General Public License
   See LICENSE file for details

*******************************************************/

#include <stdlib.h>
#include <algorithm>
//include <unistd.h>      /* pause() */
#include "../include/crgui.h"
#include "../include/crtrace.h"
#include "../include/cri18n.h"

//TODO: place to skin file
#define ITEM_MARGIN 8
#define HOTKEY_SIZE 36
#define MENU_NUMBER_FONT_SIZE 24
#define SCROLL_HEIGHT 34
#define DEF_FONT_SIZE 22
#define DEF_TITLE_FONT_SIZE 28

// if 1, full page (e.g. 8 items) is scrolled even if on next page would be less items (show empty space)
#define FULL_SCROLL 1

extern lString32 pbSkinFileName;

const char * cr_default_skin =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<CR3Skin>\n"
"  <menu id=\"main\">\n"
"        <text color=\"#000000\" face=\"Arial\" size=\"25\" bold=\"true\" italic=\"false\" valign=\"center\" halign=\"center\"/>\n"
"        <background color=\"#AAAAAA\"/>\n"
"        <border widths=\"0,8,8,8\"/>\n"
"        <!--icon image=\"filename\" valign=\"\" halign=\"\"/-->\n"
"        <title>\n"
"            <size minvalue=\"32,0\" maxvalue=\"0,0\"/>\n"
"            <text color=\"#000000\" face=\"Arial\" size=\"25\" bold=\"true\" italic=\"false\" valign=\"center\" halign=\"center\"/>\n"
"            <background color=\"#AAAAAA\"/>\n"
"            <border widths=\"4,4,4,4\"/>\n"
"            <!--icon image=\"filename\" valign=\"\" halign=\"\"-->\n"
"        </title>\n"
"        <item>\n"
"            <size minvalue=\"48,48\" maxvalue=\"0,0\"/>\n"
"            <text color=\"#000000\" face=\"Arial\" size=\"24\" bold=\"false\" italic=\"false\" valign=\"center\" halign=\"left\"/>\n"
"            <background image=\"std_menu_item_background.xpm\" color=\"#FFFFFF\"/>\n"
"            <border widths=\"6,6,6,6\"/>\n"
"            <!--icon image=\"filename\" valign=\"\" halign=\"\"-->\n"
"        </item>\n"
"        <shortcut>\n"
"            <size minvalue=\"48,48\" maxvalue=\"0,0\"/>\n"
"            <text color=\"#000000\" face=\"Arial\" size=\"24\" bold=\"false\" italic=\"false\" valign=\"center\" halign=\"center\"/>\n"
"            <background image=\"std_menu_shortcut_background.xpm\" color=\"#FFFFFF\"/>\n"
"            <border widths=\"6,6,6,6\"/>\n"
"            <!--icon image=\"filename\" valign=\"\" halign=\"\"-->\n"
"        </shortcut>\n"
"  </menu>\n"
"  <menu id=\"settings\">\n"
"        <text color=\"#000000\" face=\"Arial\" size=\"25\" bold=\"true\" italic=\"false\" valign=\"center\" halign=\"center\"/>\n"
"        <background color=\"#AAAAAA\"/>\n"
"        <border widths=\"8,8,8,8\"/>\n"
"        <!--icon image=\"filename\" valign=\"\" halign=\"\"/-->\n"
"        <title>\n"
"            <size minvalue=\"0,40\" maxvalue=\"0,0\"/>\n"
"            <text color=\"#000000\" face=\"Arial\" size=\"28\" bold=\"true\" italic=\"false\" valign=\"center\" halign=\"center\"/>\n"
"            <background color=\"#AAAAAA\"/>\n"
"            <border widths=\"4,4,4,4\"/>\n"
"            <!--icon image=\"filename\" valign=\"\" halign=\"\"-->\n"
"        </title>\n"
"        <item>\n"
"            <size minvalue=\"48,48\" maxvalue=\"0,0\"/>\n"
"            <text color=\"#000000\" face=\"Arial\" size=\"24\" bold=\"false\" italic=\"false\" valign=\"center\" halign=\"left\"/>\n"
"            <background image=\"std_menu_item_background.xpm\" color=\"#FFFFFF\"/>\n"
"            <border widths=\"6,6,6,6\"/>\n"
"            <!--icon image=\"filename\" valign=\"\" halign=\"\"-->\n"
"        </item>\n"
"        <shortcut>\n"
"            <size minvalue=\"48,48\" maxvalue=\"0,0\"/>\n"
"            <text color=\"#000000\" face=\"Arial\" size=\"24\" bold=\"false\" italic=\"false\" valign=\"center\" halign=\"center\"/>\n"
"            <background image=\"std_menu_shortcut_background.xpm\" color=\"#FFFFFF\"/>\n"
"            <border widths=\"6,6,6,6\"/>\n"
"            <!--icon image=\"filename\" valign=\"\" halign=\"\"-->\n"
"        </shortcut>\n"
"  </menu>\n"
"</CR3Skin>\n";

bool CRGUIWindowManager::loadSkin( lString32 pathname )
{
    CRSkinRef skin;
    if ( !pathname.empty() )
        skin = LVOpenSkin( pathname );
    if ( skin.isNull() ) {
        skin = LVOpenSimpleSkin( lString8( cr_default_skin ) );
        setSkin( skin );
        return false;
    }
    setSkinFilePath(pathname);
    setSkin( skin );
    loadSkinKeymaps();

    const char *basename = strrchr(UnicodeToUtf8(pathname).c_str(), '/');
    basename = basename ? basename+1 : UnicodeToUtf8(pathname).c_str();
    pbSkinFileName = lString32(basename);

    return true;
}


/// add all items from another table
void CRGUIAcceleratorTable::addAll( const CRGUIAcceleratorTable & v )
{
	for ( int i=0; i<v._items.length(); i++ ) {
		CRGUIAccelerator * item = v._items.get( i );
		add( item->keyCode, item->keyFlags, item->commandId, item->commandParam );
	}
}

/// add accelerator to table or change existing
bool CRGUIAcceleratorTable::add( int keyCode, int keyFlags, int commandId, int commandParam )
{
    int index = indexOf( keyCode, keyFlags );
    if ( index >= 0 ) {
        // just update
        CRGUIAccelerator * item = _items[index];
        item->commandId = commandId;
        item->commandParam = commandParam;
        return false;
    }
    CRGUIAccelerator * item = new CRGUIAccelerator();
    item->keyCode = keyCode;
    item->keyFlags = keyFlags;
    item->commandId = commandId;
    item->commandParam = commandParam;
    _items.add(item);
    return true;
}

CRGUIAcceleratorTableRef CRGUIAcceleratorTableList::get( const lString32 & name, CRPropRef keyRemappingOptions )
{
    CRGUIAcceleratorTableRef prev = get(name);
    if ( !prev )
        return prev;
    CRPropRef keymaps = keyRemappingOptions->getSubProps(LCSTR(cs32("keymap.") + name + "."));
    if ( keymaps.isNull() || keymaps->getCount()==0 )
        return prev;
    CRGUIAcceleratorTableRef acc( new CRGUIAcceleratorTable( *prev ));
    for ( int i=0; i<keymaps->getCount(); i++ ) {
        lString32 name( keymaps->getName(i) );
        lString32 value = keymaps->getValue(i);
//        CRLog::trace("Override key map: %s -> %s", LCSTR(name), LCSTR(value) );
        int key, flags;
        int cmd, params;
        if ( !splitIntegerList( name, cs32("."), key, flags ))
            continue;
        if ( !splitIntegerList( value, cs32(","), cmd, params ))
            continue;
        acc->add(key, flags, cmd, params);
    }
    return acc;
}

/// add all tables
void CRGUIAcceleratorTableList::addAll( const CRGUIAcceleratorTableList & v )
{
	LVHashTable<lString32, CRGUIAcceleratorTableRef>::iterator i( v._table );
	for ( ;; ) {
		LVHashTable<lString32, CRGUIAcceleratorTableRef>::pair * p = i.next();
		if ( !p )
			break;
		CRGUIAcceleratorTableRef t = _table.get( p->key );
		if ( t.isNull() ) {
			t = CRGUIAcceleratorTableRef( new CRGUIAcceleratorTable() );
			_table.set( p->key, t );
		}
		crtrace trace;
		trace << "Merging accelerators for '"  << p->key << "'";
		t->addAll( *p->value );
	}
}

/// returns true if key is processed
bool CRGUIWindowManager::onKeyPressed( int key, int flags )
{
    // orientation related key substitution
    const char * orientation_mapping_name = NULL;
    if ( _orientation==CR_ROTATE_ANGLE_90 )
        orientation_mapping_name = "key-substitution-90";
    else if ( _orientation==CR_ROTATE_ANGLE_180 )
        orientation_mapping_name = "key-substitution-180";
    else if ( _orientation==CR_ROTATE_ANGLE_270 )
        orientation_mapping_name = "key-substitution-270";
    if ( orientation_mapping_name ) {
        CRGUIAcceleratorTableRef subst = _accTables.get(orientation_mapping_name);
        if ( !subst.isNull() ) {
            const CRGUIAccelerator * acc = subst->findKeyAccelerator(key, flags);
            if ( acc ) {
                CRLog::debug("Translation using %s : %d, %d -> %d, %d", orientation_mapping_name,
                             acc->keyCode, acc->keyFlags, acc->commandId, acc->commandParam);
                key = acc->commandId;
                flags = acc->commandParam;
            }
        }
    }
    CRLog::trace("CRGUIWindowManager::onKeyPressed( %d, %d)", key, flags );
    for ( int i=_windows.length()-1; i>=0; i-- ) {
        if ( _windows[i]->isVisible() ) {
            if ( _windows[i]->onKeyPressed( key, flags ) ) {
                CRLog::trace("CRGUIWindowManager::onKeyPressed() -- window %d has processed key, exiting", i );
                return true;
            } else {
                CRLog::trace("CRGUIWindowManager::onKeyPressed() -- window %d cannot process key, continue", i );
            }
        } else {
            CRLog::trace("CRGUIWindowManager::onKeyPressed() -- window %d is invisible, continue", i );
        }
    }
    return false;
}

bool CRGUIWindowManager::onTouch( int x, int y, CRGUITouchEventType evType )
{
    if (_ignoreTillUp) {
        if (evType == CRTOUCH_UP)
            _ignoreTillUp = false;
        return false;
    }
    CRGUIWindow * topWin = getTopVisibleWindow();
    for ( int i=_windows.length()-1; i>=0; i-- ) {
        if ( _windows[i]->isVisible() ) {
            if ( _windows[i]->onTouch( x, y, evType ) ) {
                CRLog::trace("CRGUIWindowManager::onTouch() -- window %d has processed the event, exiting", i );
                if (topWin != _windows[i])
                    activateWindow(_windows[i]);
                _ignoreTillUp = (evType == CRTOUCH_DOWN_LONG);
                return true;
            } else {
                CRLog::trace("CRGUIWindowManager::onTouch() -- window %d cannot process the event, continue", i );
            }
        } else {
            CRLog::trace("CRGUIWindowManager::onTouch() -- window %d is invisible, continue", i );
        }
    }
    return false;
}

/// changes screen size and orientation
void CRGUIWindowManager::reconfigure( int dx, int dy, cr_rotate_angle_t orientation )
{
    CRLog::info("CRGuiWindowManager::reconfigure( dx=%d, dy=%d, angle=%d", dx, dy, (int)orientation);
    int flags = 0;
    lvRect fullRect = _screen->getRect();
    if ( fullRect.width()!=dx || fullRect.height()!=dy )
        flags |= CRGUI_CONFIGURE_FLAG_SCREEN_SIZE;
    if ( orientation!=_orientation ) {
        flags |= CRGUI_CONFIGURE_FLAG_SCREEN_ORIENTATION;
        _orientation = orientation;
    }
    if ( !flags )
        return;
    if ( _screen->setSize( dx, dy ) ) {
        reconfigureWindows( flags );
        postEvent( new CRGUIUpdateEvent(true) );
    }
}

/// redraw one window
void CRGUIWindowManager::updateWindow( CRGUIWindow * window )
{
    int index = _windows.indexOf( window );
    if ( index < 0 )
        return;
    lvRect coverBox;
    if  ( _windows.empty() )
        return;
    LVPtrVector<CRGUIWindow, false> drawList;
    for ( int i=_windows.length()-1; i>=index; i-- ) {
        if ( !_windows[i]->isVisible() )
            continue;
        lvRect rc = _windows[i]->getRect();
        if ( coverBox.isRectInside( rc ) )
            continue; // fully covered by top window
        if ( !rc.isEmpty() )
            drawList.add( _windows[i] );
        if ( !rc.isRectInside( coverBox ) )
            coverBox = rc;
    }
    while ( !drawList.empty()  ) {
        CRGUIWindow * w = drawList.pop();
        if ( w->isDirty() ) {
            if ( w->isVisible() )
                w->flush();
            _screen->invalidateRect( w->getRect() );
        }
    }
/// invalidates rectangle: add it to bounding box of next partial update
    _screen->flush( false );
}

void CRGUIWindowManager::update( bool fullScreenUpdate, bool forceFlushScreen )
{
    lvRect coverBox;
    if  ( _windows.empty() )
        return;
    LVPtrVector<CRGUIWindow, false> drawList;
    for ( int i=_windows.length()-1; i>=0; i-- ) {
        if ( !_windows[i]->isVisible() )
            continue;
        lvRect rc = _windows[i]->getRect();
        if ( coverBox.isRectInside( rc ) )
            continue; // fully covered by top window
        if ( !rc.isEmpty() )
            drawList.add( _windows[i] );
        if ( !rc.isRectInside( coverBox ) )
            coverBox = rc;
    }
    while ( !drawList.empty()  ) {
        CRGUIWindow * w = drawList.pop();
        if (fullScreenUpdate) w->requestFullRedraw();
        if ( w->isDirty() || fullScreenUpdate ) {
            if ( w->isVisible() )
                w->flush();
            _screen->invalidateRect( w->getRect() );
        }
    }
    _lastProgressPercent = -1;
    if ( !forceFlushScreen ) {
#if 0
        // simulate slow rendering
        sleep(1);
#endif
        forwardSystemEvents(false);
        if ( !_events.empty() ) {
            // postpone screen update
            postEvent( new CRGUIUpdateEvent(fullScreenUpdate) );
            return;
        }
    }
    _screen->flush( fullScreenUpdate );
}

/// closes window, removes from stack, destroys object
void CRGUIWindowManager::closeWindow( CRGUIWindow * window )
{
    int index = _windows.indexOf( window );
    if ( index >= 0 ) {
        if ( window == _windows.peek() )
            window->covered(); // send cover before close
        _windows.remove( index );
    }
    window->closing();
    delete window;
    for ( int i=0; i<_windows.length() && (index<0 || i<index); i++ )
        _windows[i]->setDirty();
    CRGUIWindow * gotFocus = getTopVisibleWindow();
    if (gotFocus)
        gotFocus->reactivated();
    fontMan->gc();
}

/// activates window, brings it on top; add to stack if not added
void CRGUIWindowManager::activateWindow( CRGUIWindow * window )
{
    int index = _windows.indexOf( window );
    CRGUIWindow * lostFocus = getTopVisibleWindow();
    window->setVisible( true );
    if ( index < 0 ) {
        _windows.push( window );
    } else if ( index < _windows.length() - 1 ) {
        _windows.push( _windows.remove( index ) );
    }
    if ( window != lostFocus )
    {
        if ( lostFocus )
            lostFocus->covered();
        window->activated();
    } else
        window->reactivated();
}

/// runs event loop
int CRGUIWindowManager::runEventLoop()
{
    while ( !_stopFlag ) {
        if ( !getWindowCount() )
            _stopFlag = true;
        handleAllEvents(!_stopFlag);
    }
    return 0;
}


/// handle all events from queue
bool CRGUIWindowManager::handleAllEvents( bool waitForEvent )
{
    bool handled = false;
    if ( _events.empty() && waitForEvent ) {
        idle();
        forwardSystemEvents( true );
    }
    for (CRGUIEvent * event=getEvent(); event; event=getEvent() ) {
        handleEvent( event );
        delete event;
        handled = true;
    }
    return handled;
}

void CRGUIWindowManager::postCommand( int command, int params )
{
    postEvent(new CRGUICommandEvent(command, params) );
}

void CRGUIWindowManager::postEvent( CRGUIEvent * event )
{
    int evt = event->getType();
    if ( evt==CREV_KEYDOWN || evt==CREV_KEYUP || evt==CREV_COMMAND || evt == CREV_TOUCH ) {
        // for window events, like keyPress or Command, post them before Update/Resize
        int i=_events.length()-1;
        for ( ; i>=0; i-- ) {
            int t = _events[i]->getType();
            if ( t!=CREV_UPDATE && t!=CREV_RESIZE )
                break;
        }
        _events.insert(i+1, event);
        return;
    } else if ( evt==CREV_UPDATE || evt==CREV_RESIZE ) {
        for ( int i=_events.length()-1; i>=0; i-- ) {
            int t = _events[i]->getType();
            if ( t==evt || t==CREV_UPDATE ) { // UPDATE is invalidated by Resize
                if ( t==CREV_UPDATE && evt==CREV_UPDATE && _events[i]->getParam1()!=0 )
                    event->setParam1(1); // don't miss fullScreen flag from removed events
                // remove duplicates
                delete _events.remove(i);
            }
        }
        _events.push(event);
        return;
    }
    _events.push(event);
}

/// override to handle
bool CRGUIWindowManager::handleEvent( CRGUIEvent * event )
{
    // by default, allow event to do something with window
    if ( event->isWMEvent() )
        return event->handle( this );
    if ( event->isWindowEvent() ) {
        CRLog::trace("CRGUIWindowManager::handleEvent( %d, %d, %d)", event->getType(), event->getParam1(), event->getParam2() );
        for ( int i=_windows.length()-1; i>=0; i-- ) {
            if ( !event->isForVisibleOnly() || _windows[i]->isVisible() ) {
                if ( _windows[i]->handleEvent(event) ) {
                    CRLog::trace("CRGUIWindowManager::handleEvent() -- window %d has processed event, exiting", i );
                    return true;
                } else {
                    CRLog::trace("CRGUIWindowManager::handleEvent() -- window %d cannot process event, continue", i );
                    if ( event->isForModalOnly() )
                        return false;
                }
            } else {
                CRLog::trace("CRGUIWindowManager::handleEvent() -- window %d is invisible, continue", i );
            }
        }
        return false;
    }
	return false;
}


static bool firstWaitUpdate = true;

/// draws icon at center of screen
void CRGUIWindowManager::showWaitIcon( lString32 filename, int progressPercent )
{
    LVImageSourceRef img = _skin->getImage( filename );
    if ( !img.isNull() ) {
        int dx = img->GetWidth();
        int dy = img->GetHeight();
        int x = (_screen->getWidth() - dx) / 2;
        int y = (_screen->getHeight() - dy) / 2;
        CRLog::debug("Drawing wait image %s [%d,%d %dx%d]  progress=%d%%", UnicodeToUtf8(filename).c_str(), x, y, dx, dy, progressPercent );
        _screen->getCanvas()->Draw( img, x, y, dx, dy, true );
        int gaugeH = 0;
        if ( progressPercent>=0 && progressPercent<=100 ) {
            CRScrollSkinRef skin = _skin->getScrollSkin(U"#progress");
            if ( !skin.isNull() ) {
                CRLog::trace("Drawing gauge %d%%", progressPercent);
                gaugeH = 16;
                lvRect gaugeRect( x, y+dy, x+dx, y+dy+gaugeH );
                skin->drawGauge( *(_screen->getCanvas()), gaugeRect, progressPercent );
            }
        }
        _screen->invalidateRect( lvRect(x, y, x+dx, y+dy+gaugeH) );
        _screen->flush(firstWaitUpdate);
        firstWaitUpdate = (progressPercent == 100);
    } else {
        CRLog::error("CRGUIWindowManager::showWaitIcon(%s): image not found in current skin", UnicodeToUtf8(filename).c_str() );
    }
}

#define PROGRESS_UPDATE_INTERVAL 5
/// draws icon at center of screen, with optional progress gauge
void CRGUIWindowManager::showProgress( lString32 filename, int progressPercent )
{
    time_t t = (time_t)time((time_t)0);
    if ( t<_lastProgressUpdate+PROGRESS_UPDATE_INTERVAL || progressPercent==_lastProgressPercent )
        return;
    showWaitIcon( filename, progressPercent );
    _lastProgressUpdate = t;
    _lastProgressPercent = progressPercent;
}

void CRGUIWindowManager::setSkin(CRSkinRef skin)
{
    bool needUpdate = !_skin.isNull();
    _skin = skin;
    if (needUpdate) {
        reconfigureWindows( 0 );
        fontMan->gc();
        postEvent( new CRGUIUpdateEvent(true) );
    }
}

bool CRGUIWindowManager::setSkin( lString32 name )
{
    CRSkinListItem *item = _skins.findById(name);
    if (NULL!=item ) {
        return loadSkin(item->getFileName());
    }
    return false;
}

void CRGUIScreenBase::flush( bool full )
{
    if ( _updateRect.isEmpty() && !full && !getTurboUpdateEnabled() ) {
        CRLog::trace("CRGUIScreenBase::flush() - update rectangle is empty");
        return;
    }
    if ( !_front.isNull() && !_updateRect.isEmpty() && !full ) {
        // calculate really changed area
        lvRect rc;
        lvRect lineRect(_updateRect);
        int sz = _canvas->GetRowSize();
        for ( int y = _updateRect.top; y < _updateRect.bottom; y++ ) {
            if ( y>=0 && y<_height ) {
                void * line1 = _canvas->GetScanLine( y );
                void * line2 = _front->GetScanLine( y );
                if (memcmp( line1, line2, sz ) != 0) {
                    // line content is different
                    lineRect.top = y;
                    lineRect.bottom = y+1;
                    rc.extend( lineRect );
                    // copy line to front buffer
                    memcpy( line2, line1, sz );
                }
            }
        }
        if ( rc.isEmpty() ) {
            // no actual changes
            _updateRect.clear();
            return;
        }
        _updateRect.top = rc.top;
        _updateRect.bottom = rc.bottom;
    }
    //if ( !full && !checkFullUpdateCounter() )
    //    full = false;
    if ( full && !_front.isNull() ) {
        // copy full screen to front buffer
        _canvas->DrawTo( _front.get(), 0, 0, 0, NULL );
    }
    if ( full )
        _updateRect = getRect();
    update( _updateRect, full );
    _updateRect.clear();
}

/// calculates title rectangle for specified window rectangle
bool CRGUIWindowBase::getTitleRect( lvRect & rc )
{
    rc = _rect;
    CRWindowSkinRef skin = getSkin();
    if ( skin.isNull() ) {
        rc.bottom = rc.top;
        return false;
    }
    rc.shrinkBy(skin->getBorderWidths());
    rc.bottom = rc.top;
    CRRectSkinRef titleSkin = skin->getTitleSkin();
    if ( !titleSkin.isNull() ) {
        rc.bottom += titleSkin->getMinSize().y;
    }
    return !rc.isEmpty();
}

/// calculates status rectangle for specified window rectangle
bool CRGUIWindowBase::getStatusRect( lvRect & rc )
{
    rc = _rect;
    CRWindowSkinRef skin = getSkin();
    if ( skin.isNull() ) {
        rc.bottom = rc.top;
        return false;
    }
    rc.shrinkBy(skin->getBorderWidths());
    rc.top = rc.bottom;
    lvPoint scrollSize = getMinScrollSize( _page, _pages );
    int h = scrollSize.y;
    CRRectSkinRef statusSkin = skin->getStatusSkin();
    CRScrollSkinRef sskin = skin->getScrollSkin();
    bool scroll = ( !sskin.isNull() && sskin->getLocation()==CRScrollSkin::Status );
    if ( !statusSkin.isNull() ) {
        rc.top -= statusSkin->getMinSize().y;
    }
    if ( scroll && rc.height()<h ) {
        rc.top = rc.bottom - h;
    }
    return !rc.isEmpty();
}

/// calculates client rectangle for specified window rectangle
bool CRGUIWindowBase::getClientRect( lvRect & rc )
{
    rc = _rect;
    CRWindowSkinRef skin = getSkin();
    if ( skin.isNull() ) {
        rc.bottom = rc.top;
        return false;
    }
    rc.shrinkBy(skin->getBorderWidths());
    rc.bottom = rc.top;
    lvRect titleRect;
    getTitleRect( titleRect );
    lvRect statusRect;
    getStatusRect( statusRect );
    rc.top = titleRect.bottom;
    rc.bottom = statusRect.top;
    return !rc.isEmpty();
}

/// formats scroll label (like "1 of 2")
lString32 CRGUIWindowBase::getScrollLabel( int page, int pages )
{
    return lString32::itoa(page) << " of " << fmt::decimal(pages);
}

/// calculates minimum scroll size
lvPoint CRGUIWindowBase::getMinScrollSize( int page, int pages )
{
    lvPoint sz(0,0);
    CRWindowSkinRef skin = getSkin();
    if ( skin.isNull() ) {
        return sz;
    }
    CRScrollSkinRef sskin = skin->getScrollSkin();
    if ( !sskin.isNull() ) {
		LVFontRef sf = sskin->getFont();
        int h = sf.isNull() ? sskin->getFontSize() : sf->getHeight();
        int w = 0;
        bool noData = sskin->getAutohide() && pages<=1;
        lString32 label = getScrollLabel( page, pages );
        if ( !label.empty() )
            w = sskin->getFont()->getTextWidth(label.c_str(), label.length());
        if ( !sskin->getBottomTabSkin().isNull() ) {
            if ( h < sskin->getBottomTabSkin()->getMinSize().y )
                h = sskin->getBottomTabSkin()->getMinSize().y;
            if ( !noData && w < _rect.width()/4 )
                w = _rect.width()/4;
        }
        if ( !noData && h < sskin->getMinSize().y )
            h = sskin->getMinSize().y;
        if ( !sskin->getHBody().isNull() ) {
            if ( h < sskin->getHBody()->GetHeight() )
                h = sskin->getHBody()->GetHeight();
            if ( !noData && w < _rect.width()/4 )
                w = _rect.width()/4;
        }
        if ( h && w ) {
            sz.y = h;
            sz.x = w;
        }
    }
    return sz;
}

#define BATTERY_ICON_SIZE 60
/// calculates scroll rectangle for specified window rectangle
bool CRGUIWindowBase::getScrollRect( lvRect & rc )
{
    rc = _rect;
    CRWindowSkinRef skin = getSkin();
    if ( skin.isNull() ) {
        rc.bottom = rc.top;
        return false;
    }
    rc.shrinkBy(skin->getBorderWidths());
    CRScrollSkinRef sskin = skin->getScrollSkin();
    if ( sskin.isNull() )
        return false;
    lvPoint scrollSize = getMinScrollSize( _page, _pages );
    int h = scrollSize.y;
    if ( sskin->getLocation()==CRScrollSkin::Title ) {
        rc.bottom = rc.top;
        CRRectSkinRef titleSkin = skin->getTitleSkin();
        if ( !titleSkin.isNull() ) {
            getTitleRect( rc );
            lvRect rc2;
            sskin->getRect( rc2, rc );
            if ( rc2.width() < rc.width()/2 )
                rc = rc2;
            else
                rc.left = rc.right - rc.width()/4;
            rc.left -= BATTERY_ICON_SIZE;
            rc.right -= BATTERY_ICON_SIZE;
        } else if ( !sskin.isNull() ) {
            rc.bottom += h;
        }
    } else {
        rc.top = rc.bottom;
        CRRectSkinRef statusSkin = skin->getStatusSkin();
        if ( !statusSkin.isNull() ) {
            if ( statusSkin->getMinSize().y > h )
                h = statusSkin->getMinSize().y;
        }
        rc.top -= h;
    }
    return !rc.isEmpty();
}

bool CRGUIWindowBase::onTouch( int x, int y, CRGUITouchEventType evType)
{
    lvPoint pt = lvPoint(x, y);
    lvRect rc;
    rc = _rect;
    CRWindowSkinRef skin = getSkin();
    if ( skin.isNull() ) {
        rc.shrinkBy(skin->getBorderWidths());
    }
    if (rc.isPointInside(pt)) {
        CRGUIEvent * event = new CRGUITouchEvent( x, y, evType);
        event->setTargetWindow(this);
        _wm->postEvent( event );
        return true;
    }
    return false;
}

int zOrderComparator(const CRGUIControl ** item1, const CRGUIControl ** item2 )
{
    const CRGUIControl *a = *item1;
    const CRGUIControl *b = *item2;
    if ( a->getZorder()>b->getZorder() )
        return -1;
    if ( a->getZorder()<b->getZorder() )
        return 1;
    return 0;
}

bool CRGUIWindowBase::onTouchEvent(int x, int y, CRGUITouchEventType evType)
{
    lvPoint pt(x, y);

    if (_sortZorder && _controlsCreated) {
        _controls.sort(zOrderComparator);
        _sortZorder = false;
    }
    for (int i = 0; i < _controls.length(); i++) {
        CRGUIControl *control = _controls.get(i);
        if (control->hitTest(pt)) {
            bool upd = control != _selectedControl;
            if (upd) {
                upd = _selectedControl ? _selectedControl->setSelected(false) : false;
                _selectedControl = control;
                upd = _selectedControl->setSelected(true) || upd;
            }
            if (control->isEnabled())
                return control->onTouch(evType) || upd;
            return upd;
        }
    }
    if (_selectedControl) {
        bool ret = _selectedControl->setSelected(false);
        _selectedControl = NULL;
        return ret;
    }
    return false;
}

int CRGUIWindowBase::getTapZone(int x, int y, CRPropRef props)
{
    lvRect rc;

    getClientRect(rc);

    int dx = rc.width();
    int dy = rc.height();

    int x1 = rc.left + dx * props->getIntDef(PROP_TAP_ZONE_WIDTH_1, 33) / 100;
    int x2 = x1 + dx * props->getIntDef(PROP_TAP_ZONE_WIDTH_2, 33) / 100;
    int y1 = rc.top + dy * props->getIntDef(PROP_TAP_ZONE_HEIGHT_1, 33) / 100;
    int y2 = y1 + dy * props->getIntDef(PROP_TAP_ZONE_HEIGHT_2, 33) / 100;
    int zone = 0;
    if ( y<y1 ) {
        if ( x<x1 )
            zone = 1;
        else if ( x<x2 )
            zone = 2;
        else
            zone = 3;
    } else if ( y<y2 ) {
        if ( x<x1 )
            zone = 4;
        else if ( x<x2 )
            zone = 5;
        else
            zone = 6;
    } else {
        if ( x<x1 )
            zone = 7;
        else if ( x<x2 )
            zone = 8;
        else
            zone = 9;
    }
    return zone;
}

/// calculates input box rectangle for window rectangle
bool CRGUIWindowBase::getInputRect( lvRect & rc )
{
    rc = _rect;
    CRWindowSkinRef skin = getSkin();
    if ( skin.isNull() ) {
        rc.bottom = rc.top;
        return false;
    }
    //rc.shrinkBy(skin->getBorderWidths());
    CRRectSkinRef inputSkin = skin->getInputSkin();
    CRRectSkinRef statusSkin = skin->getStatusSkin();
    if ( inputSkin.isNull() || statusSkin.isNull() )
        return false;
    lvRect rc2;
    if ( !getStatusRect( rc2 ) )
        return false;
    inputSkin->getRect( rc, rc2 );
    return !rc.isEmpty();
}

/// draw input box, if any
void CRGUIWindowBase::drawInputBox()
{
    CRWindowSkinRef skin = getSkin();
    if ( skin.isNull() ) {
        return;
    }
    LVDrawBuf & buf = *_wm->getScreen()->getCanvas();
    CRRectSkinRef inputSkin = skin->getInputSkin();
    CRRectSkinRef statusSkin = skin->getStatusSkin();
    if ( inputSkin.isNull() || statusSkin.isNull() )
        return;
    lvRect rc2;
    if ( !getInputRect( rc2 ) )
        return;
    inputSkin->draw( buf, rc2 );
    if ( !_inputText.empty() )
        inputSkin->drawText(buf, rc2, _inputText );
}

/// draw status text
void CRGUIWindowBase::drawStatusText( LVDrawBuf & buf, const lvRect & rc, CRRectSkinRef skin )
{
    lvRect statusRc( rc );
    lvRect b = skin->getBorderWidths();
    statusRc.shrinkBy( b );
    if ( statusRc.width() > 100 ) {
        // draw status text
        skin->drawText( buf, statusRc, getStatusText() );
    }
}

/// draw status bar using current skin, with optional status text and scroll/tab/page indicator
void CRGUIWindowBase::drawStatusBar()
{
    CRWindowSkinRef skin = getSkin();
    if ( skin.isNull() ) {
        return;
    }
    LVDrawBuf & buf = *_wm->getScreen()->getCanvas();
    CRRectSkinRef statusSkin = skin->getStatusSkin();
    CRScrollSkinRef sskin = skin->getScrollSkin();
    lvRect statusRc;
    lvRect scrollRc;
    lvRect inputRc;
    if ( !getStatusRect( statusRc ) )
        return;
    getScrollRect( scrollRc );
    bool showInput = getInputRect( inputRc );
    if ( !statusSkin.isNull() ) {
        statusSkin->draw( buf, statusRc );
    }
    bool scroll = ( !sskin.isNull() && (sskin->getLocation()==CRScrollSkin::Status || hasTouchSettingsNavigation()) && !scrollRc.isEmpty() );
    if ( scroll ) {
        drawScroll(sskin, scrollRc, false, _page-1, _pages, 1 );
    }
    if ( !statusSkin.isNull() && !statusRc.isEmpty() && !getStatusText().empty() ) {
        if ( scroll ) {
            if ( scrollRc.left - statusRc.left > statusRc.right - scrollRc.right )
                statusRc.right = scrollRc.left;
            else
                statusRc.left = scrollRc.right;
        }
        if ( showInput ) {
            if ( inputRc.left - statusRc.left > statusRc.right - inputRc.right )
                statusRc.right = inputRc.left;
            else
                statusRc.left = inputRc.right;
        }
        drawStatusText( buf, statusRc, statusSkin );
    }
    drawInputBox();
}

// draws frame, title, status and client
void CRGUIWindowBase::draw()
{
    CRLog::trace("getting skin at CRGUIWindowBase::draw()");
    CRWindowSkinRef skin = getSkin();
    if ( skin.isNull() ) {
        return;
    }
    CRLog::trace("enter CRGUIWindowBase::draw()");
    LVDrawBuf & buf = *_wm->getScreen()->getCanvas();
    CRLog::trace("drawing window skin background at CRGUIWindowBase::draw()");
    skin->draw( buf, _rect );
    CRLog::trace("start drawing at CRGUIWindowBase::draw()");
    drawTitleBar();
    drawStatusBar();
    drawClient();
    _controlsCreated = true;
    CRLog::trace("exit CRGUIWindowBase::draw()");
}

static int settingsNavigationButtonWidth(const lvRect &rc)
{
    // Leave at least a button-height of room for the page counter.
    return std::min(rc.height() * 3, (rc.width() - rc.height()) / 2);
}

class CRSettingsNavigationButtonSkin : public CRButtonSkin
{
    CRRectSkinRef _title;
    bool _right;
public:
    CRSettingsNavigationButtonSkin(CRRectSkinRef title, bool right = false)
        : _title(title), _right(right) {}
    virtual void drawButton(LVDrawBuf &buf, const lvRect &rc, int flags = ENABLED)
    {
        _title->draw(buf, rc);
        lUInt32 color = _title->getTextColor();
        if (!(flags & ENABLED)) color = 0x999999;
        if ((flags & ENABLED) && (flags & PRESSED)) {
            buf.FillRect(rc, color);
            color ^= 0x00FFFFFF;
        }
        lvRect frame(rc);
        int inset = std::max(4, rc.height() / 12);
        frame.shrinkBy(lvRect(inset, inset, inset, inset));
        buf.DrawFrame(frame, color, 2);
        // A bold, symmetric chevron scales with header height, never target width.
        int halfHeight = rc.height() / 4;
        int stroke = rc.height() / 12;
        if (stroke < 2) stroke = 2;
        int left = rc.left + (rc.width() - halfHeight - stroke) / 2;
        int centerY = rc.top + rc.height() / 2;
        for (int y = -halfHeight; y <= halfHeight; ++y) {
            int x = left + (_right ? halfHeight - abs(y) : abs(y));
            buf.FillRect(x, centerY + y, x + stroke, centerY + y + 1, color);
        }
    }
};

static void settingsText(LVDrawBuf &buf, lvRect rc, lString32 text, LVFontRef font,
                         lUInt32 ink, bool center = false)
{
    LVDrawStateSaver state(buf);
    lvRect clip;
    buf.GetClipRect(&clip);
    rc.intersect(clip);
    if (rc.isEmpty() || font.isNull()) return;
    buf.SetClipRect(&rc);
    while (!text.empty() && (int)font->getTextWidth(text.c_str(), text.length()) > rc.width()) {
        if (text.endsWith(U"…")) text.erase(text.length()-1, 1);
        if (text.empty()) break;
        text.erase(text.length()-1, 1);
        if (!text.empty()) text << U"…";
    }
    int width = font->getTextWidth(text.c_str(), text.length());
    buf.SetTextColor(ink);
    font->DrawTextString(&buf, center ? rc.left + (rc.width()-width)/2 : rc.left,
                        rc.top + (rc.height()-font->getHeight())/2,
                        text.c_str(), text.length(), U'?');
}

class CRSettingsTextButtonSkin : public CRButtonSkin {
    LVFontRef _font;
    lString32 _text;
    lUInt32 _ink, _paper;
    bool _chosen, _enabled;
public:
    CRSettingsTextButtonSkin(LVFontRef font, lString32 text, lUInt32 ink, lUInt32 paper,
                             bool chosen = false, bool enabled = true)
        : _font(font), _text(text), _ink(ink), _paper(paper), _chosen(chosen), _enabled(enabled) {}
    virtual void drawButton(LVDrawBuf &buf, const lvRect &rect, int flags = ENABLED) {
        bool filled = _chosen || (flags & PRESSED);
        lUInt32 ink = _enabled ? _ink : 0x888888;
        lvRect rc(rect);
        rc.shrinkBy(lvRect(3,3,3,3));
        buf.FillRect(rc, filled ? ink : _paper);
        buf.DrawFrame(rc, ink, 1);
        rc.shrinkBy(lvRect(8,2,8,2));
        settingsText(buf, rc, _text, _font, filled ? _paper : ink, true);
    }
};

class CRSettingsButtonControl : public CRButtonControl {
    bool _enabled;
public:
    CRSettingsButtonControl(CRGUIWindowBase *parent, lvRect rc, CRButtonSkinRef skin,
                            int command, int param, bool enabled = true)
        : CRButtonControl(parent, rc, skin, param, command, param), _enabled(enabled) {}
    virtual bool isEnabled() const { return _enabled; }
    virtual bool onTouch(CRGUITouchEventType type) {
        // One action on release, including after a long hold.
        return _enabled && type != CRTOUCH_DOWN_LONG && CRButtonControl::onTouch(type);
    }
};

CRMenu *CRMenu::settingsRoot() const {
    const CRMenu *menu = this;
    while (menu->_menu) menu = menu->_menu;
    return menu->_settingsEditor ? const_cast<CRMenu *>(menu) : NULL;
}

void CRMenu::enableSettingsEditor(CRPropRef props, lString32 scope, int dpi) {
    _settingsEditor = true;
    _actionMenu = false;
    _props = props;
    _settingsScope = scope;
    _settingsDpi = std::max(120, std::min(600, dpi));
}

void CRMenu::enableActionMenu(lString32 scope, int dpi) {
    // Reuse the readable menu layout without staging any reader preferences.
    enableSettingsEditor(LVCreatePropsContainer(),scope,dpi);
    _actionMenu = true;
}

void CRMenu::captureSettingsBaseline() {
    CRMenu *root = settingsRoot();
    if (root) root->_settingsBaseline = LVClonePropsContainer(root->_props);
}

bool CRMenu::settingsChanged() const {
    CRMenu *root = settingsRoot();
    if (!root || root->_settingsBaseline.isNull()) return false;
    CRPropRef a = root->_settingsBaseline, b = root->_props;
    if (a->getCount() != b->getCount()) return true;
    for (int i=0; i<a->getCount(); ++i)
        if (strcmp(a->getName(i), b->getName(i)) || a->getValue(i) != b->getValue(i)) return true;
    return false;
}

LVFontRef CRMenu::settingsFont(bool value) {
    CRMenu *root = settingsRoot();
    if (!root) return LVFontRef();
    LVFontRef &font = value ? root->_settingsValueFont : root->_settingsLabelFont;
    if (font.isNull()) {
        CRRectSkinRef skin = root->getSkin()->getItemSkin();
        font = fontMan->GetFont(((value ? 32 : 42)*root->_settingsDpi+150)/300,
                               400, false, css_ff_sans_serif, UnicodeToUtf8(skin->getFontFace()));
    }
    return font;
}

bool CRMenu::hasInlineSetting() const {
    return isSettingsEditor() && !_propName.empty() && !_props.isNull() &&
        (_settingsStepper || _items.length() == 2);
}

void CRMenu::drawTitleBar() {
    if (!isSettingsEditor() || !_wm->getScreen()->isTouchSupported()) {
        CRGUIWindowBase::drawTitleBar();
        return;
    }
    LVDrawBuf &buf = *_wm->getScreen()->getCanvas();
    CRRectSkinRef skin = getSkin()->getTitleSkin();
    lvRect header;
    getTitleRect(header);
    lUInt32 ink = skin->getTextColor(), paper = skin->getBackgroundColor();
    buf.FillRect(header, paper);
    buf.FillRect(header.left, header.bottom-2, header.right, header.bottom, ink);
    LVFontRef font = settingsFont(true);
    lString32 left = (_menu || isActionMenu()) ? lString32(_("Back")) : lString32(_("Cancel"));
    lString32 done(isActionMenu() ? _("Read") : _("Done"));
    int width = std::max(header.height()*3/2, (int)font->getTextWidth(left.c_str(),left.length())+32);
    width = std::max(width, (int)font->getTextWidth(done.c_str(),done.length())+32);
    width = std::min(width, header.width()/3);
    lvRect back(header), apply(header), caption(header);
    back.right = back.left + width;
    apply.left = apply.right - width;
    back.shrinkBy(lvRect(8,12,8,12));
    apply.shrinkBy(lvRect(8,12,8,12));
    caption.left += width + 8; caption.right -= width + 8;
    CRButtonSkinRef backSkin(new CRSettingsTextButtonSkin(font,left,ink,paper));
    CRButtonSkinRef doneSkin(new CRSettingsTextButtonSkin(font,done,ink,paper,true));
    backSkin->drawButton(buf,back); doneSkin->drawButton(buf,apply);
    if (!_controlsCreated) {
        addControl(new CRSettingsButtonControl(this,back,backSkin,DCMD_BUTTON_PRESSED,BTN_CLOSE));
        addControl(new CRSettingsButtonControl(this,apply,doneSkin,DCMD_BUTTON_PRESSED,BTN_SETTINGS_DONE));
    }
    lvRect context(caption);
    context.bottom = context.top + font->getHeight() + 8;
    settingsText(buf,context,_menu ? _menu->getLabel() : settingsRoot()->_settingsScope,font,ink,true);
    caption.top = context.bottom;
    settingsText(buf,caption,_label,settingsFont(),ink,true);
}

void CRMenu::invalidateSettingsValues() {
    for (int i=0; i<_items.length(); ++i)
        if (_items[i]->isSettingsPreview()) _items[i]->setItemDirty();
    setDirty();
}

void CRMenu::updateSettingsValue(int item, int option, bool step) {
    if (item < 0 || item >= _items.length() || !_items[item]->isSubmenu()) return;
    CRMenu *menu = static_cast<CRMenu *>(_items[item]);
    if (!menu->hasInlineSetting()) return;
    lString8 name = UnicodeToUtf8(menu->_propName);
    lString32 current = menu->_props->getStringDef(name.c_str());
    int selected = option;
    if (step) {
        int value = current.atoi();
        selected = -1;
        for (int i=0; i<menu->_items.length(); ++i) {
            int candidate = menu->_items[i]->getPropValue().atoi();
            if ((option ? candidate > value : candidate < value) &&
                (selected < 0 || (option ? candidate < menu->_items[selected]->getPropValue().atoi()
                                         : candidate > menu->_items[selected]->getPropValue().atoi())))
                selected=i;
        }
    }
    if (selected < 0 || selected >= menu->_items.length()) return;
    if (current == menu->_items[selected]->getPropValue()) return;
    menu->_props->setString(name.c_str(),menu->_items[selected]->getPropValue());
    menu->setItemDirty();
    invalidateControls();
    _pageUpdate = true;
    invalidateSettingsValues();
}

void CRMenu::drawSettingsItem(CRMenuItem &item, LVDrawBuf &buf, lvRect rc, bool selected) {
    LVDrawStateSaver state(buf);
    lvRect oldClip;
    buf.GetClipRect(&oldClip);
    lvRect clip(rc); clip.intersect(oldClip); buf.SetClipRect(&clip);
    CRRectSkinRef skin = getSkin()->getClientSkin();
    lUInt32 paper = skin->getBackgroundColor(), ink = getSkin()->getItemSkin()->getTextColor();
    buf.FillRect(rc,paper);
    int pad = settingsFont(true)->getHeight()/2;
    rc.shrinkBy(lvRect(pad,6,pad,6));
    if (selected) buf.FillRect(rc.left-pad,rc.top+8,rc.left-pad+4,rc.bottom-8,ink);
    bool choices = !_propName.empty() && !item.isSubmenu();
    if (choices) {
        int size = settingsFont(true)->getHeight();
        lvRect mark(rc.left,rc.top+(rc.height()-size)/2,rc.left+size,rc.top+(rc.height()+size)/2);
        buf.DrawFrame(mark,ink,2);
        if (_props->getStringDef(UnicodeToUtf8(_propName).c_str()) == item.getPropValue()) {
            mark.shrinkBy(lvRect(7,7,7,7)); buf.FillRect(mark,ink);
        }
        rc.left += size+pad;
    }
    CRMenu *submenu = item.isSubmenu() ? static_cast<CRMenu *>(&item) : NULL;
    lString32 value = item.getSubmenuValue();
    bool inlineValue = submenu && submenu->hasInlineSetting();
    lvRect label(rc), detail(rc);
    LVFontRef labelFont = settingsFont();
    if (choices && (_propName == PROP_FONT_FACE || _propName == PROP_STATUS_FONT_FACE || _propName == PROP_FALLBACK_FONT_FACE))
        if (!item.getFont().isNull()) labelFont = item.getFont();
    lString32 text = item.getLabel();
    if (choices && (_propName == PROP_FONT_SIZE || _propName == PROP_STATUS_FONT_SIZE))
        text = item.getPropValue() + U" px";
    if (!value.empty() || inlineValue) {
        int gap = std::max(4, pad/2);
        int secondary = settingsFont(true)->getHeight();
        if (inlineValue) secondary = std::min(secondary*2,std::max(secondary,rc.height()-labelFont->getHeight()-gap));
        int height = labelFont->getHeight() + secondary + gap;
        label.top = rc.top + (rc.height()-height)/2;
        label.bottom = label.top + labelFont->getHeight();
        detail.top = label.bottom + gap;
        detail.bottom = rc.bottom;
    }
    if (submenu && !inlineValue) {
        label.right -= pad*2; detail.right -= pad*2;
        int half = std::max(5,pad/2), x=rc.right-pad, y=rc.top+rc.height()/2;
        for (int dy=-half; dy<=half; ++dy)
            buf.FillRect(x+half-abs(dy),y+dy,x+half-abs(dy)+3,y+dy+1,ink);
    }
    settingsText(buf,label,text,labelFont,ink);
    if (!inlineValue) {
        if (!value.empty()) settingsText(buf,detail,value,settingsFont(true),ink);
        return;
    }
    int index=0;
    while (index<_items.length() && _items[index]!=&item) ++index;
    int buttonHeight = std::min(detail.height(), settingsFont(true)->getHeight()*2);
    detail.bottom = detail.top + buttonHeight;
    int buttonWidth = submenu->_settingsStepper ? buttonHeight*3/2 : (detail.width()-pad)/2;
    lString32 current = submenu->_props->getStringDef(UnicodeToUtf8(submenu->_propName).c_str());
    for (int option=0; option<2; ++option) {
        lvRect button(detail);
        if (option) button.left = button.right-buttonWidth;
        else button.right = button.left+buttonWidth;
        lString32 caption = submenu->_settingsStepper ? lString32(option ? U"+" : U"−") : submenu->_items[option]->getLabel();
        bool enabled = true;
        if (submenu->_settingsStepper) {
            enabled = false;
            for (int i=0; i<submenu->_items.length(); ++i) {
                int candidate=submenu->_items[i]->getPropValue().atoi();
                if (option ? candidate>current.atoi() : candidate<current.atoi()) enabled=true;
            }
        }
        bool chosen = !submenu->_settingsStepper && current==submenu->_items[option]->getPropValue();
        CRButtonSkinRef buttonSkin(new CRSettingsTextButtonSkin(settingsFont(true),caption,ink,paper,chosen,enabled));
        buttonSkin->drawButton(buf,button);
        if (!_controlsCreated)
            addControl(new CRSettingsButtonControl(this,button,buttonSkin,
                submenu->_settingsStepper ? DCMD_SETTINGS_STEP : DCMD_SETTINGS_CHOICE,index*2+option,enabled));
    }
    if (submenu->_settingsStepper) {
        detail.left += buttonWidth+pad; detail.right -= buttonWidth+pad;
        settingsText(buf,detail,value,settingsFont(true),ink,true);
    }
}

/// draw title bar using current skin, with optional scroll/tab/page indicator
void CRGUIWindowBase::drawClient()
{
    CRWindowSkinRef skin = getSkin();
    if ( skin.isNull() ) {
        return;
    }
    LVDrawBuf & buf = *_wm->getScreen()->getCanvas();
    CRRectSkinRef clientSkin = skin->getClientSkin();
    if ( clientSkin.isNull() )
        return;
    lvRect rc;
    if ( !getClientRect( rc ) )
        return;
    clientSkin->draw( buf, rc );
}

void CRGUIWindowBase::drawScroll(CRScrollSkinRef sskin, lvRect rect, bool vertical, int pos, int maxpos, int pagesize)
{
    LVDrawBuf & buf = *_wm->getScreen()->getCanvas();
    if (hasTouchSettingsNavigation() && !vertical) {
        CRMenu *menu=dynamic_cast<CRMenu *>(this);
        if (menu && menu->isSettingsEditor()) {
            CRRectSkinRef titleSkin=getSkin()->getTitleSkin();
            lUInt32 ink=titleSkin->getTextColor(), paper=titleSkin->getBackgroundColor();
            buf.FillRect(rect,paper);
            buf.FillRect(rect.left,rect.top,rect.right,rect.top+1,ink);
            int pages=pagesize>0 ? (maxpos+pagesize-1)/pagesize : 0;
            lvRect label(rect);
            if (pages>1) {
                int width=rect.height()*3/2;
                lvRect previous(rect),next(rect);
                previous.right=previous.left+width; next.left=next.right-width;
                CRButtonSkinRef a(new CRSettingsNavigationButtonSkin(titleSkin));
                CRButtonSkinRef b(new CRSettingsNavigationButtonSkin(titleSkin,true));
                a->drawButton(buf,previous,isScrollEnabled(false)?CRButtonSkin::ENABLED:0);
                b->drawButton(buf,next,isScrollEnabled(true)?CRButtonSkin::ENABLED:0);
                if (!_controlsCreated) {
                    addControl(new CRScrollButtonControl(this,previous,a,false));
                    addControl(new CRScrollButtonControl(this,next,b,true));
                }
                label.left=previous.right; label.right=next.left;
                settingsText(buf,label,lString32::itoa(pos/pagesize+1)+U" / "+lString32::itoa(pages),menu->settingsFont(true),ink,true);
            } else {
                settingsText(buf,label,lString32(menu->isActionMenu() ? _("Choose an action") :
                    menu->settingsChanged()?_("Done applies changes"):_("Changes apply on Done")),menu->settingsFont(true),ink,true);
            }
            return;
        }
        CRRectSkinRef titleSkin = getSkin()->getTitleSkin();
        titleSkin->draw(buf, rect);
        buf.FillRect(rect.left, rect.top, rect.right, rect.top + 1,
                     titleSkin->getTextColor());
        int width = settingsNavigationButtonWidth(rect);
        lvRect previous(rect), next(rect), label(rect);
        previous.right = previous.left + width;
        next.left = next.right - width;
        label.left = previous.right;
        label.right = next.left;
        CRButtonSkinRef previousSkin(new CRSettingsNavigationButtonSkin(titleSkin));
        CRButtonSkinRef nextSkin(new CRSettingsNavigationButtonSkin(titleSkin, true));
        if (!_controlsCreated) {
            addControl(new CRScrollButtonControl(this, previous, previousSkin, false));
            addControl(new CRScrollButtonControl(this, next, nextSkin, true));
        }
        previousSkin->drawButton(buf, previous, isScrollEnabled(false) ? CRButtonSkin::ENABLED : 0);
        nextSkin->drawButton(buf, next, isScrollEnabled(true) ? CRButtonSkin::ENABLED : 0);
        int pages = pagesize > 0 ? (maxpos + pagesize - 1) / pagesize : 0;
        int page = pages > 0 ? pos / pagesize + 1 : 0;
        sskin->drawText(buf, label, lString32::itoa(page) + U" / " + lString32::itoa(pages));
        return;
    }
    lvRect rc = rect;
    sskin->draw(buf, rc);
    lvRect borders;

    sskin->getBorderWidths();

    int pages = pagesize>0 ? (maxpos+pagesize-1)/pagesize : 0;
    int page = pages>0 ? pos/pagesize+1 : 0;

    CRRectSkinRef bottomTabSkin = sskin->getBottomTabSkin();
    CRRectSkinRef bottomPageBoundSkin = sskin->getBottomPageBoundSkin();
    CRRectSkinRef bottomActiveTabSkin = sskin->getBottomActiveTabSkin();

    if ( !bottomTabSkin.isNull() && !bottomPageBoundSkin.isNull() &&
         !bottomActiveTabSkin.isNull() ) {
        // tabs
        if ( pages<=1 )
            return; // don't draw tabs if no other pages
        int tabwidth = bottomTabSkin->getMinSize().x;
        if ( tabwidth < 40 )
            tabwidth = 40;
        if ( tabwidth > bottomTabSkin->getMaxSize().x && bottomTabSkin->getMaxSize().x > 0)
            tabwidth = bottomTabSkin->getMaxSize().x;
        int maxtabs = (rc.width() - borders.left - borders.right) / tabwidth;
        if ( pages <= maxtabs ) {
            // can draw tabs
            lvRect r(rc);
            r.left += borders.left;
            for ( int i=0; i<pages; i++ ) {
                r.right = r.left + tabwidth;
                if ( i+1!=page ) {
                    bottomTabSkin->draw(buf, r);
                    lString32 label = lString32::itoa(i+1);
                    bottomTabSkin->drawText(buf, r, label);
                }
                if (!_controlsCreated)
                    addControl(new CRCommandControl(this, r, DCMD_GO_PAGE, i));
                r.left += tabwidth - r.height()/6;
            }
            bottomPageBoundSkin->draw(buf, rc);
            r = rc;
            r.left += borders.left;
            for ( int i=0; i<pages; i++ ) {
                r.right = r.left + tabwidth;
                if ( i+1==page ) {
                    bottomActiveTabSkin->draw(buf, r);
                    lString32 label = lString32::itoa(i+1);
                    bottomActiveTabSkin->drawText(buf, r, label);
                }
                r.left += tabwidth - r.height()/6;
            }
            return;
        }
    }

    rc.shrinkBy( borders );

    int btn1State = CRButtonSkin::ENABLED;
    int btn2State = CRButtonSkin::ENABLED;
    if ( pos <= 0 )
        btn1State = 0;
    if ( pos >= maxpos-pagesize )
        btn2State = 0;
    CRButtonSkinRef btn1Skin;
    CRButtonSkinRef btn2Skin;
    lvRect btn1Rect = rc;
    lvRect btn2Rect = rc;
    lvRect bodyRect = rc;
    lvRect sliderRect = rc;
    LVImageSourceRef bodyImg;
    LVImageSourceRef sliderImg;

    LVImageSourceRef hBody = sskin->getHBody();
    if ( hBody.isNull() ) {
        // text label with optional arrows
        lString32 label;
        label << lString32::itoa(page) + U" / " << lString32::itoa(pages);
        // calc label width
        int w = sskin->getFont()->getTextWidth( label.c_str(), label.length() );
        int margin = 4;
        btn1Skin = sskin->getLeftButton();
        btn2Skin = sskin->getRightButton();
        // calc button widths
        int bw1 = btn1Skin.isNull() ? 0 : btn1Skin->getMinSize().x;
        int bw2 = btn1Skin.isNull() ? 0 : btn2Skin->getMinSize().x;
        // total width
        int ww = w + margin + margin + bw1 + bw2;
        int dw = rc.width() - ww;
        rc.left += dw*3/4;
        rc.right = rc.left + ww;
        // adjust rectangle size
        btn1Rect = rc;
        btn2Rect = rc;
        btn1Rect.right = btn1Rect.left + bw1;
        btn2Rect.left = btn2Rect.right - bw2;
        bodyRect.left = btn1Rect.right;
        bodyRect.right = btn2Rect.left;
        int dy = bodyRect.height() - btn1Skin->getMinSize().y;
        btn1Rect.top += dy/2;
        btn1Rect.bottom = btn1Rect.top + btn1Skin->getMinSize().y;
        dy = bodyRect.height() - btn2Skin->getMinSize().y;
        btn2Rect.top += dy/2;
        btn2Rect.bottom = btn2Rect.top + btn2Skin->getMinSize().y;
        if (!_controlsCreated) {
            addControl(new CRScrollButtonControl(this, btn1Rect, btn1Skin, false));
            addControl(new CRScrollButtonControl(this, btn2Rect, btn2Skin, true));
        }
        btn1Skin->drawButton( buf, btn1Rect, btn1State );
        btn2Skin->drawButton( buf, btn2Rect, btn2State );
        sskin->drawText( buf, bodyRect, label );
        return;
    }

    if ( vertical ) {
        // draw vertical
        btn1Skin = sskin->getUpButton();
        btn2Skin = sskin->getDownButton();
        btn1Rect.bottom = btn1Rect.top + btn1Skin->getMinSize().y;
        btn2Rect.top = btn2Rect.bottom - btn2Skin->getMinSize().y;
        bodyRect.top = btn1Rect.bottom;
        bodyRect.bottom = btn2Rect.top;
        int sz = bodyRect.height();
        if ( pagesize < maxpos ) {
            sliderRect.top = bodyRect.top + sz * pos / maxpos;
            sliderRect.bottom = bodyRect.top + sz * (pos + pagesize) / maxpos;
        } else
            sliderRect = bodyRect;
        bodyImg = sskin->getVBody();
        sliderImg = sskin->getVSlider();
    } else {
        // draw horz
        btn1Skin = sskin->getLeftButton();
        btn2Skin = sskin->getRightButton();
        btn1Rect.right = btn1Rect.left + btn1Skin->getMinSize().x;
        btn2Rect.left = btn2Rect.right - btn2Skin->getMinSize().x;
        bodyRect.left = btn1Rect.right;
        bodyRect.right = btn2Rect.left;
        int sz = bodyRect.width();
        if ( pagesize < maxpos ) {
            sliderRect.left = bodyRect.left + sz * pos / maxpos;
            sliderRect.right = bodyRect.left + sz * (pos + pagesize) / maxpos;
        } else
            sliderRect = bodyRect;
        bodyImg = hBody;
        sliderImg = sskin->getHSlider();
    }
    if (!_controlsCreated) {
        addControl(new CRScrollButtonControl(this, btn1Rect, btn1Skin, false));
        addControl(new CRScrollButtonControl(this, btn2Rect, btn2Skin, true));
    }
    btn1Skin->drawButton( buf, btn1Rect, btn1State );
    btn2Skin->drawButton( buf, btn2Rect, btn2State );
    if ( !bodyImg.isNull() ) {
        LVImageSourceRef img = LVCreateStretchFilledTransform( bodyImg,
            bodyRect.width(), bodyRect.height() );
        buf.Draw( img, bodyRect.left, bodyRect.top, bodyRect.width(), bodyRect.height(), false );
    }
    if ( !sliderImg.isNull() ) {
        LVImageSourceRef img = LVCreateStretchFilledTransform( sliderImg,
            sliderRect.width(), sliderRect.height() );
        buf.Draw( img, sliderRect.left, sliderRect.top, sliderRect.width(), sliderRect.height(), false );
        if ( sskin->getShowPageNumbers() ) {
            lString32 label;
            label << lString32::itoa(page) + U" / " << lString32::itoa(pages);
            sskin->drawText( buf, sliderRect, label );
        }
    }
}

/// draw title bar using current skin, with optional scroll/tab/page indicator
void CRGUIWindowBase::drawTitleBar()
{
    CRWindowSkinRef skin = getSkin();
    if ( skin.isNull() ) {
        return;
    }
    LVDrawBuf & buf = *_wm->getScreen()->getCanvas();
    CRRectSkinRef titleSkin = skin->getTitleSkin();
    lvRect titleRc;
    if ( !getTitleRect( titleRc ) )
        return;
    titleSkin->draw( buf, titleRc );

    lvRect batteryRc( titleRc );
    batteryRc.left = batteryRc.right - BATTERY_ICON_SIZE;
    batteryRc.shrinkBy( titleSkin->getBorderWidths() );
    _wm->drawBattery( buf, batteryRc );

    CRScrollSkinRef sskin = skin->getScrollSkin();
    lvRect scrollRc;
    getScrollRect( scrollRc );
    bool scroll = ( !sskin.isNull() && sskin->getLocation()==CRScrollSkin::Title && !hasTouchSettingsNavigation() && !scrollRc.isEmpty() );
    if ( scroll ) {
        drawScroll( sskin, scrollRc, false, _page-1, _pages, 1 );
        titleRc.right = scrollRc.left;
    }

    //lvRect b = titleSkin->getBorderWidths();
    buf.SetTextColor( skin->getTextColor() );
    buf.SetBackgroundColor( skin->getBackgroundColor() );
    int imgWidth = 0;
    //titleRc.shrinkBy( b );
    int hh = titleRc.bottom - titleRc.top;
    if ( !_icon.isNull() ) {
        int w = _icon->GetWidth();
        int h = _icon->GetHeight();
        buf.Draw( _icon, titleRc.left + hh/2-w/2, titleRc.top + hh/2 - h/2, w, h );
        imgWidth = w + ITEM_MARGIN;
    }
    if (_wm->getScreen()->isTouchSupported()) {
        CRButtonSkinRef closeBtnSkin = skin->getCloseButton();
        if (!closeBtnSkin.isNull()) {
            lvRect closeBtnRc = titleRc;
            closeBtnRc.left += imgWidth;
            closeBtnRc.right = closeBtnRc.left + closeBtnSkin->getMinSize().x;

            int dy = titleRc.height() - closeBtnSkin->getMinSize().y;
            closeBtnRc.top += dy/2; //FIXME: no boundaries checks
            closeBtnRc.bottom = closeBtnRc.top + closeBtnSkin->getMinSize().y;

            lvRect touchRc = closeBtnRc;
            if (getSkinName() == U"#settings") {
                // Keep the chevron centred in a wide, full-height target.
                // Reserve space for the settings caption.
                touchRc.top = titleRc.top;
                touchRc.bottom = titleRc.bottom;
                int buttonWidth = settingsNavigationButtonWidth(titleRc);
                if (touchRc.width() < buttonWidth)
                    touchRc.right = touchRc.left + buttonWidth;
                closeBtnSkin = CRButtonSkinRef(new CRSettingsNavigationButtonSkin(titleSkin));
                closeBtnRc = touchRc;
            }
            closeBtnSkin->drawButton(buf, closeBtnRc);
            if (!_controlsCreated)
                addControl(new CRButtonControl(this, touchRc, closeBtnSkin, BTN_CLOSE,
                                               DCMD_BUTTON_PRESSED ));
            imgWidth += (touchRc.width() + ITEM_MARGIN);
        }
    }
    lvRect textRect = titleRc;
    textRect.left += imgWidth;
    titleSkin->drawText( buf, textRect, _caption );
}

void CRGUIWindowBase::setSkinName( const lString32  & skin )
{
    _skinName = skin;
    _skin.Clear();
    _baseSkinName.clear();
}

void CRGUIWindowBase::setSkinName( const lString32  & skin, const lString32 & baseSkin )
{
    if ( !_wm->getSkin().isNull() ) {
        setSkinName(skin);
        CRWindowSkinRef windowSkin = getSkin();
        if ( windowSkin.isNull() ) {
            setSkinName(baseSkin);
        } else
            _baseSkinName = baseSkin;
    }
}

void CRGUIWindowBase::addControl(CRGUIControl *control)
{
    _controls.add(control);
    _sortZorder = true;
}

void CRGUIWindowBase::invalidateControls()
{
    _controls.clear();
    _controlsCreated = false;
    _selectedControl = NULL;
}

/// called on system configuration change: screen size and orientation
void CRGUIWindowBase::reconfigure( int flags )
{
    lvRect fs = _wm->getScreen()->getRect();
    _skin.Clear();
    if ( _fullscreen ) {
        setRect( fs );
    } else {
        lvRect rc = getRect();
        int dx = fs.width();
        //int dy = fs.height();
        if ( rc.right > dx ) {
            rc.left -= rc.right - dx;
            rc.right = dx;
            if ( rc.left < 0 )
                rc.left = 0;
        }
        if ( rc.right > dx ) {
            rc.left -= rc.right - dx;
            rc.right = dx;
            if ( rc.left < 0 )
                rc.left = 0;
        }
        setRect( rc );
    }
    _selectedControl = NULL;
    invalidateControls();
    setDirty();
}

/// returns true if key is processed (by default, let's translate key to command using accelerator table)
bool CRGUIWindowBase::onKeyPressed( int key, int flags )
{
    if ( _acceleratorTable.isNull() ) {
        CRLog::trace("CRGUIWindowBase::onKeyPressed( %d, %d) - no accelerator table specified!", key, flags );
        return !_passKeysToParent;
    }
    int cmd, param;
    if ( _acceleratorTable->translate( key, flags, cmd, param ) ) {
        CRLog::trace("Accelerator applied: key %d(%d) -> command(%d,%d)", key, flags, cmd, param );
		if ( cmd == GCMD_PASS_TO_PARENT ) {
			return false;
		}
        CRGUIEvent * event = new CRGUICommandEvent( cmd, param );
        event->setTargetWindow(this);
        _wm->postEvent( event );
        return true;
        //return onCommand( cmd, param );
    } else {
        CRLog::trace("Accelerator not found for key %d(%d)", key, flags );
        _acceleratorTable->dump();
    }
    return !_passKeysToParent;
}

void CRDocViewWindow::draw()
{
    lvRect clientRect = _rect;
    CRWindowSkinRef skin = getSkin();
    if ( !skin.isNull() ) {
        if ( getClientRect( clientRect ) ) {
            skin->draw( *_wm->getScreen()->getCanvas(), _rect );
            drawTitleBar();
            drawStatusBar();
        }
    }
    LVDocImageRef pageImage = _docview->getPageImage(0);
    LVDrawBuf * drawbuf = pageImage->getDrawBuf();
    _wm->getScreen()->draw( drawbuf, clientRect.left, clientRect.top );
}

void CRDocViewWindow::setRect( const lvRect & rc )
{
    if ( rc == _rect )
        return;
    _rect = rc;
    lvRect clientRect = _rect;
    if ( !_skin.isNull() )
        clientRect = _skin->getClientRect( rc );
    _docview->Resize( clientRect.width(), clientRect.height() );
    setDirty();
}

LVImageSourceRef CRMenuItem::getItemIcon()
{
    if ( _image.isNull() && _menu != NULL ) {
        _image = _menu->getImage( _imageId );
    }
    return _image;
}

int CRMenuItem::DrawIcon(LVDrawBuf & buf, lvRect & rc, lvRect & bordersRc)
{
    int imgWidth = 0;

    LVImageSourceRef image = getItemIcon();
    if ( !image.isNull() ) {
        int hh = rc.bottom - rc.top - bordersRc.top - bordersRc.bottom;
        int w = image->GetWidth();
        int h = image->GetHeight();
        if ( h > hh)
            h = hh;
        buf.Draw( image, rc.left + bordersRc.left, rc.top + bordersRc.top + (hh - h)/2 , w, h );
        imgWidth = w + ITEM_MARGIN;
    }
    return imgWidth;
}

void CRMenuItem::Draw( LVDrawBuf & buf, lvRect & rc, CRRectSkinRef skin, CRRectSkinRef valueSkin, bool selected )
{
    if (_menu && _menu->isSettingsEditor()) {
        _menu->drawSettingsItem(*this,buf,rc,selected);
        _itemDirty = false;
        return;
    }
    _itemDirty = false;
    lvRect itemBorders = skin->getBorderWidths();
    skin->draw( buf, rc );
    buf.SetTextColor( skin->getTextColor() );
    buf.SetBackgroundColor( skin->getBackgroundColor() );

    int imgWidth = DrawIcon( buf, rc, itemBorders );

    lvRect textRect = rc;
    textRect.left += imgWidth;

    lString32 s1;
    lString32 s2;
    lvRect valueRect = textRect;
    if ( _label.split2(cs32("\t"), s1, s2 ) ) {
        //valueSkin->drawText( buf, textRect, s2 );
    } else {
        s1 = _label;
    }
    LVFontRef font = getFont();
    if ( font.isNull() )
        font = skin->getFont();
    skin->drawText( buf, textRect, s1, font );
    if ( !s2.empty() ) {
        valueSkin->drawText( buf, valueRect, s2 );
    }
}

int CRMenu::getPageCount()
{
    return (_items.length() + _pageItems - 1) / _pageItems;
}

bool CRMenu::setCurPage( int nPage )
{
    int oldTop = _topItem;
    _topItem = _pageItems * nPage;
#if FULL_SCROLL==1
    if ( _topItem >= (int)_items.length() )
        _topItem = ((int)_items.length() - 1) / _pageItems * _pageItems;
#else
    if ( _topItem + _pageItems >= (int)_items.length() )
        _topItem = (int)_items.length() - _pageItems;
#endif
    if ( _topItem < 0 )
        _topItem = 0;
    if ( _topItem != oldTop ) {
        invalidateControls();
        _pageUpdate = true;
        setDirty();
        return true;
    }
    return false;
}

int CRMenu::getCurPage( )
{
    return (_topItem + (_pageItems-1)) / _pageItems;
}

int CRMenu::getTopItem()
{
    return _topItem;
}

void CRMenu::Draw( LVDrawBuf & buf, lvRect & rc, CRRectSkinRef skin, CRRectSkinRef valueSkin, bool selected )
{
    if (_menu && _menu->isSettingsEditor()) {
        _menu->drawSettingsItem(*this,buf,rc,selected);
        _itemDirty = false;
        return;
    }
    CRLog::trace("enter CRMenu::Draw()");
    CRMenuSkinRef menuSkin = _skin; //getSkin();
    //CRRectSkinRef valueSkin = menuSkin->getValueSkin();

    lvRect rc2 = rc;

    lvRect itemBorders = skin->getBorderWidths();
    skin->draw( buf, rc );
    buf.SetTextColor( skin->getTextColor() );
    buf.SetBackgroundColor( skin->getBackgroundColor() );
    int imgWidth = DrawIcon( buf, rc, itemBorders );

    lvRect textRect = rc;
    textRect.left += imgWidth;
    //textRect.shrinkBy( itemBorders );

    lString32 s = getSubmenuValue();
    lvRect valueRect = textRect;
    if ( !s.empty() ) {
        if ( valueSkin.isNull() ) {
            textRect.bottom -= textRect.height()*2/5;
        } else {
            valueSkin->drawText( buf, textRect, s );
        }
    }
    skin->drawText( buf, textRect, _label );
    if ( !s.empty() ) {
        if ( valueSkin.isNull() ) {
            // old implementation: no value skin
            int w = _valueFont->getTextWidth( s.c_str(), s.length() );
            rc2 = valueRect;
            rc2.top += rc2.height()*3/8;
            int hh = rc2.height();
            buf.SetTextColor( skin->getTextColor() );
            _valueFont->DrawTextString( &buf, rc2.right - w - ITEM_MARGIN, rc2.top + hh/2 - _valueFont->getHeight()/2, s.c_str(), s.length(), U'?', NULL, false, 0 );
        } else {
            valueSkin->drawText( buf, valueRect, s );
        }
    }
    _itemDirty = false;
    CRLog::trace("exit CRMenu::Draw()");
}

lvPoint CRMenuItem::getItemSize( CRRectSkinRef skin )
{
    LVFontRef font = _defFont;
    if ( font.isNull() )
        font = skin->getFont();
    lvRect borders = skin->getBorderWidths();
    int h = font->getHeight() * 7/4;

    int w = font->getTextWidth( _label.c_str(), _label.length() );
    w += ITEM_MARGIN * 2;
    LVImageSourceRef image = getItemIcon();
    if ( !image.isNull() ) {
        int hi = 0;
        if ( image->GetHeight()>h )
            hi = image->GetHeight() * 8 / 7;
        if ( h < hi )
            h = hi;
        w += hi;
    }

    lvPoint minsize = skin->getMinSize();
    if ( minsize.y>0 && h < minsize.y )
        h = minsize.y;
    if ( minsize.x>0 && w < minsize.x )
        w = minsize.x;
    h += borders.bottom + borders.top;
    w += borders.left + borders.right;
    return lvPoint( w, h );
}

CRMenuSkinRef CRMenu::getSkin()
{
    if ( !_skin.isNull() )
        return _skin;
    lString32 path = getSkinName();
    if (!path.startsWith("#"))
        path = cs32("/CR3Skin/") + path;
    if ( _wm->getScreenOrientation()&1 ) {
        _skin = _wm->getSkin()->getMenuSkin( (path + "-rotated").c_str() );
        if ( _skin.isNull() && !_baseSkinName.empty() ) {
            _skin = _wm->getSkin()->getMenuSkin( (_baseSkinName + "-rotated").c_str() );
        }
    }
    if ( !_skin )
        _skin = _wm->getSkin()->getMenuSkin( path.c_str() );
    return _skin;
}

lvPoint CRMenu::getItemSize()
{
    CRMenuSkinRef skin = getSkin();
    CRRectSkinRef itemSkin = skin->getItemSkin();
    lvRect itemBorders = itemSkin->getBorderWidths();
    lvPoint sz = CRMenuItem::getItemSize( itemSkin );
    if ( !isSubmenu() || _propName.empty() || _props.isNull() )
        return sz;
    int maxw = 0;
    for ( int i=0; i<_items.length(); i++ ) {
        lString32 s = _items[i]->getLabel();
        int w = _valueFont->getTextWidth( s.c_str(), s.length() );
        if ( w > maxw )
            maxw = w;
    }
    if ( maxw>0 )
        sz.x = sz.x + itemBorders.left + itemBorders.right + maxw;
    return sz;
}

bool CRMenu::getStatusRect(lvRect &rc)
{
    if (isSettingsEditor() && _wm->getScreen()->isTouchSupported()) {
        rc = _rect;
        rc.top = rc.bottom - settingsFont(true)->getHeight()*2 - 12;
        return !rc.isEmpty();
    }
    if (!hasTouchSettingsNavigation() || _pageItems <= 0)
        return CRGUIWindowBase::getStatusRect(rc);
    CRMenuSkinRef skin = getSkin();
    rc = _rect;
    rc.shrinkBy(skin->getBorderWidths());
    CRRectSkinRef separator = skin->getSeparatorSkin();
    int separatorHeight = separator.isNull() ? 0 : separator->getMinSize().y;
    // Divide space between the header, rows and footer without recursively
    // asking for the client/title size while those rectangles are calculated.
    int height = (rc.height() - separatorHeight * (_pageItems - 1)) / (_pageItems + 2);
    lvRect title;
    CRGUIWindowBase::getTitleRect(title);
    height = std::max(height, title.height());
    rc.top = rc.bottom - height;
    return !rc.isEmpty();
}

bool CRMenu::getScrollRect(lvRect &rc)
{
    if (hasTouchSettingsNavigation()) return getStatusRect(rc);
    return CRGUIWindowBase::getScrollRect(rc);
}

bool CRMenu::getTitleRect(lvRect &rc)
{
    if (!CRGUIWindowBase::getTitleRect(rc))
        return false;
    if (isSettingsEditor() && _wm->getScreen()->isTouchSupported()) {
        rc.bottom = rc.top + settingsFont()->getHeight()*2 + 24;
        return true;
    }
    if (!_fullscreen || _pageItems <= 0 || getSkinName() != U"#settings")
        return true;
    CRMenuSkinRef skin = getSkin();
    CRRectSkinRef separator = skin->getSeparatorSkin();
    int separatorHeight = separator.isNull() ? 0 : separator->getMinSize().y;
    lvRect status;
    getStatusRect(status);
    if (hasTouchSettingsNavigation()) {
        rc.bottom = rc.top + status.height();
        return true;
    }
    // Share the available height between the header and the visible rows.
    // Avoid getItemHeight(): its client rectangle depends on this title.
    int rowHeight = (status.top - rc.top - separatorHeight * (_pageItems - 1))
                    / (_pageItems + 1);
    if (rowHeight > rc.height())
        rc.bottom = rc.top + rowHeight;
    return true;
}

int CRMenu::getItemHeight()
{
    CRMenuSkinRef skin = getSkin();
    CRRectSkinRef itemSkin = skin->getItemSkin();
    CRRectSkinRef separatorSkin = skin->getSeparatorSkin();
    int separatorHeight = 0;
    if ( !separatorSkin.isNull() )
        separatorHeight = separatorSkin->getMinSize().y;
    if (isSettingsEditor()) separatorHeight=1;
    int h = itemSkin->getFont()->getHeight() * 5/4;
    lvPoint minsize = skin->getMinSize();
    if ( minsize.y>0 && h < minsize.y )
        h = minsize.y;
    if ( _fullscreen ) {
        int nItems = _pageItems; // _items.length();
        //int scrollHeight = 0;
        //lvRect statusRect;
        //if ( getStatusRect(  statusRect ) )
        //    scrollHeight = statusRect.height();
        //lvRect rc(0,0,_wm->getScreen()->getWidth(), _wm->getScreen()->getHeight() );
        lvRect client;
        getClientRect( client );
        h = client.height() - separatorHeight*(nItems-1);
        if ( nItems > 0 )
            h /= nItems;
    }

    return h;
}

lvPoint CRMenu::getMaxItemSize()
{
    CRMenuSkinRef skin = getSkin();
    CRRectSkinRef itemSkin = skin->getItemSkin();
    lvPoint mySize = getItemSize();
    //int itemHeight = getItemHeight();
    int maxx = 0;
    int maxy = 0;
    for ( int i=0; i<_items.length(); i++ ) {
        lvPoint sz = _items[i]->getItemSize( itemSkin );
        if ( maxx < sz.x )
            maxx = sz.x;
        if ( maxy < sz.y )
            maxy = sz.y;
    }
    if ( maxx < mySize.x )
        maxx = mySize.x;
    if ( maxy < mySize.y )
        maxy = mySize.y;
    if ( _fullscreen )
        maxy = getItemHeight();
    return lvPoint( maxx, maxy );
}

lvPoint CRMenu::getSize()
{
    if ( _fullscreen )
        return lvPoint( _wm->getScreen()->getWidth(), _wm->getScreen()->getHeight() );
    lvPoint itemSize = getMaxItemSize();
    int nItems = _items.length();
    int scrollHeight = 0;
    if ( nItems > _pageItems ) {
        nItems = _pageItems;
        scrollHeight = SCROLL_HEIGHT;
    }
    int h = nItems * (itemSize.y) + scrollHeight;
    int w = itemSize.x + 3 * ITEM_MARGIN + HOTKEY_SIZE;
    CRMenuSkinRef skin = getSkin();
    CRRectSkinRef sskin = skin->getItemShortcutSkin();
    CRRectSkinRef iskin = skin->getItemSkin();
    if ( !sskin.isNull() ) {
        lvPoint ssz = sskin->getMinSize();
        lvRect borders = sskin->getBorderWidths();
        w += ssz.x + borders.left + borders.right;
    }
    if ( !iskin.isNull() ) {
        lvRect borders = iskin->getBorderWidths();
        w += borders.left + borders.right;
    }
    if ( w>600 )
        w = 600;
    lvPoint res = skin->getWindowSize( lvPoint( w, h ) );
    if ( res.x > _wm->getScreen()->getWidth() )
        res.x = _wm->getScreen()->getWidth();
    if ( res.y > _wm->getScreen()->getHeight() )
        res.y = _wm->getScreen()->getHeight();
    return res;
}

lString32 CRMenu::getSubmenuValue()
{
    if ( !isSubmenu() || _propName.empty() || _props.isNull() )
        return lString32::empty_str;
    lString32 value = getProps()->getStringDef(
                               UnicodeToUtf8(getPropName()).c_str(), "");
    for ( int i=0; i<_items.length(); i++ ) {
        if ( !_items[i]->getPropValue().empty() &&
                value==(_items[i]->getPropValue()) )
            return _items[i]->getLabel();
    }
    return lString32::empty_str;
}

void CRMenu::toggleSubmenuValue()
{
    if ( !isSubmenu() || _propName.empty() || _props.isNull() )
        return;
    lString32 value = getProps()->getStringDef(
                               UnicodeToUtf8(getPropName()).c_str(), "");
    for ( int i=0; i<_items.length(); i++ ) {
        if ( !_items[i]->getPropValue().empty() &&
              value==(_items[i]->getPropValue()) ) {
            int n = (i + 1) % _items.length();
            getProps()->setString(UnicodeToUtf8(getPropName()).c_str(), _items[n]->getPropValue() );
            //return _items[i]->getLabel();
            return;
        }
    }
}

#if 0
static void DrawArrow( LVDrawBuf & buf, int x, int y, int dx, int dy, lvColor cl, int direction )
{
    int x0 = x + dx/2;
    int y0 = y + dy/2;
    dx -= 4;
    dy -= 4;
    dx /= 2;
    dy /= 2;
    int deltax = direction?-1:1;
    x0 -= deltax * dx/2;
    for ( int k=0; k<dx; k++ ) {
        buf.FillRect( x0+k*deltax, y0-k, x0+k*deltax+1, y0+k+1, cl );
    }
}
#endif

int CRMenu::getScrollHeight()
{
    CRMenuSkinRef skin = getSkin();
    int nItems = _items.length();
    int scrollHeight = 0;
    CRScrollSkinRef sskin = skin->getScrollSkin();
    if ( nItems > _pageItems || !sskin->getAutohide() ) {
        nItems = _pageItems;
        scrollHeight = SCROLL_HEIGHT;
        if ( sskin->getMinSize().y>0 )
            scrollHeight = sskin->getMinSize().y;
    }
    return scrollHeight;
}

/// returns index of selected item, -1 if no item selected
int CRMenu::getSelectedItemIndex()
{
    if ( _cmdToHighlight>=0 ) {
        // highlighted command
        for ( int i=0; i<_items.length(); i++ ) {
            if ( _items[i]->getId()==_cmdToHighlight ) {
                return i;
            }
        }
        return -1;
    }
    if ( getProps().isNull() )
        return -1;
    for ( int i=0; i<_items.length(); i++ ) {
        if ( !_items[i]->getPropValue().empty() &&
              getProps()->getStringDef(
                       UnicodeToUtf8(getPropName()).c_str()
                       , "")==(_items[i]->getPropValue()) )
            return i;
    }
    return -1;
}

void CRMenu::highlightCommandItem( int cmd )
{
    CRLog::debug("Highlighting menu item");
    _cmdToHighlight = cmd;
    _selectedItem = getSelectedItemIndex();
    _cmdToHighlight = -1;
    setDirty();
    _wm->updateWindow(this);
}

void CRMenu::drawClient()
{
    CRLog::trace("enter CRMenu::drawClient()");
    LVDrawBuf & buf = *_wm->getScreen()->getCanvas();
    CRMenuSkinRef skin = getSkin();
    CRRectSkinRef clientSkin = skin->getClientSkin();
    //CRRectSkinRef titleSkin = skin->getTitleSkin();
    //CRScrollSkinRef sskin = skin->getScrollSkin();
    CRRectSkinRef valueSkin = skin->getValueSkin();
    CRRectSkinRef itemSkin = skin->getItemSkin();
    CRRectSkinRef itemShortcutSkin = skin->getItemShortcutSkin();
    CRRectSkinRef itemSelSkin = skin->getSelItemSkin();
    CRRectSkinRef itemSelShortcutSkin = skin->getSelItemShortcutSkin();
    CRRectSkinRef evenitemSkin = skin->getEvenItemSkin();
    CRRectSkinRef evenitemShortcutSkin = skin->getEvenItemShortcutSkin();
    CRRectSkinRef evenitemSelSkin = skin->getEvenSelItemSkin();
    CRRectSkinRef evenitemSelShortcutSkin = skin->getEvenSelItemShortcutSkin();
    if ( evenitemSkin.isNull() )
        evenitemSkin = itemSkin;
    if ( evenitemShortcutSkin.isNull() )
        evenitemShortcutSkin = itemShortcutSkin;
    if ( evenitemSelSkin.isNull() )
        evenitemSelSkin = itemSelSkin;
    if ( evenitemSelShortcutSkin.isNull() )
        evenitemSelShortcutSkin = itemSelShortcutSkin;

    CRRectSkinRef separatorSkin = skin->getSeparatorSkin();
    int separatorHeight = 0;
    if ( !separatorSkin.isNull() )
        separatorHeight = separatorSkin->getMinSize().y;

    if (isSettingsEditor()) separatorHeight=1;
    //lvRect itemBorders = itemSkin->getBorderWidths();

    bool showShortcuts = skin->getShowShortcuts();

    buf.SetTextColor( 0x000000 );
    buf.SetBackgroundColor( 0xFFFFFF );

    lvRect clientRect;
    getClientRect(clientRect);
    if ( !clientSkin.isNull() && _pageUpdate)
        clientSkin->draw( buf, clientRect );

    lvPoint itemSize = getMaxItemSize();
    //int hdrHeight = itemSize.y; // + ITEM_MARGIN + ITEM_MARGIN;
    //lvPoint sz = getSize();

    //int nItems = _items.length();
    //int scrollHeight = getScrollHeight();

    lvRect itemsRc( clientRect );
    lvRect rc( itemsRc );
    rc.top += 0; //ITEM_MARGIN;
    //rc.left += ITEM_MARGIN;
    //rc.right -= ITEM_MARGIN;
    LVFontRef numberFont( fontMan->GetFont( MENU_NUMBER_FONT_SIZE, 600, true, css_ff_sans_serif, cs8("Arial")) );
    for ( int index=0; index<_pageItems; index++ ) {
        int i = _topItem + index;
        if ( i >= _items.length() )
            break;
        CRMenuItem * item = _items[i];

        bool selected = (i == _selectedItem);

        rc.bottom = rc.top + itemSize.y;
		if (!(_pageUpdate || item->isItemDirty())) {
			rc.top = rc.bottom + separatorHeight;
			continue;
		}

		if (!_pageUpdate && !clientSkin.isNull()) {
			lvRect oldcr;
			buf.GetClipRect(&oldcr);
			buf.SetClipRect(&rc);
			clientSkin->draw( buf, clientRect );
			buf.SetClipRect(&oldcr);
		}

        bool even = (i & 1);
        CRRectSkinRef is = selected
                           ? (even ? evenitemSelSkin : itemSelSkin)
                           : (even ? evenitemSkin : itemSkin);
        CRRectSkinRef ss = selected
                           ? (even ? evenitemSelShortcutSkin : itemSelShortcutSkin)
                           : (even ? evenitemShortcutSkin : itemShortcutSkin);
        if ( selected ) {
            lvRect sel = rc;
            sel.extend( 4 );
            //buf.FillRect(sel, itemSelSkin->getBackgroundColor() );
        }

        lvRect itemRc( rc );

        if (!_controlsCreated && !item->isSettingsPreview()) {
            lvRect hit(itemRc);
            if (isSettingsEditor() && item->isSubmenu() && static_cast<CRMenu *>(item)->hasInlineSetting())
                hit.bottom = hit.top + hit.height()/2;
            addControl(new CRMenuItemControl(this, hit, index));
        }
        if ( showShortcuts ) {
            int shortcutSize = HOTKEY_SIZE;
            if ( shortcutSize < ss->getMinSize().x )
                shortcutSize = ss->getMinSize().x;
            // number
            lvRect numberRc( rc );
            //numberRc.extend(ITEM_MARGIN/4); //ITEM_MARGIN/8-2);
            numberRc.right = numberRc.left + shortcutSize;

            ss->draw( buf, numberRc );
            lString32 number = index<9 ? lString32::itoa( index+1 ) : U"0";
            buf.SetTextColor( ss->getTextColor() );
            buf.SetBackgroundColor( ss->getBackgroundColor() );
            ss->drawText( buf, numberRc, number );
            // item
            itemRc.left = numberRc.right;
        }

        is->setTextAlign( is->getTextAlign() | SKIN_EXTEND_TAB);
        item->Draw( buf, itemRc, is, valueSkin, selected );

        // draw separator
        if ( separatorHeight>0 && index<_pageItems-1 ) {
            lvRect r(rc);
            r.top += itemSize.y;
            r.bottom = r.top + separatorHeight;
            if (isSettingsEditor()) buf.FillRect(r,getSkin()->getItemSkin()->getTextColor());
            else separatorSkin->draw(buf, r);
        }
        rc.top += itemSize.y + separatorHeight;
    }
    CRLog::trace("exit CRMenu::drawClient()");
    _pageUpdate = false;
}

/// draw battery state to specified rectangle of screen
void CRGUIWindowManager::drawBattery( LVDrawBuf & buf, const lvRect & rc )
{
    int percent;
    bool charge;
    if ( !getBatteryStatus( percent, charge ) )
        return;
    LVDrawStateSaver saver( buf );
    buf.SetTextColor(0xFFFFFF);
    buf.SetBackgroundColor(0x000000);

    LVRefVec<LVImageSource> icons;
    bool drawPercent = false; //m_props->getBoolDef(PROP_SHOW_BATTERY_PERCENT, true) || m_batteryIcons.size()<=2;
    if ( m_batteryIcons.size()>1 ) {
        icons.add(getBatteryIcons()[0]);
        if ( drawPercent ) {
//            m_batteryFont = fontMan->GetFont(getBatteryIcons()[0]->GetHeight()-1, 900, false,
//                    DEFAULT_FONT_FAMILY, m_statusFontFace);
            icons.add(getBatteryIcons()[getBatteryIcons().length()-1]);
        } else {
            for ( int i=1; i<getBatteryIcons().length()-1; i++ )
                icons.add(getBatteryIcons()[i]);
        }
    } else {
        if ( m_batteryIcons.size()==1 )
            icons.add(getBatteryIcons()[0]);
    }

    //bool drawPercent = m_props->getBoolDef( PROP_SHOW_BATTERY_PERCENT, true );
    LVDrawBatteryIcon( &buf, rc,
                       percent, charge,
                       icons,
                       NULL );
}

/// closes menu and its submenus
void CRMenu::destroyMenu()
{
    for ( int i=_items.length()-1; i>=0; i-- )
        if ( _items[i]->isSubmenu() ) {
            ((CRMenu*)_items[i])->destroyMenu();
            _items.remove( i );
        }
    _wm->closeWindow( this ); // close, for root menu
}

/// closes menu and its submenus
void CRMenu::closeMenu( int command, int params )
{
    for ( int i=0; i<_items.length(); i++ )
        if ( _items[i]->isSubmenu() )
            ((CRMenu*)_items[i])->closeMenu( 0, 0 );
    if ( _menu != NULL )
        _wm->showWindow( this, false ); // just hide, for submenus
    else {
        if ( command )
            _wm->postCommand( command, params );
        destroyMenu();
    }
}

/// closes top level menu and its submenus, posts command
void CRMenu::closeAllMenu( int command, int params )
{
    CRMenu* p = this;
    while ( p->_menu )
        p = p->_menu;
    if ( command )
        _wm->postCommand( command, params );
    p->destroyMenu();
}

/// called on system configuration change: screen size and orientation
void CRMenu::reconfigure( int flags )
{
    _skin.Clear();
    getSkin();
    _fullscreen = isSettingsEditor() || _fullscreen || _skin->getFullScreen();
    CRGUIWindowBase::reconfigure( flags );
    int pageItems = _pageItems;
    if (isSettingsEditor()) {
        int label = settingsFont()->getHeight(), value = settingsFont(true)->getHeight();
        int usable = _wm->getScreen()->getHeight() - label*2 - value*2 - 36;
        int target = _propName.empty() ? label*2+value+4 : label*2+8;
        pageItems = std::max(4,std::min(10,usable/std::max(1,target)));
        if (_items.length()) pageItems = std::min(pageItems,std::max(6,_items.length()));
    }
    if (!isSettingsEditor() && _skin->getMinItemCount()>0 && pageItems<_skin->getMinItemCount() )
        pageItems = _skin->getMinItemCount();
    if (!isSettingsEditor() && _skin->getMaxItemCount()>0 && pageItems>_skin->getMaxItemCount() )
        pageItems = _skin->getMaxItemCount();
    if ( pageItems!=_pageItems ) {
        // change items per page
        _pageItems = pageItems;
        _topItem = _topItem / pageItems * pageItems;
    }

    // reconfigure submenus
    LVPtrVector<CRMenuItem> &items = getItems();
    for ( int i=0; i<items.length(); i++) {
        if (items[i]->isSubmenu()) {
            CRMenu * submenu = static_cast<CRMenu *>(items[i]);
            submenu->reconfigure(flags);
        }
    }
}

bool CRMenu::onKeyPressed( int key, int flags )
{
    CRGUIWindowBase::onKeyPressed( key, flags );
    return true; // don't allow key processing by parent window
}

/// called if window gets focus
void CRMenu::activated()
{
    int curItem = getSelectedItemIndex();
    if ( curItem>=0 ) {
        _topItem = curItem / _pageItems * _pageItems;
        _selectedItem = curItem;
    } else
		_selectedItem = getDefaultSelectionIndex();
    _pageUpdate = true;
    setDirty();
}

void CRMenu::doCloseMenu(int command, bool highlight, int param)
{
	if ( _menu == NULL ) {
		if (highlight)
			highlightCommandItem( command );
		closeMenu(command, param); // close, for root menu	
    } else
		closeMenu(0);
}

int CRMenu::getLastOnPage()
{
	int lastOnPage = _topItem + _pageItems;
	if (lastOnPage >= (int)_items.length())
		lastOnPage = (int)_items.length();
	return lastOnPage;
}

void CRMenu::setCurItem(int nItem)
{
	int oldItem = _selectedItem;

	if (_selectedItem >= 0) {
		_items[oldItem]->onLeave();
	}
	int lastOnPage = getLastOnPage();
	_selectedItem = nItem;
	if (_selectedItem < _topItem)
		_selectedItem = lastOnPage -1;
	else if (_selectedItem > lastOnPage -1)
		_selectedItem = _topItem;
	_items[_selectedItem]->onEnter();
	if (oldItem != _selectedItem) {
		setDirty();
		_wm->updateWindow(this);
	}
}

/// returns true if command is processed
bool CRMenu::onCommand( int command, int params )
{
    CRLog::trace( "CRMenu::onCommand(%d, %d)", command, params );
    if (isSettingsEditor()) {
        if (command == DCMD_SETTINGS_CHOICE || command == DCMD_SETTINGS_STEP) {
            updateSettingsValue(params/2, params&1, command == DCMD_SETTINGS_STEP);
            return true;
        }
        if ((command == DCMD_BUTTON_PRESSED || command == DCMD_BUTTON_PRESSED_LONG) && params == BTN_SETTINGS_DONE) {
            CRMenu *root=settingsRoot();
            closeAllMenu(root->getId(),0);
            return true;
        }
        if (command == DCMD_BUTTON_PRESSED && params == BTN_CLOSE && !_menu) {
            closeMenu(0); // Root Cancel discards the staged property copy.
            return true;
        }
    }
    if (DCMD_BUTTON_PRESSED == command || DCMD_BUTTON_PRESSED_LONG == command) {
        switch (params) {
        case BTN_CLOSE:
            doCloseMenu(getId(), false, 0);
            return true;
        case BTN_SCROLL_DOWN:
            command = MCMD_NEXT_PAGE;
            params = 1;
            break;
        case BTN_SCROLL_UP:
            command = MCMD_PREV_PAGE;
            params = 1;
            break;
        default:
            return false;
        }
    }
    switch (command) {
	case MCMD_CANCEL:
		closeMenu( 0 );
		return true;
	case MCMD_OK:
		doCloseMenu(getId(), false, params);
		return true;
	case MCMD_SCROLL_FORWARD_LONG:
        setCurPage( getCurPage()+10 );
        return true;
    case MCMD_SCROLL_BACK_LONG:
		setCurPage( getCurPage()-10 );
		return true;
	case MCMD_SCROLL_FORWARD:
		if ( params==0 )
			params = 1;
        setCurPage( getCurPage()+params );
        return true;
    case MCMD_SCROLL_BACK:
		if ( params==0 )
			params = 1;
        setCurPage( getCurPage()-params );
        return true;
    case MCMD_NEXT_ITEM:
		setCurItem(_selectedItem + 1);
		return true;
    case MCMD_PREV_ITEM:
		setCurItem(_selectedItem < 0 ? getLastOnPage() -1 : _selectedItem - 1);
		return true;
    case MCMD_NEXT_PAGE:
		if (setCurPage(getCurPage() + 1))
			if (_selectedItem >= 0)	
				setCurItem(_selectedItem + _pageItems);
		return true;
    case MCMD_PREV_PAGE:
		if (_topItem == 0) {
			doCloseMenu(getId(), false, params);
		} else if (setCurPage(getCurPage() - 1) && _selectedItem >= 0)
			setCurItem(_selectedItem - _pageItems);
		return true;
    case MCMD_SELECT:
		onItemSelect(_selectedItem, params);
		return true;
    case DCMD_GO_PAGE:
        if (setCurPage(params) && _selectedItem >= 0)
            setCurItem(_selectedItem - _pageItems);
        return true;
    }

    int option = -1;
    int longPress = 0;
    if ( command>=MCMD_SELECT_0 && command<=MCMD_SELECT_9 )
        option = (command==MCMD_SELECT_0) ? 9 : command - MCMD_SELECT_1;
    if ( command>=MCMD_SELECT_0_LONG && command<=MCMD_SELECT_9_LONG ) {
        option = (command==MCMD_SELECT_0_LONG) ? 9 : command - MCMD_SELECT_1_LONG;
        longPress = 1;
    }
    if ( option < 0 ) {
        CRLog::error( "CRMenu::onCommand() - unsupported command %d, %d", command, params );
        return true;
    }
	onItemSelect(option += getTopItem(), longPress);
    return true;
}

bool CRMenu::onItemSelect(int itemId, int param)
{
	if (itemId < 0 || itemId >= _items.length()) {
		CRLog::error( "CRMenu::onItemSelect() - invalid selection: %d", itemId);
		return true;
	}

    CRMenuItem * item = _items[itemId];

    if (item->onSelect() > 0)
        return true;

    if (item->isSubmenu()) {
        CRMenu * menu = (CRMenu *)item;
        if (!isSettingsEditor() && menu->getItems().length() <= 3 ) {
            // toggle 2 and 3 choices w/o menu
            menu->toggleSubmenuValue();
            item->setItemDirty();
            setDirty();
        } else {
            // show menu
            _wm->activateWindow( menu );
        }
        return true;
    } else {
        // command menu item
        if ( !item->getPropValue().empty() ) {
                // set property
            CRLog::trace("Setting property value");
            _props->setString( UnicodeToUtf8(getPropName()).c_str(), item->getPropValue() );
            if (_menu && isSettingsEditor()) {
                _menu->invalidateControls();
                _menu->_pageUpdate=true;
                _menu->invalidateSettingsValues();
            }
            doCloseMenu(getId());
            return true;
        }
        if (isSettingsEditor()) closeAllMenu(item->getId(),param);
        else doCloseMenu(item->getId(), true, param);
        return true;
	}
}

const lvRect & CRMenu::getRect()
{
    lvPoint sz = getSize();
    lvRect rc = _wm->getScreen()->getRect();
    _rect = rc;
    _rect.top = _rect.bottom - sz.y;
    _rect.right = _rect.left + sz.x;
    return _rect;
}

void CRMenu::draw()
{
	_pages = _pageItems>0 ? (_items.length()+_pageItems-1)/_pageItems : 0;
	_page = _pages>0 ? _topItem/_pageItems+1 : 0;
	_caption = _label;
        _icon = getItemIcon();

	if (_pageUpdate) {
		CRGUIWindowBase::draw();
	} else {
	    drawTitleBar();
		drawClient();
		drawStatusBar();
	}
}

LVImageSourceRef CRMenu::getImage(const lChar32 * imageId)
{
    lString32 fn( imageId );
    CRSkinRef skin = _wm->getSkin();
    if( !skin.isNull() && !fn.empty() ) {
        int extPos = fn.pos(".png");
        if( -1 == extPos) {
            fn.append(U".png");
        }
        return skin->getImage(fn);
    }
    return LVImageSourceRef();
}

static bool readNextLine( const LVStreamRef & stream, lString32 & dst )
{
    lString8 line;
    bool flgComment = false;
    for ( ; ; ) {
        int ch = stream->ReadByte();
        if ( ch<0 )
            break;
        if ( ch=='#' && line.empty() )
            flgComment = true;
        if ( ch=='\r' || ch=='\n' ) {
            if ( flgComment ) {
                flgComment = false;
                line.clear();
            } else {
                                if ( line[0]==(lChar8)0xEf && line[1]==(lChar8)0xBB && line[2]==(lChar8)0xBF )
					line.erase( 0, 3 );
                if ( !line.empty() ) {
                    dst = Utf8ToUnicode( line );
                    return true;
                }
            }
        } else {
            line << ch;
        }
    }
    return false;
}

static bool splitLine( lString32 line, const lString32 & delimiter, lString32 & key, lString32 & value )
{
    if ( !line.empty() ) {
        int n = line.pos(delimiter);
        value.clear();
        key = line;
        if ( n>0 && n <line.length()-1 ) {
            value = line.substr( n+1, line.length() - n - 1 );
            key = line.substr( 0, n );
            key.trim();
            value.trim();
            return key.length()!=0 && value.length()!=0;
        }
    }
    return false;
}

static int decodeKey( lString32 name )
{
    name.trim();
    if ( name.empty() )
        return 0;
    int key = 0;
    lChar32 ch0 = name[0];
    if ( ch0 >= '0' && ch0 <= '9' )
        return name.atoi();
    if ( ch0=='-' && name.length()>=2 && name[1] >= '0' && name[1] <= '9' )
        return name.atoi();
    if ( name.length()==3 && name[0]=='\'' && name[2]=='\'' )
        key = name[1];
    if ( name.length() == 1 )
        key = name[0];
    if ( key == 0 && name.length()>=4 && name[0]=='0' && name[1]=='x' ) {
        for ( int i=2; i<name.length(); i++ ) {
            lChar32 ch = name[i];
            if ( ch>='0' && ch<='9' )
                key = key*16 + (ch-'0');
            else if ( ch>='a' && ch<='f' )
                key = key*16 + (ch-'a') + 10;
            else if ( ch>='A' && ch<='F' )
                key = key*16 + (ch-'A') + 10;
            else
                break;
        }
    }
    return key;
}

bool CRGUIAcceleratorTableList::openFromStream( LVStreamRef defStream, LVStreamRef mapStream)
{
    _table.clear();
    LVHashTable<lString32, int> defs( 256 );

    if ( defStream.isNull() ) {
        CRLog::error( "cannot open keymap def file" );
        return false;
    }
    if ( mapStream.isNull() ) {
        CRLog::error( "cannot open keymap file" );
        return false;
    }
    lString32 line;
    while ( readNextLine(defStream, line) ) {
        lString32 name;
        lString32 value;
        if ( splitLine( line, cs32("="), name, value ) )  {
            int key = decodeKey( value );
            if ( key!=0 )
                defs.set( name, key );
            else
                CRLog::error("Unknown key definition in line %s", UnicodeToUtf8(line).c_str() );
        } else if ( !line.empty() )
            CRLog::error("Invalid definition in line %s", UnicodeToUtf8(line).c_str() );
    }
    if ( !defs.length() ) {
        CRLog::error("No definitions read from defFile");
        return false;

    }
    lString32 section;
    CRGUIAcceleratorTableRef table( new CRGUIAcceleratorTable() );
    bool eof = false;
    do {
        eof = !readNextLine(mapStream, line);
        if ( eof || (!line.empty() && line[0]=='[') ) {
            // eof or [section] found
            // save old section
            if ( !section.empty() ) {
                if ( table->length() ) {
                    _table.set( section, table );
                }
                section.clear();
            }
            // begin new section
            if ( !eof ) {
                table = CRGUIAcceleratorTableRef( new CRGUIAcceleratorTable() );
                int endbracket = line.pos( cs32("]") );
                if ( endbracket<=0 )
                    endbracket = line.length();
                if ( endbracket >= 2 )
                    section = line.substr( 1, endbracket - 1 );
                else
                    section.clear(); // wrong sectino
            }
        } else if ( !section.empty() ) {
            // read definition
            lString32 name;
            lString32 value;
            if ( splitLine( line, cs32("="), name, value ) ) {
                int flag = 0;
                int key = 0;
                lString32 keyName;
                lString32 flagName;
                splitLine( name, cs32(","), keyName, flagName );
                if ( !flagName.empty() ) {
                    flag = decodeKey( flagName );
                    if ( !flag )
                        flag = defs.get( flagName );
                }
                // decoding key name
                key = decodeKey( keyName );
                if ( !key )
                    key = defs.get( keyName );
                if ( !key ) {
                    CRLog::error( "unknown key definition %s in line %s", UnicodeToUtf8(keyName).c_str(), UnicodeToUtf8(line).c_str() );
                    continue;
                }
                int cmd = 0;
                int cmdParam = 0;
                lString32 cmdName;
                lString32 paramName;
                splitLine( value, cs32(","), cmdName, paramName );
                if ( !paramName.empty() ) {
                    cmdParam = decodeKey( paramName );
                    if ( !cmdParam )
                        cmdParam = defs.get( paramName );
                }
                cmd = decodeKey( cmdName );
                if ( !cmd )
                    cmd = defs.get( cmdName );
                if ( key != 0 && cmd != 0 ) {
                    // found valid key cmd definition
                    table->add( key, flag, cmd, cmdParam );
                    //CRLog::trace("Acc: %d, %d => %d, %d", key, flag, cmd, cmdParam);
                } else {
                    CRLog::error( "unknown command definition %s in line %s", UnicodeToUtf8(cmdName).c_str(), UnicodeToUtf8(line).c_str() );
                    continue;
                }
            }
        }
    } while ( !eof );

    return !empty();
}

bool CRGUIAcceleratorTableList::openFromFile( const char  * defFile, const char * mapFile )
{
    return openFromStream( LVOpenFileStream( defFile, LVOM_READ ),
                           LVOpenFileStream( mapFile, LVOM_READ ));
}

// get currently set layout
CRKeyboardLayoutRef CRKeyboardLayoutList::getCurrentLayout()
{
	if ( !_current.isNull() )
		return _current;
    _current = get(cs32("english"));
	if ( !_current )
		nextLayout();
	return _current;
}

// get next layout
CRKeyboardLayoutRef CRKeyboardLayoutList::prevLayout()
{
	bool found = false;
	CRKeyboardLayoutRef prev;
	CRKeyboardLayoutRef next;
	CRKeyboardLayoutRef first;
	CRKeyboardLayoutRef last;
	LVHashTable<lString32, CRKeyboardLayoutRef>::iterator i( _table );
	for ( ;; ) {
		LVHashTable<lString32, CRKeyboardLayoutRef>::pair * item = i.next();
		if ( !item )
			break;
		if ( first.isNull() )
			first = item->value;
		last = item->value;
		if ( item->value.get() == _current.get() ) {
			found = true;
		} else {
			if ( !found )
				prev = item->value;
			if ( !found && !next.isNull() )
				next = item->value;

		}
	}
	if ( prev.isNull() )
		_current = last;
	else
		_current = prev;
	return _current;
}

// get next layout
CRKeyboardLayoutRef CRKeyboardLayoutList::nextLayout()
{
	bool found = false;
	CRKeyboardLayoutRef prev;
	CRKeyboardLayoutRef next;
	CRKeyboardLayoutRef first;
	CRKeyboardLayoutRef last;
	LVHashTable<lString32, CRKeyboardLayoutRef>::iterator i( _table );
	for ( ;; ) {
		LVHashTable<lString32, CRKeyboardLayoutRef>::pair * item = i.next();
		if ( !item )
			break;
		if ( first.isNull() )
			first = item->value;
		last = item->value;
		if ( item->value.get() == _current.get() ) {
			found = true;
		} else {
			if ( !found )
				prev = item->value;
			else if ( next.isNull() )
				next = item->value;

		}
	}
	if ( next.isNull() )
		_current = first;
	else
		_current = next;
	return _current;
}

bool CRKeyboardLayoutList::openFromFile( const char  * layoutFile )
{
    //_table.clear();
    LVStreamRef stream = LVOpenFileStream( layoutFile, LVOM_READ );
    if ( stream.isNull() ) {
        CRLog::error( "cannot open keyboard layout file %s", layoutFile );
        return false;
    }
    lString32 line;
    lString32 section;
	CRKeyboardLayoutRef table;
	LVRef<CRKeyboardLayout> layout;
    bool eof = false;
    do {
        eof = !readNextLine(stream, line);
        if ( eof || (!line.empty() && line[0]=='[') ) {
            // eof or [section] found
            // save old section
            if ( !section.empty() ) {
                if ( layout->getItems().length() ) {
                    _table.set( section, table );
                }
                section.clear();
            }
            // begin new section
            if ( !eof ) {
                int endbracket = line.pos( cs32("]") );
                if ( endbracket<=0 )
                    endbracket = line.length();
                if ( endbracket >= 2 )
                    section = line.substr( 1, endbracket - 1 );
                else
                    section.clear(); // wrong sectino
				lString32 langname;
				lString32 layouttype;
                if ( !section.empty() && splitLine( section, cs32("."), langname, layouttype ) ) {
					table = _table.get( langname );
					if ( table.isNull() ) {
						table = CRKeyboardLayoutRef( new CRKeyboardLayoutSet() );
						_table.set( langname, table );
					}
                    if (layouttype == "tx")
						layout = table->tXKeyboard;
					else
						layout = table->vKeyboard;
				} else
					section.clear();
            }
        } else if ( !section.empty() ) {
            // read definition
            lString32 name;
            lString32 value;
            if ( splitLine( line, cs32("="), name, value ) ) {
                if (name == "enabled") {
					//if ( value == U"0" )
					//	; //TODO:set disabled flag
					continue;
				}
                int item;
				if ( !name.atoi(item) )
					continue;
				layout->set( item, value );
            }
        }
    } while ( !eof );
	return _table.length()>0;
}



#if CR_INTERNAL_PAGE_ORIENTATION==1
static int inv_control_table[] = {
    // old cmd, new cmd, param multiplier
    DCMD_LINEUP, DCMD_LINEDOWN, 1,
    DCMD_LINEDOWN, DCMD_LINEUP, 1,
    DCMD_PAGEUP, DCMD_PAGEDOWN, 1,
    DCMD_PAGEDOWN, DCMD_PAGEUP, 1,
    DCMD_MOVE_BY_CHAPTER, DCMD_MOVE_BY_CHAPTER, -1,
    0, 0, 0, 0,
};
#endif

/// returns true if command is processed
bool CRDocViewWindow::onCommand( int command, int params )
{
    if ( (command >= LVDOCVIEW_COMMANDS_START && command <= LVDOCVIEW_COMMANDS_END) ||
         command == DCMD_REFRESH_PAGE || command == DCMD_TOGGLE_BOLD ) {
        // TODO: rework controls inversion
#if CR_INTERNAL_PAGE_ORIENTATION==1
        cr_rotate_angle_t a = _docview->GetRotateAngle();
		if ( a==CR_ROTATE_ANGLE_90 || a==CR_ROTATE_ANGLE_180 ) {
			// inverse controls
			for ( int i=0; inv_control_table[i]; i+=3 ) {
				if ( command == inv_control_table[i] ) {
					command = inv_control_table[i+1];
					params *= inv_control_table[i+2];
					break;
				}
			}
		}
#endif
         if (DCMD_BUTTON_PRESSED == command || DCMD_BUTTON_PRESSED_LONG == command) {
             switch (params) {
             case BTN_SCROLL_DOWN:
                 command = DCMD_PAGEDOWN;
                 params = 1;
                 break;
             case BTN_SCROLL_UP:
                 command = DCMD_PAGEUP;
                 params = 1;
                 break;
             default:
                 return false;
             }
         }
        _docview->doCommand( (LVDocCmd)command, params );
        _dirty = true;
        return true;
    }
    return !_passCommandsToParent;
}


bool CRGUIKeyDownEvent::handle( CRGUIWindow * window )
{
    if ( _targetWindow!=NULL ) {
        if ( window!=_targetWindow )
            return false;
    }
    CRGUIWindowManager * wm = window->getWindowManager();
    bool res = window->onKeyPressed( _param1, _param2 );
    if ( res )
        wm->postEvent( new CRGUIUpdateEvent(false) );
    return res;
}

bool CRGUICommandEvent::handle( CRGUIWindow * window )
{
    if ( _targetWindow!=NULL ) {
        if ( window!=_targetWindow )
            return false;
    }
    CRGUIWindowManager * wm = window->getWindowManager();
    wm->forwardSystemEvents(false);
    bool res = window->onCommand( _param1, _param2 );
    wm->forwardSystemEvents(false);
    if ( res )
        wm->postEvent( new CRGUIUpdateEvent(false) );
    return res;
}

bool CRGUITouchEvent::handle(CRGUIWindow *window)
{
    if ( _targetWindow!=NULL ) {
        if ( window!=_targetWindow )
            return false;
    }
    CRGUIWindowManager * wm = window->getWindowManager();
    bool res = window->onTouchEvent( _param1, _param2, _type );
    if ( res )
        wm->postEvent( new CRGUIUpdateEvent(false) );
    return res;
}

CRToolBar::CRToolBar(CRGUIWindowBase *window, lString32 id, CRToolBarSkinRef tbskin, bool active) :
    m_window(window), m_currentButton(0), m_active(active), _controlsCreated(false),
    m_id(id), _tbSkin(tbskin)
{
}

bool CRToolBar::init(lString32 id)
{
    CRGUIAcceleratorTableRef buttons = m_window->getWindowManager()->getSkinAccTables().get( id);
    if (buttons.isNull())
        return false;

    for (int i=0; i < buttons->length(); i++) {
        int key = i+1;
        int cmd;
        int param;
        if (buttons->translate( key, 0, cmd, param)) {
            int cmdLong = DCMD_BUTTON_PRESSED_LONG;
            int paramLong = -1;
            buttons->translate( key, 1, cmdLong, paramLong);
            addButton( new CRToolButton(this, key, lString32(), cmd, param, paramLong, cmdLong));
        }
    }
    return ( m_buttons.length() != 0 );
}

bool CRToolBar::selectFirstButton()
{
    return selectButton(0, 1);
}

bool CRToolBar::selectLastButton()
{
    return selectButton(m_buttons.length() - 1, -1);
}

bool CRToolBar::selectNextButton(bool wrapAround)
{
    if (-1 == getCurrentButtonIndex())
        return selectFirstButton();
    if ( selectButton( m_currentButton+1, 1 ) )
        return true;
    if ( wrapAround )
        return selectFirstButton();
    return false;
}

bool CRToolBar::selectPrevButton(bool wrapAround)
{
    if (-1 == getCurrentButtonIndex())
        return selectLastButton();
    if ( selectButton( m_currentButton-1, -1) )
        return true;
    if ( wrapAround )
        return selectLastButton();
    return false;
}

bool CRToolBar::selectButton(int index)
{
    if ( index>=0 && index<getButtonsCount() &&
        m_buttons[index]->isEnabled() ) {
        setActive(true);
        if (index != m_currentButton)
            m_window->setDirty();
        m_currentButton = index;
        return true;
    }
    return false;
}

bool CRToolBar::selectButton(int from, int delta)
{
    for( int i=from; i>=0 && i<m_buttons.length(); i+=delta)
    {
        if ( selectButton(i) ) {
            return true;
        }
    }
    return false;
}

int CRToolBar::getCurrentButtonIndex()
{
    if (isActive()) {
        return m_currentButton;
    }
    return -1;
}

bool CRToolBar::isEnabled(int index)
{
    if ( index>=0 && index<getButtonsCount() )
        return m_buttons[index]->isEnabled();
    return false;
}

bool CRToolBar::setEnabled(int index, bool value)
{
    if ( index>=0 && index<getButtonsCount() )
        return m_buttons[index]->setEnabled(value);
    return false;
}

void CRToolBar::draw(CRToolBarSkinRef tbskin, LVDrawBuf &buf, const lvRect &rect)
{
    if ( tbskin.isNull() )
        return;
    tbskin->draw(buf, rect);

    lvRect rc = rect;
    rc.shrinkBy( tbskin->getBorderWidths() );
    CRButtonListRef buttons = tbskin->getButtons();
    int width = 0;
    for ( int i=0; i<buttons->length(); i++ ) {
        int flags = isEnabled(i) ? CRButtonSkin::ENABLED : 0;
        if ( i==getCurrentButtonIndex() && ( flags & CRButtonSkin::ENABLED ) )
            flags |= CRButtonSkin::SELECTED;
        LVRef<CRButtonSkin> button = buttons->get(i);
        if ( !button.isNull() ) {
            width += button->getMinSize().x;
        }
    }
    int deltaX = rc.width() - width;
    int offsetX = 0;
    if (deltaX > 0) {
        if ( tbskin->getHAlign()==SKIN_HALIGN_RIGHT )
            offsetX = deltaX;
        else if ( tbskin->getHAlign()==SKIN_HALIGN_CENTER )
            offsetX = deltaX/2;
    }
    int h = rc.height();
    CRToolBarControl *toolBarControl = NULL;
    if ( !_controlsCreated)
        toolBarControl = new CRToolBarControl(m_window, this, rect);
    for ( int i=0; i<buttons->length() && i < m_buttons.length(); i++ ) {
        lvRect rc2 = rc;
        int flags = isEnabled(i) ? CRButtonSkin::ENABLED : 0;
        if ( i==getCurrentButtonIndex() && ( flags & CRButtonSkin::ENABLED ) )
            flags |= CRButtonSkin::SELECTED;
        CRButtonSkinRef buttonSkin = buttons->get(i);
        if ( !buttonSkin.isNull() ) {
            rc2.left += offsetX;
            if (rc2.left >= rc.right)
                break;
            rc2.right = rc2.left + buttonSkin->getMinSize().x;
            if (rc2.right > rc.right)
                rc2.right = rc.right;
            int imgh = buttonSkin->getMinSize().y;
            int deltaY = h - imgh;
            if ( deltaY > 0) {
                if ( tbskin->getVAlign()==SKIN_VALIGN_BOTTOM )
                    rc2.top += deltaY;
                else if ( tbskin->getVAlign()==SKIN_VALIGN_CENTER ) {
                    rc2.top += (deltaY/2);
                    rc2.bottom = rc2.top + imgh;
                } else
                    rc2.bottom = rc2.top + imgh;
            }
            if ( NULL != toolBarControl) {
                CRToolButton *button = m_buttons[i];
                toolBarControl->addButton(new CRButtonControl(m_window, rc2, buttonSkin,
                                                              button->getId(),
                                                              button->getCommand(),
                                                              button->getParam(),
                                                              button->getParam(true),
                                                              button->getCommand(true)));
            }
            buttonSkin->drawButton( buf, rc2, flags );
            offsetX = rc2.right - rc.left;
        }
    }
    if ( NULL != toolBarControl ) {
        m_window->addControl(toolBarControl);
        _controlsCreated = true;
    }
}

int CRToolBar::findButton(int command, int param)
{
    int ret = -1;

    for ( int i=0; i<m_buttons.length() && ret==-1; i++) {
        CRToolButton *current = m_buttons[i];
        if (current->getCommand() == command && current->getParam() == param) {
            ret = i;
        }
    }
    return ret;
}

bool CRToolBar::setActive(bool value)
{
    if ( m_active != value ) {
        m_active = value;
        return true;
    }
    return false;
}

bool CRToolBar::getRect(lvRect &rc)
{
    if ( !_tbSkin.isNull() ) {
        lvRect rect = m_window->getWindowManager()->getScreen()->getRect();
        return _tbSkin->getRect(rc, rect);
    }
    return false;
}

CRToolButton * CRToolBar::getCurrentButton()
{
    int index = getCurrentButtonIndex();
    if (index != -1)
        return m_buttons[index];
    return NULL;
}

void CRToolBarControl::addButton(CRButtonControl *button)
{
    m_buttons.add( button );
}

bool CRToolBarControl::hitTest(lvPoint &pt)
{
    m_touchIndex = -1;
    if ( CRGUIControl::hitTest(pt) ) {
        for (int i = 0; i < m_buttons.length(); i++) {
            CRButtonControl *button = m_buttons[i];
            if ( button->hitTest(pt) ) {
                m_touchIndex = i;
                break;
            }
        }
        return true;
    }
    return false;
}

bool CRToolBarControl::onTouch(CRGUITouchEventType evType)
{
    if (m_touchIndex != -1) {
        if (m_toolbar->getCurrentButtonIndex() != m_touchIndex)
            setSelected(true);
        return m_buttons[m_touchIndex]->onTouch(evType);
    }
    return false;
}

bool CRToolBarControl::setSelected(bool selected)
{
    if (m_touchIndex != -1 && selected) {
        if ( m_toolbar->selectButton(m_touchIndex) )
            m_buttons[m_touchIndex]->setSelected(selected);
    }
    return m_toolbar->setActive(selected);
}

CRWindowSkinRef CRGUIWindowBase::getSkin()
{
    if ( !_skin.isNull() )
        return _skin;
    lString32 path = getSkinName();
    if (!path.startsWith("#"))
        path = cs32("/CR3Skin/") + path;
    if ( _wm->getScreenOrientation()&1 ) {
        _skin = _wm->getSkin()->getWindowSkin( (path + "-rotated").c_str() );
        if ( _skin.isNull() && !_baseSkinName.empty() ) {
            _skin = _wm->getSkin()->getWindowSkin( (_baseSkinName + "-rotated").c_str() );
        }
    }
    if ( !_skin )
        _skin = _wm->getSkin()->getWindowSkin( path.c_str() );
    return _skin;
}

void CRGUIWindowManager::reconfigureWindows(int flags)
{
    for ( int i=_windows.length()-1; i>=0; i-- ) {
        CRMenu *menu = dynamic_cast<CRMenu *>(_windows[i]);
        if (NULL == menu || menu->isMainMenu() )
            _windows[i]->reconfigure( flags );
    }
}

void CRGUIWindowManager::loadSkinKeymaps()
{
    _skinAccTables.clear();

    if ( !_skin.isNull() ) {
        lString32 keymapDir = LVExtractPath(getKeymapFilePath());
        lString32 defFile = LVCombinePaths(keymapDir, cs32("keydefs.ini"));
        LVStreamRef keymapFile = _skin->getStream(U"keymaps.ini");

        _skinAccTables.openFromStream(LVOpenFileStream( defFile.c_str(), LVOM_READ ),
                                      keymapFile);
    }
}
