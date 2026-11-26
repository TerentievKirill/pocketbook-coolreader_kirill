#include "../cr3gui/src/pbquickmenu.h"
#include "testutil.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>

ibitmap *LVImageSourceRef_to_ibitmab(LVImageSourceRef &img);

// Draw the same icons through the original stretch-and-edge-fill path.
static bool referenceIcon(const CRSkinRef &skin, const PBQuickMenuLayout &layout,
                          int index, bool bookmarked, bool pressed)
{
    const char *name = pbQuickMenuIconNames[index];
    const bool alternate = strcmp(name, "bookmarks") == 0 && bookmarked;
    lString32 path = lString32("icon_") + name;
    path += pressed ? (alternate ? "_alt_tap.png" : "_tap.png") : (alternate ? "_alt.png" : ".png");
    LVImageSourceRef image = skin->getImage(path);
    if (image.isNull())
        return false;
    ibitmap *bitmap = LVImageSourceRef_to_ibitmab(image);
    const int x = layout.spacing + index * (layout.iconWidth + layout.spacing);
    StretchBitmap(x, 0, layout.iconWidth, layout.iconHeight, bitmap, 0);
    const int edge = std::round(layout.iconWidth * 0.04);
    FillArea(x + layout.iconWidth - edge, 0, edge, layout.iconHeight, 0xFFFFFF);
    free(bitmap);
    return true;
}

static void referenceTop(const CRSkinRef &skin, const PBQuickMenuLayout &layout, bool bookmarked)
{
    FillArea(0, 0, layout.width, layout.iconHeight + 2, 0xFFFFFF);
    FillArea(0, layout.iconHeight, layout.width, 1, 0x000000);
    for (int i = 0; i < pbQuickMenuIconCount; ++i)
        referenceIcon(skin, layout, i, bookmarked, false);
}

static int checkDrawing(PBQuickMenuIcons &cache, const CRSkinRef &skin, const PBQuickMenuLayout &layout, int depth)
{
    icanvas canvas{};
    canvas.width = layout.width;
    canvas.height = layout.height;
    canvas.depth = depth;
    canvas.scanline = (layout.width * depth + 7) / 8 + 7;
    std::vector<lUInt8> pixels(canvas.scanline * canvas.height);
    canvas.addr = pixels.data();
    icanvas *previous = GetCanvas();
    SetCanvas(&canvas);
    SetClip(0, 0, layout.width, layout.height);
    std::vector<lUInt8> normal[2];
    for (bool bookmarked : {false, true}) {
        for (lUInt8 background : {0xA5, 0x63}) {
            std::fill(pixels.begin(), pixels.end(), background);
            referenceTop(skin, layout, bookmarked);
            const auto reference = pixels;
            if (background == 0xA5)
                normal[bookmarked] = reference;
            std::fill(pixels.begin(), pixels.end(), background);
            cache.drawTop(skin, layout, bookmarked);
            CHECK(pixels == reference);
        }
        for (int i = 0; i < pbQuickMenuIconCount; ++i) {
            auto background = normal[bookmarked];
            for (int repeat = 0; repeat < 2; ++repeat) {
                std::copy(background.begin(), background.end(), pixels.begin());
                CHECK(referenceIcon(skin, layout, i, bookmarked, true));
                const auto pressed = pixels;
                CHECK(pressed != background || depth == 4);
                std::copy(background.begin(), background.end(), pixels.begin());
                CHECK(cache.drawPressed(skin, layout, bookmarked, i));
                CHECK(pixels == pressed);
                std::fill(background.begin(), background.end(), 0x63);
            }
        }
    }
    if (depth == 8)
        CHECK(normal[0] != normal[1]); // Exercise distinct bookmark assets.

    // A partial clip must bypass the cache without replacing the saved strip.
    SetClip(11, 3, layout.width - 22, layout.iconHeight - 6);
    std::fill(pixels.begin(), pixels.end(), 0x31);
    referenceTop(skin, layout, false);
    const auto clipped = pixels;
    std::fill(pixels.begin(), pixels.end(), 0x31);
    cache.drawTop(skin, layout, false);
    CHECK(pixels == clipped);

    std::fill(pixels.begin(), pixels.end(), 0x49);
    CHECK(referenceIcon(skin, layout, 2, true, true));
    const auto clippedPressed = pixels;
    std::fill(pixels.begin(), pixels.end(), 0x49);
    CHECK(cache.drawPressed(skin, layout, true, 2));
    CHECK(pixels == clippedPressed);

    SetClip(0, 0, layout.width, layout.height);
    std::fill(pixels.begin(), pixels.end(), 0xA5);
    cache.drawTop(skin, layout, false);
    CHECK(pixels == normal[0]);
    SetCanvas(previous);
    return 0;
}

static int runTests(int argc, char **argv)
{
    CHECK(argc == 3);
    CRSkinRef first = LVOpenSkin(Utf8ToUnicode(argv[1]));
    CRSkinRef second = LVOpenSkin(Utf8ToUnicode(argv[2]));
    CHECK(!first.isNull() && !second.isNull());
    PBQuickMenuIcons cache;
    const PBQuickMenuLayout portrait{1072, 1448, 51, 51, 89, 0};
    const PBQuickMenuLayout landscape{1448, 1072, 61, 61, 127, 1};
    CHECK(checkDrawing(cache, first, portrait, 8) == 0);
    CHECK(checkDrawing(cache, second, portrait, 8) == 0);
    CHECK(checkDrawing(cache, second, landscape, 8) == 0);
    CHECK(checkDrawing(cache, second, landscape, 4) == 0);
    CHECK(checkDrawing(cache, first, portrait, 8) == 0);
    puts("PASS: native InkView quick-menu pixels, pressed icons, bookmark variants, geometry, skin and clip changes");
    return 0;
}

int main(int argc, char **argv)
{
    InitFontManager(lString8());
    const int result = runTests(argc, argv);
    ShutdownFontManager();
    return result;
}
