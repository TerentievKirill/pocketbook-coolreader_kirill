/***************************************************************************
 *   CoolReader engine                                                     *
 *   Copyright (C) 2007,2008 Vadim Lopatin <coolreader.org@gmail.com>      *
 *   Copyright (C) 2020 Aleksey Chernov <valexlin@gmail.com>               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU General Public License           *
 *   as published by the Free Software Foundation; either version 2        *
 *   of the License, or (at your option) any later version.                *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the Free Software           *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,            *
 *   MA 02110-1301, USA.                                                   *
 ***************************************************************************/

#ifndef __LVDIRECTORYCONTAINER_H_INCLUDED__
#define __LVDIRECTORYCONTAINER_H_INCLUDED__

#include "lvnamedcontainer.h"

class LVDirectoryContainerItemInfo : public LVCommonContainerItemInfo
{
    friend class LVDirectoryContainer;
};

class LVDirectoryContainer : public LVNamedContainer
{
    bool _listOnDemand;
    mutable LVContainerRef _listing;
    const LVContainerRef &listing() const;
    LVDirectoryContainer(lString32 path, bool listOnDemand);
protected:
    LVDirectoryContainer * m_parent;
public:
    virtual LVStreamRef OpenStream( const char32_t * fname, lvopen_mode_t mode );
    virtual LVContainer * GetParentContainer()
    {
        return (LVContainer*)m_parent;
    }
    virtual const LVContainerItemInfo * GetObjectInfo(int index);
    virtual int GetObjectCount() const;
    virtual lverror_t GetSize( lvsize_t * pSize );
    virtual ~LVDirectoryContainer();
    static LVDirectoryContainer * OpenDirectory( const char32_t * path, const char32_t * mask = U"*.*" );
    static LVDirectoryContainer * OpenForLookup(const lString32 &path);
};

#endif  // __LVDIRECTORYCONTAINER_H_INCLUDED__
