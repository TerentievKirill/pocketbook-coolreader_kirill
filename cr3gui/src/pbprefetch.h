#ifndef PB_PREFETCH_H
#define PB_PREFETCH_H

#include <cstddef>

class PBPrefetchDirection {
    const void *_document = NULL;
    int _page = -1;
    int _direction = 1;
public:
    void observe(const void *document, int page) {
        if (document != _document || page > _page + 1 || page < _page - 1)
            _direction = 1;
        else if (page != _page)
            _direction = page < _page ? -1 : 1;
        _document = document;
        _page = page;
    }
    int direction() const { return _direction; }
};

#endif
