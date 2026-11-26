// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef LV_DECODED_IMAGE_CACHE_H
#define LV_DECODED_IMAGE_CACHE_H

#include "lvhashtable.h"
#include "lvimg.h"
#include "lvimagedecodercallback.h"
#include <array>
#include <atomic>
#include <cstring>
#include <list>
#include <memory>
#include <new>

class LVDecodedImage : public LVImageSource {
    int _width;
    int _height;
    std::unique_ptr<lUInt32[]> _pixels;
    std::shared_ptr<std::atomic<size_t>> _usedBytes;

    size_t bytes() const { return size_t(_width) * _height * sizeof(lUInt32); }

public:
    LVDecodedImage(const LVImageMetadata &info, const std::shared_ptr<std::atomic<size_t>> &usedBytes)
        : _width(info.width), _height(info.height),
          _pixels(new (std::nothrow) lUInt32[size_t(_width) * _height]), _usedBytes(usedBytes)
    {
        if (_pixels)
            _usedBytes->fetch_add(bytes(), std::memory_order_relaxed);
    }

    ~LVDecodedImage() override
    {
        if (_pixels) {
            _pixels.reset();
            _usedBytes->fetch_sub(bytes(), std::memory_order_relaxed);
        }
    }

    ldomNode *GetSourceNode() override { return nullptr; }
    LVStream *GetSourceStream() override { return nullptr; }
    void Compact() override {}
    int GetWidth() const override { return _width; }
    int GetHeight() const override { return _height; }
    lUInt32 *row(int y) { return _pixels.get() + size_t(_width) * y; }
    bool allocated() const { return bool(_pixels); }

    bool Decode(LVImageDecoderCallback *callback) override
    {
        if (!callback)
            return true;
        // Callbacks may modify their input. Keep the cached pixels intact.
        std::unique_ptr<lUInt32[]> line(new (std::nothrow) lUInt32[_width]);
        if (!line)
            return false;
        callback->OnStartDecode(this);
        for (int y = 0; y < _height; ++y) {
            std::memcpy(line.get(), row(y), _width * sizeof(lUInt32));
            callback->OnLineDecoded(this, y, line.get());
        }
        callback->OnEndDecode(this, false);
        return true;
    }
};

// Forward the original decoder's output while retaining complete sequential
// images. Failed and out-of-order decodes still reach the original callback.
class LVImageCapture : public LVImageDecoderCallback {
    LVDecodedImage &_image;
    LVImageDecoderCallback &_target;
    int _nextRow = 0;
    bool _matches = false;
    bool _complete = false;

public:
    LVImageCapture(LVDecodedImage &image, LVImageDecoderCallback &target)
        : _image(image), _target(target) {}

    void OnStartDecode(LVImageSource *source) override
    {
        _nextRow = 0;
        _complete = false;
        _matches = source->GetWidth() == _image.GetWidth() && source->GetHeight() == _image.GetHeight();
        _target.OnStartDecode(source);
    }

    bool OnLineDecoded(LVImageSource *source, int y, lUInt32 *data) override
    {
        if (_matches && y == _nextRow && y < _image.GetHeight()) {
            std::memcpy(_image.row(y), data, _image.GetWidth() * sizeof(lUInt32));
            ++_nextRow;
        } else {
            _matches = false;
        }
        return _target.OnLineDecoded(source, y, data);
    }

    void OnEndDecode(LVImageSource *source, bool errors) override
    {
        _complete = !errors && _matches && _nextRow == _image.GetHeight();
        _target.OnEndDecode(source, errors);
    }

    bool complete() const { return _complete; }
};

// The owning document serializes cache access. Pixel buffers pinned by a
// replay or an in-progress decode count against the budget until released.
class LVDecodedImageCache {
public:
    struct Key {
        lUInt32 binaryIndex = 0;
        lString32 path;

        bool operator==(const Key &other) const
        {
            return binaryIndex == other.binaryIndex && path == other.path;
        }

        friend lUInt32 getHash(const Key &key)
        {
            return key.binaryIndex ? getHash(key.binaryIndex) : getHash(key.path);
        }
    };

    static constexpr size_t byteBudget = 4 * 1024 * 1024;

private:
    struct Entry {
        Key key;
        std::shared_ptr<LVDecodedImage> image;
    };
    std::list<Entry> _entries;
    LVHashTable<Key, std::list<Entry>::iterator> _index{64};
    std::array<lUInt64, 128> _recent{};
    std::shared_ptr<std::atomic<size_t>> _usedBytes = std::make_shared<std::atomic<size_t>>(0);
    lUInt64 _generation = 0;

    void erase(std::list<Entry>::iterator it)
    {
        _index.remove(it->key);
        _entries.erase(it);
    }

public:
    static bool eligible(const LVImageMetadata &info)
    {
        return info.width > 0 && info.height > 0 && info.format != LVImageFormat::Dummy &&
               info.format != LVImageFormat::Unknown &&
               size_t(info.width) <= byteBudget / sizeof(lUInt32) / size_t(info.height);
    }

    std::shared_ptr<LVDecodedImage> find(const Key &key)
    {
        std::list<Entry>::iterator it;
        if (!_index.get(key, it))
            return {};
        _entries.splice(_entries.begin(), _entries, it);
        return it->image;
    }

    bool seenRecently(const Key &key)
    {
        // Fingerprints only decide admission. Hits always compare the exact key.
        const lUInt32 hash = getHash(key);
        const lUInt64 fingerprint = lUInt64(hash) + 1;
        auto &recent = _recent[hash % _recent.size()];
        const bool seen = recent == fingerprint;
        recent = fingerprint;
        return seen;
    }

    std::shared_ptr<LVDecodedImage> reserve(const LVImageMetadata &info)
    {
        if (!eligible(info))
            return {};
        const size_t bytes = size_t(info.width) * info.height * sizeof(lUInt32);
        while (_usedBytes->load(std::memory_order_relaxed) > byteBudget - bytes) {
            if (_entries.empty())
                return {};
            erase(std::prev(_entries.end()));
        }
        auto image = std::make_shared<LVDecodedImage>(info, _usedBytes);
        return image->allocated() ? image : nullptr;
    }

    void insert(const Key &key, const std::shared_ptr<LVDecodedImage> &image, lUInt64 generation)
    {
        std::list<Entry>::iterator it;
        if (generation != _generation || _index.get(key, it))
            return;
        if (_entries.size() == 64)
            erase(std::prev(_entries.end()));
        _entries.push_front({key, image});
        _index.set(key, _entries.begin());
    }

    lUInt64 generation() const { return _generation; }

    void invalidateBinary(lUInt32 index)
    {
        ++_generation;
        std::list<Entry>::iterator it;
        if (_index.get(Key{index, lString32()}, it))
            erase(it);
    }

    void clear()
    {
        ++_generation;
        _index.clear();
        _entries.clear();
        _recent.fill(0);
    }
};

#endif
