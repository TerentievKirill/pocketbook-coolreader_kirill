#ifndef CR3_DICTIONARY_UI_H
#define CR3_DICTIONARY_UI_H
#include "crgui.h"
#include <algorithm>

namespace dictview {
inline int titleHeight(const lvRect &window) {
    return std::min(window.height()/2,window.width()>=1000?144:112);
}
// The same rectangles drive painting and touch input, in every orientation.
struct Header {
    lvRect buttons[7];
    explicit Header(const lvRect &rc) {
        int middle = rc.top + rc.height()/2;
        int closeWidth = std::max(80, rc.width()/7);
        buttons[0] = lvRect(rc.left,rc.top,rc.right-closeWidth,middle);
        buttons[1] = lvRect(rc.right-closeWidth,rc.top,rc.right,middle);
        const int order[] = {6,4,5,2,3};
        for (int i=0;i<5;++i)
            buttons[order[i]] = lvRect(rc.left+rc.width()*i/5,middle,rc.left+rc.width()*(i+1)/5,rc.bottom);
    }
    int hit(int x,int y) const {
        for (int i=0;i<7;++i) if (buttons[i].isPointInside(lvPoint(x,y))) return i;
        return -1;
    }
};
struct Entries {
    lvRect area, previous, next;
    int count;
    explicit Entries(const lvRect &rc) : area(rc) {
        int footer = std::min(rc.width()>=1000?72:48, std::max(36,rc.height()/4));
        area.bottom -= footer;
        count = std::max(1,area.height()/(rc.width()>=1000?96:76));
        previous = lvRect(rc.left,area.bottom,rc.left+rc.width()/2,rc.bottom);
        next = lvRect(previous.right,area.bottom,rc.right,rc.bottom);
    }
    lvRect row(int i) const { return lvRect(area.left,area.top+area.height()*i/count,area.right,area.top+area.height()*(i+1)/count); }
    int hit(int x,int y,int available) const {
        if (!area.isPointInside(lvPoint(x,y))) return -1;
        for (int i=0;i<std::min(count,available);++i) if (row(i).isPointInside(lvPoint(x,y))) return i;
        return -1;
    }
};
inline LVFontRef font(int size,bool bold=false) {
    return fontMan->GetFont(size,bold?700:400,false,css_ff_sans_serif,lString8("Liberation Sans, DejaVu Sans"));
}
inline lString32 fit(lString32 text, LVFontRef f, int width) {
    if (width<=0 || f.isNull()) return lString32();
    if (f->getTextWidth(text.c_str(),text.length())<=static_cast<unsigned>(width)) return text;
    int low=0,high=text.length();
    while (low<high) {
        int middle=(low+high+1)/2;
        lString32 candidate=text.substr(0,middle)+U"…";
        if (f->getTextWidth(candidate.c_str(),candidate.length())<=static_cast<unsigned>(width)) low=middle;
        else high=middle-1;
    }
    return text.substr(0,low)+U"…";
}
inline void label(LVDrawBuf &buf,CRRectSkinRef skin,const lvRect &rc,const lString32 &text,int size,bool bold=false,bool center=false) {
    lvRect padded(rc); padded.left+=10; padded.right-=10;
    LVFontRef f=font(size,bold);
    skin->drawText(buf,padded,fit(text,f,padded.width()),f,0,0xffffff,
        SKIN_VALIGN_CENTER | (center?SKIN_HALIGN_CENTER:SKIN_HALIGN_LEFT));
}
inline void dropdown(LVDrawBuf &buf,const lvRect &rc) {
    // Draw the indicator ourselves: reader fonts may not contain U+25BE.
    int x=rc.right-24, y=(rc.top+rc.bottom)/2-4;
    for (int row=0;row<8;++row) buf.FillRect(x-8+row,y+row,x+8-row,y+row+1,0);
}
inline void header(LVDrawBuf &buf,CRRectSkinRef skin,const lvRect &rc,const lString32 &caption,int selected,bool active) {
    skin->draw(buf,rc);
    Header layout(rc);
    const char *labels[]={"",_("Close"),_("Google"),_("Wikipedia"),_("Entries"),_("Pick word"),_("Search")};
    int size=std::max(14,std::min(rc.width()>=1000?30:24,rc.height()/4));
    for (int i=0;i<7;++i) {
        const lvRect &r=layout.buttons[i];
        lvRect text(r);
        if (i==0) text.right-=36;
        label(buf,skin,text,i==0?caption:lString32(labels[i]),size,i==0,i!=0);
        if (i==0) dropdown(buf,r);
        buf.FillRect(r.left,r.bottom-1,r.right,r.bottom,0);
        if (i!=0) buf.FillRect(r.left,r.top,r.left+1,r.bottom,0);
        if (active && selected==i) buf.InvertRect(r.left+2,r.top+2,r.right-2,r.bottom-2);
    }
}
inline void entry(LVDrawBuf &buf,CRRectSkinRef skin,const lvRect &rc,const lString32 &word,const lString32 &preview,bool selected) {
    buf.FillRect(rc,0xffffff);
    lvRect title(rc),detail(rc); title.bottom=rc.top+rc.height()/2; detail.top=title.bottom;
    int size=std::max(16,std::min(34,rc.height()/3));
    label(buf,skin,title,word,size,true);
    label(buf,skin,detail,preview,size-3);
    buf.FillRect(rc.left,rc.bottom-1,rc.right,rc.bottom,0xaaaaaa);
    if (selected) buf.InvertRect(rc.left+2,rc.top+2,rc.right-2,rc.bottom-2);
}
inline lString32 entrySummary(lString32 text) {
    // A row should show the meaning before a dictionary's long list of IPA
    // variants. Pronunciation remains intact in the full article.
    int start=0;
    while (start<text.length() && text[start]==U'/') {
        int end=start+1;
        while (end<text.length() && text[end]!=U'/') ++end;
        if (end==text.length() || (end+1<text.length() && text[end+1]!=U' ' && text[end+1]!=U',')) break;
        int next=end+1;
        while (next<text.length() && (text[next]==U' ' || text[next]==U',')) ++next;
        if (next==text.length()) break;
        start=next;
    }
    if (start) text=text.substr(start);
    return text;
}
inline void navigation(LVDrawBuf &buf,CRRectSkinRef skin,const Entries &layout) {
    buf.FillRect(layout.previous,0xffffff); buf.FillRect(layout.next,0xffffff);
    label(buf,skin,layout.previous,lString32(_("‹ Previous")),24,false,true);
    label(buf,skin,layout.next,lString32(_("Next ›")),24,false,true);
    buf.FillRect(layout.area.left,layout.area.bottom,layout.area.right,layout.area.bottom+1,0);
    buf.FillRect(layout.next.left,layout.next.top,layout.next.left+1,layout.next.bottom,0);
}
inline lString32 name(lString32 raw) {
    lString32 plain=raw; plain.replace(U" (StarDict)",U"");
    const char *codes[]={"cs-cs","cs-en","cs-ru","en-en","en-cs","en-ru","ru-ru","ru-cs","ru-en","uk-uk"};
    const char *original[]={"reader.dict CS","čeština-English FreeDict+WikDict dictionary","čeština-Русский FreeDict+WikDict dictionary","reader.dict EN","English-čeština FreeDict+WikDict dictionary","English-Русский FreeDict+WikDict dictionary (en-ru)","reader.dict RU","Русский-čeština FreeDict+WikDict dictionary","Русский-English FreeDict+WikDict dictionary (ru-en)","reader.dict UK"};
    const char *names[]={_("Czech definitions"),_("Czech → English"),_("Czech → Russian"),_("English definitions"),_("English → Czech"),_("English → Russian"),_("Russian definitions"),_("Russian → Czech"),_("Russian → English"),_("Ukrainian definitions")};
    for (int i=0;i<10;++i) if (plain==lString32("ReaderTools ")+codes[i] || plain==lString32(original[i])) return lString32(names[i]);
    return raw;
}
}
#endif
