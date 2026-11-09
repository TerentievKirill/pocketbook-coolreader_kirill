#ifndef PB_CHECKPOINT_H
#define PB_CHECKPOINT_H

#include "lvtypes.h"

// Coalesce reading-position writes after input, but keep a deadline during
// sustained page turning. Failed saves stay pending for the next safe attempt.
class PBCheckpointPolicy {
    const void *_document = NULL;
    int _position = -1;
    bool _pending = false;
    lInt64 _since = 0;
public:
    void observe(const void *document, int position, lInt64 now, bool retry = false) {
        if (document != _document || position != _position || retry) {
            if (!_pending) _since = now;
            _pending = true;
            _document = document;
            _position = position;
        }
    }
    int delay(lInt64 now) const {
        if (!_pending) return -1;
        lInt64 remaining = 15000 - (now - _since);
        return remaining <= 0 ? 1 : remaining < 1500 ? (int)remaining : 1500;
    }
    void saved() { _pending = false; }
};

#endif
