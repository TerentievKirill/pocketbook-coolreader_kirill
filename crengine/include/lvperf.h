#ifndef LV_PERF_H
#define LV_PERF_H

#include <cstdlib>
#include <cstdio>
#include <time.h>

// Explicit opt-in; normal reading does not query clocks or write timing logs.
#ifdef POCKETBOOK_FW6
class LVPerfScope {
    const char *_name;
    bool _enabled;
    unsigned long long _start;
    static unsigned long long now() {
        timespec value;
        clock_gettime(CLOCK_MONOTONIC, &value);
        return (unsigned long long)value.tv_sec * 1000000 + value.tv_nsec / 1000;
    }
public:
    static bool enabled() {
        static const bool active = getenv("CR3_PERF") != NULL;
        return active;
    }
    explicit LVPerfScope(const char *name)
        : _name(name), _enabled(enabled()), _start(_enabled ? now() : 0) {}
    ~LVPerfScope() {
        if (_enabled) fprintf(stderr, "[cr3-perf] %s %llu us\n", _name, now() - _start);
    }
};
#else
class LVPerfScope {
public:
    static bool enabled() { return false; }
    explicit LVPerfScope(const char *) {}
};
#endif

#endif
