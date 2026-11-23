/***************************************************************************
 *   CoolReader engine                                                     *
 *   Copyright (C) 2007-2009,2012 Vadim Lopatin <coolreader.org@gmail.com> *
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

#include "lvdirectorycontainer.h"
#include "lvstreamutils.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

LVStreamRef LVDirectoryContainer::OpenStream(const char32_t *fname, lvopen_mode_t mode)
{
    lString32 fn = m_fname;
    fn << fname;
    if (_listOnDemand) {
        if ((mode & LVOM_MASK) != LVOM_READ) {
            const auto &dir = listing();
            return dir.isNull() ? LVStreamRef() : dir->OpenStream(fname, mode);
        }
#ifndef _WIN32
        // An exact pathname lookup avoids reading the directory. POSIX open()
        // alone would also accept a directory or block on a named pipe.
        const lString8 path = UnicodeToUtf8(fn);
#if defined(__linux__) && defined(_LARGEFILE64_SOURCE)
        struct stat64 info;
        if (stat64(path.c_str(), &info) || !S_ISREG(info.st_mode))
#else
        struct stat info;
        if (stat(path.c_str(), &info) || !S_ISREG(info.st_mode))
#endif
            return LVStreamRef();
#endif
        return LVOpenFileStream(fn.c_str(), mode);
    }

    int found_index = -1;
    for (int i=0; i<m_list.length(); i++) {
        if ( !lStr_cmp( fname, m_list[i]->GetName() ) ) {
            if ( m_list[i]->IsContainer() ) {
                // found directory with same name!!!
                return LVStreamRef();
            }
            found_index = i;
            break;
        }
    }
    //const char * fb8 = UnicodeToUtf8( fn ).c_str();
    //printf("Opening directory container file %s : %s fname=%s path=%s\n", UnicodeToUtf8( lString32(fname) ).c_str(), UnicodeToUtf8( fn ).c_str(), UnicodeToUtf8( m_fname ).c_str(), UnicodeToUtf8( m_path ).c_str());
    LVStreamRef stream( LVOpenFileStream( fn.c_str(), mode ) );
    if (!stream) {
        return stream;
    }
    //stream->m_parent = this;
    if (found_index<0) {
        // add new info
        LVDirectoryContainerItemInfo * item = new LVDirectoryContainerItemInfo;
        item->m_name = fname;
        stream->GetSize(&item->m_size);
        Add(item);
    }
    return stream;
}

const LVContainerRef &LVDirectoryContainer::listing() const
{
    if (_listing.isNull())
        _listing = LVContainerRef(OpenDirectory(m_fname.c_str()));
    return _listing;
}

const LVContainerItemInfo *LVDirectoryContainer::GetObjectInfo(int index)
{
    if (_listOnDemand) {
        const auto &dir = listing();
        return dir.isNull() ? NULL : dir->GetObjectInfo(index);
    }
    return index >= 0 && index < m_list.length() ? m_list[index] : NULL;
}

int LVDirectoryContainer::GetObjectCount() const
{
    if (_listOnDemand) {
        const auto &dir = listing();
        return dir.isNull() ? 0 : dir->GetObjectCount();
    }
    return m_list.length();
}

lverror_t LVDirectoryContainer::GetSize(lvsize_t *pSize)
{
    if (m_fname.empty())
        return LVERR_FAIL;
    *pSize = GetObjectCount();
    return LVERR_OK;
}

LVDirectoryContainer::LVDirectoryContainer(lString32 path, bool listOnDemand)
    : _listOnDemand(listOnDemand), m_parent(NULL)
{
    if (!path.endsWith("/") && !path.endsWith("\\"))
        path << m_path_separator;
    SetName(path.c_str());
}

LVDirectoryContainer::~LVDirectoryContainer()
{
    SetName(NULL);
    Clear();
}

LVDirectoryContainer *LVDirectoryContainer::OpenForLookup(const lString32 &path)
{
    return path.empty() ? NULL : new LVDirectoryContainer(path, true);
}

LVDirectoryContainer *LVDirectoryContainer::OpenDirectory(const char32_t *path, const char32_t *mask)
{
    if (!path || !path[0])
        return NULL;

    LVDirectoryContainer *dir = new LVDirectoryContainer(lString32(path), false);
    lString32 fn = dir->m_fname;

#if !defined(__SYMBIAN32__) && defined(_WIN32)
    // WIN32 API
    fn << mask;
    WIN32_FIND_DATAW data = { 0 };
    WIN32_FIND_DATAA dataa = { 0 };
    //lString8 bs = DOMString(path).ToAnsiString();
    lString16 fn16 = UnicodeToUtf16(fn);
    HANDLE hFind = FindFirstFileW(fn16.c_str(), &data);
    bool unicode=true;
    if (hFind == INVALID_HANDLE_VALUE || !hFind) {
        lUInt32 err=GetLastError();
        if (err == ERROR_CALL_NOT_IMPLEMENTED) {
            hFind = FindFirstFileA(UnicodeToLocal(fn).c_str(), &dataa);
            unicode=false;
            if (hFind == INVALID_HANDLE_VALUE || !hFind)
            {
                delete dir;
                return NULL;
            }
        } else {
            delete dir;
            return NULL;
        }
    }
    
    if (unicode) {
        // unicode
        for (;;) {
            lUInt32 dwAttrs = data.dwFileAttributes;
            wchar_t * pfn = data.cFileName;
            for (int i=0; data.cFileName[i]; i++) {
                if (data.cFileName[i]=='/' || data.cFileName[i]=='\\')
                    pfn = data.cFileName + i + 1;
            }
            
            if ( (dwAttrs & FILE_ATTRIBUTE_DIRECTORY) ) {
                // directory
                if (!lStr_cmp(pfn, L"..") || !lStr_cmp(pfn, L".")) {
                    // .. or .
                } else {
                    // normal directory
                    LVDirectoryContainerItemInfo * item = new LVDirectoryContainerItemInfo;
                    item->m_name = Utf16ToUnicode(pfn);
                    item->m_is_container = true;
                    dir->Add(item);
                }
            } else {
                // file
                LVDirectoryContainerItemInfo * item = new LVDirectoryContainerItemInfo;
                item->m_name = Utf16ToUnicode(pfn);
                item->m_size = data.nFileSizeLow;
                item->m_flags = data.dwFileAttributes;
                dir->Add(item);
            }
            
            if (!FindNextFileW(hFind, &data)) {
                // end of list
                break;
            }
            
        }
    } else {
        // ANSI
        for (;;) {
            lUInt32 dwAttrs = dataa.dwFileAttributes;
            char * pfn = dataa.cFileName;
            for (int i=0; dataa.cFileName[i]; i++) {
                if (dataa.cFileName[i]=='/' || dataa.cFileName[i]=='\\')
                    pfn = dataa.cFileName + i + 1;
            }
            
            if ( (dwAttrs & FILE_ATTRIBUTE_DIRECTORY) ) {
                // directory
                if (!strcmp(pfn, "..") || !strcmp(pfn, ".")) {
                    // .. or .
                } else {
                    // normal directory
                    LVDirectoryContainerItemInfo * item = new LVDirectoryContainerItemInfo;
                    item->m_name = LocalToUnicode( lString8( pfn ) );
                    item->m_is_container = true;
                    dir->Add(item);
                }
            } else {
                // file
                LVDirectoryContainerItemInfo * item = new LVDirectoryContainerItemInfo;
                item->m_name = LocalToUnicode( lString8( pfn ) );
                item->m_size = data.nFileSizeLow;
                item->m_flags = data.dwFileAttributes;
                dir->Add(item);
            }
            
            if (!FindNextFileA(hFind, &dataa)) {
                // end of list
                break;
            }
            
        }
    }
    
    FindClose( hFind );
#else
    // POSIX
    lString32 p( fn );
    LVRemovePathDelimiter(p);
    lString8 p8 = UnicodeToLocal( p );
    if ( p8.empty() )
        p8 = ".";
    const char * p8s = p8.c_str();
    DIR * d = opendir(p8s);
    if ( d ) {
#if defined(__linux__) && defined(_LARGEFILE64_SOURCE)
        // On 32-bit builds, readdir() can reject a directory's inode or offset.
        while (struct dirent64 *pde = readdir64(d)) {
            lString8 fpath = p8 + "/" + pde->d_name;
            struct stat64 st;
            if (stat64(fpath.c_str(), &st))
                continue;
#else
        while (struct dirent *pde = readdir(d)) {
            lString8 fpath = p8 + "/" + pde->d_name;
            struct stat st;
            if (stat(fpath.c_str(), &st))
                continue;
#endif
            if ( S_ISDIR(st.st_mode) ) {
                // dir
                if ( strcmp(pde->d_name, ".") && strcmp(pde->d_name, "..") ) {
                    // normal directory
                    LVDirectoryContainerItemInfo * item = new LVDirectoryContainerItemInfo;
                    item->m_name = LocalToUnicode(lString8(pde->d_name));
                    item->m_is_container = true;
                    dir->Add(item);
                }
            } else if ( S_ISREG(st.st_mode) ) {
                // file
                LVDirectoryContainerItemInfo * item = new LVDirectoryContainerItemInfo;
                item->m_name = LocalToUnicode(lString8(pde->d_name));
                item->m_size = st.st_size;
                item->m_flags = st.st_mode;
                dir->Add(item);
            }
        }
        closedir(d);
    } else {
        delete dir;
        return NULL;
    }
    
    
#endif
    return dir;
}
