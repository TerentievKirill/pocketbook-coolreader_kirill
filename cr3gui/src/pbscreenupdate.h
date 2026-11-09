#ifndef PB_SCREEN_UPDATE_H
#define PB_SCREEN_UPDATE_H

#include "lvdrawbuf.h"
#include "pbgrayupdate.h"
#include <cstring>

inline void pbPackGray4(LVDrawBuf &src, unsigned char *dest, int x, int y, int width, int height)
{
    int stride = (width + 1) / 2;
    for (int row = 0; row < height; ++row) {
        const unsigned char *line = src.GetScanLine(y + row);
        for (int i = 0; i < stride; ++i) {
            int col = x + 2 * i;
            dest[i] = line[col] & 0xf0;
            if (col + 1 < x + width) dest[i] |= line[col + 1] >> 4;
        }
        dest += stride;
    }
}

// Synchronize only bytes covered by the dirty rectangle. Packed pixels expand
// to byte boundaries; row padding and unrelated columns remain untouched.
inline lvRect pbSyncScreenRegion(LVDrawBuf &canvas, LVDrawBuf &front,
                                 lvRect dirty, bool &gray)
{
    gray = false;
    lvRect changed;
    int bits = canvas.GetBitsPerPixel();
    int ppb = bits == 1 ? 8 : bits == 2 ? 4 : 1;
    int width = canvas.GetWidth();
    if (dirty.left < 0) dirty.left = 0;
    if (dirty.right > width) dirty.right = width;
    if (dirty.top < 0) dirty.top = 0;
    if (dirty.bottom > canvas.GetHeight()) dirty.bottom = canvas.GetHeight();
    if (dirty.isEmpty()) return changed;
    int start = dirty.left / ppb;
    int end = (dirty.right + ppb - 1) / ppb;
    for (int y = dirty.top; y < dirty.bottom; ++y) {
        unsigned char *next = canvas.GetScanLine(y);
        unsigned char *old = front.GetScanLine(y);
        if (!memcmp(next + start, old + start, end - start)) continue;
        int first = start, last = end;
        while (next[first] == old[first]) ++first;
        while (next[last - 1] == old[last - 1]) --last;
        for (int i = first; !gray && i < last; ++i) {
            if (next[i] == old[i]) continue;
            if (bits == 2) gray = pbGray2Changed(old[i], next[i]);
            else if (bits == 3) gray = true;
            else if (bits == 4)
                gray = (old[i] != 0 && old[i] != 0xf0) || (next[i] != 0 && next[i] != 0xf0);
            else if (bits == 8)
                gray = (old[i] > 64 && old[i] < 192) || (next[i] > 64 && next[i] < 192);
        }
        int right = last * ppb;
        if (right > width) right = width;
        lvRect line(first * ppb, y, right, y + 1);
        changed.extend(line);
        memcpy(old + first, next + first, last - first);
    }
    return changed;
}

#endif
