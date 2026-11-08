#include "crengine.h"
#include "testutil.h"
#include <cstring>
#include <vector>
lString32 pbSkinFileName, openedCacheFile, currentCacheDir;

// A destination with padding tests the stride guard independently of dimensions.
class PaddedBuffer : public LVGrayDrawBuf {
    mutable std::vector<lUInt8> storage;
public:
    PaddedBuffer() : LVGrayDrawBuf(24, 12, 8), storage(28*12, 0xa5) {}
    int GetRowSize() const override { return 28; }
    lUInt8 *GetScanLine(int y) const override { return storage.data() + y*28; }
};

int main() {
    const int formats[] = {1, 2, 3, 4, 8};
    for (int bits : formats) {
        LVGrayDrawBuf src(24, 12, bits), dst(24, 12, bits);
        for (int y = 0; y < 12; ++y) for (int x = 0; x < 24; ++x) {
            int c = ((x*37+y*53)&255);
            src.FillRect(x, y, x+1, y+1, c*0x010101);
        }
        src.DrawTo(&dst, 0, 0, 0, NULL);
        CHECK(!memcmp(src.GetScanLine(0), dst.GetScanLine(0), 12*src.GetRowSize()));
        src.DrawTo(&src, 0, 0, 0, NULL); // Aliased full copy.
        CHECK(!memcmp(src.GetScanLine(0), dst.GetScanLine(0), 12*src.GetRowSize()));
        // Empty/partial clips and nonzero offsets must never take the full copy.
        const lvRect clips[] = {lvRect(0,0,0,0), lvRect(8,2,16,10), lvRect(0,0,24,12)};
        for (int offset = 0; offset <= 1; ++offset) for (const lvRect &clip : clips) {
            dst.SetClipRect(NULL); dst.Clear(0xffffff); dst.SetClipRect(&clip);
            lUInt32 white = dst.GetPixel(0,0);
            int dx = offset*8, dy = offset*2;
            src.DrawTo(&dst, dx, dy, 0, NULL);
            for (int y = 0; y < 12; ++y) for (int x = 0; x < 24; ++x) {
                bool copied = x >= dx && y >= dy && x >= clip.left && x < clip.right &&
                              y >= clip.top && y < clip.bottom;
                CHECK(dst.GetPixel(x,y) == (copied ? src.GetPixel(x-dx,y-dy) : white));
            }
        }
    }
    LVGrayDrawBuf src(24,12,8);
    for (int y = 0; y < 12; ++y) memset(src.GetScanLine(y), 17+y, 24);
    PaddedBuffer padded;
    src.DrawTo(&padded,0,0,0,NULL);
    for (int y = 0; y < 12; ++y) {
        CHECK(!memcmp(src.GetScanLine(y),padded.GetScanLine(y),24));
        for (int x = 24; x < 28; ++x) CHECK(padded.GetScanLine(y)[x] == 0xa5);
    }
    LVColorDrawBuf color(24,12,32);
    src.DrawTo(&color,0,0,0,NULL);
    for (int y = 0; y < 12; ++y) for (int x = 0; x < 24; ++x)
        CHECK((color.GetPixel(x,y)&0xffffff) == (unsigned)(17+y)*0x010101);
    puts("PASS: grayscale bulk copy, self-copy, clipping, offsets, padded stride and color conversion");
}
