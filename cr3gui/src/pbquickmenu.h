#ifndef CR3_PBQUICKMENU_H
#define CR3_PBQUICKMENU_H

#include "crskin.h"
#include <inkview.h>
#include <vector>

constexpr int pbQuickMenuIconCount = 7;
extern const char *const pbQuickMenuIconNames[pbQuickMenuIconCount];

struct PBQuickMenuLayout {
    int width, height, iconWidth, iconHeight, spacing, orientation;
    bool operator==(const PBQuickMenuLayout &other) const;
};

class PBQuickMenuIcons {
    class CanvasRegion {
        std::vector<lUInt8> _pixels;
    public:
        void capture(const icanvas &canvas, const lvRect &rect);
        bool restore(icanvas &canvas, const lvRect &rect) const;
        void clear() { _pixels.clear(); }
    };

    CRSkinRef _skin;
    PBQuickMenuLayout _layout{};
    int _scanline = 0;
    CanvasRegion _strips[2];
    CanvasRegion _pressed[pbQuickMenuIconCount + 1];
    icanvas *prepare(const CRSkinRef &skin, const PBQuickMenuLayout &layout);
public:
    void drawTop(const CRSkinRef &skin, const PBQuickMenuLayout &layout, bool bookmarked);
    bool drawPressed(const CRSkinRef &skin, const PBQuickMenuLayout &layout, bool bookmarked, int index);
};

#endif
