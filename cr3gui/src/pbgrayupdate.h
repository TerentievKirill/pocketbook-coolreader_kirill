#ifndef PB_GRAY_UPDATE_H
#define PB_GRAY_UPDATE_H

// Packed 2-bit pixels: a changed gray pixel on either side needs grayscale.
inline bool pbGray2Changed(unsigned char before, unsigned char after)
{
    if (before == after) return false;
    for (int shift = 0; shift < 8; shift += 2) {
        int a = (before >> shift) & 3;
        int b = (after >> shift) & 3;
        if (a != b && (a == 1 || a == 2 || b == 1 || b == 2)) return true;
    }
    return false;
}

#endif
