#ifndef LV_PERF_H
#define LV_PERF_H

#include <cstdlib>
#include <cstdio>
#include <time.h>

// Explicit opt-in; normal reading does not query clocks or write timing logs.
#ifdef POCKETBOOK_FW6
class LVPerfScope {
    const char *_name;
    unsigned long long _start;
    static unsigned long long now() {
        timespec value;
        if (clock_gettime(CLOCK_MONOTONIC, &value) != 0) return 0;
        return (unsigned long long)value.tv_sec * 1000000 + value.tv_nsec / 1000;
    }
public:
    static bool enabled() {
        static const bool active = getenv("CR3_PERF") != NULL;
        return active;
    }
    static unsigned long long timestamp() { return enabled() ? now() : 0; }
    static void logElapsed(const char *name, unsigned long long start) {
        if (!start) return;
        const auto end = now();
        if (end >= start) fprintf(stderr, "[cr3-perf] %s %llu us\n", name, end - start);
    }
    explicit LVPerfScope(const char *name) : _name(name), _start(timestamp()) {}
    void finish() {
        logElapsed(_name, _start);
        _start = 0;
    }
    ~LVPerfScope() { finish(); }
};
#else
class LVPerfScope {
public:
    static bool enabled() { return false; }
    static unsigned long long timestamp() { return 0; }
    static void logElapsed(const char *, unsigned long long) {}
    explicit LVPerfScope(const char *) {}
    void finish() {}
};
#endif

#endif
