#ifndef LV_FRONTLIGHT_ICON_H
#define LV_FRONTLIGHT_ICON_H
#include "lvdrawbuf.h"
// Compact brightness/sun symbol: filled centre and eight rays; slashed when off.
// Use the current status colour so the same image also works in inverted mode.
inline void LVDrawFrontlightIcon(LVDrawBuf *buf,int x,int y,int size,bool enabled,lUInt32 color) {
    for(int row=0;row<16;++row) for(int col=0;col<16;++col) {
        int dx=2*col-15,dy=2*row-15;
        int ax=dx<0 ? -dx : dx,ay=dy<0 ? -dy : dy;
        int radius=dx*dx+dy*dy;
        bool centre=radius<=64 && (enabled || radius>=25);
        bool ray=(ax<=1 && ay>=11) || (ay<=1 && ax>=11) ||
            (ax>=9 && ax<=11 && ay>=9 && ay<=11);
        bool slash=!enabled && row>=1 && row<=14 && col==15-row;
        if(centre || ray || slash)
            buf->FillRect(x+col*size/16,y+row*size/16,x+(col+1)*size/16,y+(row+1)*size/16,color);
    }
}
#endif
