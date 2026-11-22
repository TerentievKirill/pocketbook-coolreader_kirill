#include "crengine.h"
#include "lvgraydrawbuf.h"
#include "lvimagedecodercallback.h"
#include "testutil.h"
#include <cstring>
#include <vector>

lString32 pbSkinFileName, openedCacheFile, currentCacheDir;

class TestImage : public LVImageSource {
    int _width, _height;
    std::vector<lUInt32> _pixels;
public:
    TestImage(int width, int height, int originalWidth, int originalHeight)
        : _width(width), _height(height), _pixels(width * height)
    {
        for (int y = 0; y < height; ++y) {
            const int sy = y * originalHeight / height;
            for (int x = 0; x < width; ++x) {
                const int sx = x * originalWidth / width;
                const lUInt32 rgb = (sx * 3280271 + sy * 1953029) & 0xFFFFFF;
                const lUInt32 alpha[] = {0, 0x77000000, 0xFF000000};
                _pixels[y * width + x] = alpha[(sx + sy) % 3] | rgb;
            }
        }
    }
    ldomNode *GetSourceNode() override { return NULL; }
    LVStream *GetSourceStream() override { return NULL; }
    int GetWidth() const override { return _width; }
    int GetHeight() const override { return _height; }
    void Compact() override {}
    bool Decode(LVImageDecoderCallback *callback) override
    {
        if (!callback)
            return true;
        callback->OnStartDecode(this);
        // The scaling callback must support decoders that emit rows out of order.
        for (int y = _height - 1; y >= 0; --y)
            callback->OnLineDecoded(this, y, &_pixels[y * _width]);
        callback->OnEndDecode(this, false);
        return true;
    }
};

int main()
{
    const int sizes[][4] = {{23, 19, 11, 6}, {7, 9, 17, 15}, {1, 1, 15, 9}, {33, 31, 1, 1},
                            {23, 19, 23, 5}, {23, 19, 8, 19}};
    const lvRect clip(2, 1, 15, 13);
    for (const auto &size : sizes) {
        const int sw = size[0], sh = size[1], dw = size[2], dh = size[3];
        LVImageSourceRef source(new TestImage(sw, sh, sw, sh));
        LVImageSourceRef reference(new TestImage(dw, dh, sw, sh));
        for (int bpp : {2, 3, 4, 8}) {
            for (bool dither : {false, true}) {
                for (bool invert : {false, true}) {
                    LVGrayDrawBuf scaled(20, 16, bpp), expected(20, 16, bpp);
                    for (auto *buffer : {&scaled, &expected}) {
                        buffer->Clear(0xA8A8A8);
                        buffer->SetClipRect(&clip);
                        buffer->setDitherImages(dither);
                        buffer->setInvertImages(invert);
                    }
                    // Keep one destination pixel visible even when scaling down to 1x1.
                    const int x = dw == 1 ? 4 : -2, y = dh == 1 ? 3 : -1;
                    scaled.Draw(source, x, y, dw, dh, dither);
                    expected.Draw(reference, x, y, dw, dh, dither);
                    for (int row = 0; row < scaled.GetHeight(); ++row)
                        CHECK(std::memcmp(scaled.GetScanLine(row), expected.GetScanLine(row),
                                          scaled.GetRowSize()) == 0);
                }
            }
        }
    }
    puts("Scaled image pixels match the nearest-neighbor reference");
    return 0;
}
