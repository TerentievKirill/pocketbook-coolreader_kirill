#include "pbquickmenu.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <tuple>

ibitmap *LVImageSourceRef_to_ibitmab(LVImageSourceRef &img);

const char *const pbQuickMenuIconNames[pbQuickMenuIconCount] = {
    "home", "toc", "bookmarks", "dictionary", "search", "rotate", "settings"
};

namespace {
constexpr int bookmarkIcon = 2;

bool canvasContains(const icanvas &canvas, const lvRect &rect)
{
    return !rect.isEmpty() && rect.left >= 0 && rect.top >= 0 &&
           rect.right <= canvas.width && rect.bottom <= canvas.height &&
           rect.left >= canvas.clipx1 && rect.top >= canvas.clipy1 &&
           rect.right - 1 <= canvas.clipx2 && rect.bottom - 1 <= canvas.clipy2;
}

bool drawIcon(const CRSkinRef &skin, const lvRect &rect, int index, bool bookmarked, bool pressed)
{
    lString32 path = lString32("icon_") + pbQuickMenuIconNames[index];
    if (index == bookmarkIcon && bookmarked)
        path += "_alt";
    if (pressed)
        path += "_tap";
    path += ".png";
    LVImageSourceRef image = skin->getImage(path);
    if (image.isNull())
        return false;
    ibitmap *bitmap = LVImageSourceRef_to_ibitmab(image);
    if (!bitmap)
        return false;
    StretchBitmap(rect.left, rect.top, rect.width(), rect.height(), bitmap, 0);
    // Preserve the white strip that covers the scaler's right-edge artifact.
    const int whiteEdge = std::round(rect.width() * 0.04);
    FillArea(rect.right - whiteEdge, rect.top, whiteEdge, rect.height(), 0xFFFFFF);
    free(bitmap);
    return true;
}
}

bool PBQuickMenuLayout::operator==(const PBQuickMenuLayout &other) const
{
    return std::tie(width, height, iconWidth, iconHeight, spacing, orientation) ==
           std::tie(other.width, other.height, other.iconWidth, other.iconHeight, other.spacing, other.orientation);
}

void PBQuickMenuIcons::CanvasRegion::capture(const icanvas &canvas, const lvRect &rect)
{
    const int rowBytes = rect.width();
    _pixels.resize(static_cast<size_t>(rect.height()) * rowBytes);
    const lUInt8 *row = canvas.addr + rect.top * canvas.scanline + rect.left;
    for (int y = 0; y < rect.height(); ++y, row += canvas.scanline)
        memcpy(_pixels.data() + y * rowBytes, row, rowBytes);
}

bool PBQuickMenuIcons::CanvasRegion::restore(icanvas &canvas, const lvRect &rect) const
{
    if (_pixels.empty())
        return false;
    const int rowBytes = rect.width();
    lUInt8 *row = canvas.addr + rect.top * canvas.scanline + rect.left;
    for (int y = 0; y < rect.height(); ++y, row += canvas.scanline)
        memcpy(row, _pixels.data() + y * rowBytes, rowBytes);
    return true;
}

icanvas *PBQuickMenuIcons::prepare(const CRSkinRef &skin, const PBQuickMenuLayout &layout)
{
    icanvas *canvas = GetCanvas();
    // InkView can leave background pixels intact when drawing to packed canvases.
    if (!canvas || !canvas->addr || canvas->width != layout.width || canvas->height != layout.height ||
        canvas->depth != 8 || canvas->scanline < canvas->width)
        return NULL;
    if (_skin.get() != skin.get() || !(_layout == layout) || _scanline != canvas->scanline) {
        _skin = skin;
        _layout = layout;
        _scanline = canvas->scanline;
        for (CanvasRegion &strip : _strips)
            strip.clear();
        for (CanvasRegion &pressed : _pressed)
            pressed.clear();
    }
    return canvas;
}

void PBQuickMenuIcons::drawTop(const CRSkinRef &skin, const PBQuickMenuLayout &layout, bool bookmarked)
{
    icanvas *canvas = prepare(skin, layout);
    const lvRect strip(0, 0, layout.width, layout.iconHeight + 2);
    const bool canCache = canvas && canvasContains(*canvas, strip);
    CanvasRegion &cached = _strips[bookmarked];
    if (canCache && cached.restore(*canvas, strip))
        return;

    FillArea(0, 0, strip.width(), strip.height(), 0xFFFFFF);
    FillArea(0, layout.iconHeight, layout.width, 1, 0x000000);
    int x = layout.spacing;
    for (int index = 0; index < pbQuickMenuIconCount; ++index) {
        drawIcon(skin, lvRect(x, 0, x + layout.iconWidth, layout.iconHeight), index, bookmarked, false);
        x += layout.iconWidth + layout.spacing;
    }
    if (canCache)
        cached.capture(*canvas, strip);
}

bool PBQuickMenuIcons::drawPressed(const CRSkinRef &skin, const PBQuickMenuLayout &layout, bool bookmarked, int index)
{
    icanvas *canvas = prepare(skin, layout);
    const int x = layout.spacing + index * (layout.iconWidth + layout.spacing);
    const lvRect rect(x, 0, x + layout.iconWidth, layout.iconHeight);
    const bool canCache = canvas && canvasContains(*canvas, rect);
    CanvasRegion &cached = _pressed[index == bookmarkIcon && bookmarked ? pbQuickMenuIconCount : index];
    if (canCache && cached.restore(*canvas, rect))
        return true;
    if (!drawIcon(skin, rect, index, bookmarked, true))
        return false;
    if (canCache)
        cached.capture(*canvas, rect);
    return true;
}
