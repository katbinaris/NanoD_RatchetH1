#pragma once

#include <stdbool.h>
#include <stdint.h>

// POSIX TZ rules -- the footer of a tzfile, e.g. "IST-2IDT,M3.4.4/26,M10.5.0": a standard
// time, and optionally a daylight time with the two Mm.w.d dates it starts and ends. Read
// without setenv("TZ") / tzset() (newlib's setenv leaks when a longer value replaces a shorter
// one, and TZ is global), so any number of zones can be looked at, from any task. Plain C, no
// dependencies: tools/tz_test checks it against the system's zoneinfo.

typedef struct {
    uint8_t m, w, d; // month 1-12, week 1-5 (5 = the last), weekday 0 = Sunday
    int32_t secs;    // local time of day it switches at; may be past 24h or negative
} tz_date_t;

typedef struct {
    int32_t std_off, dst_off; // seconds east of UTC
    bool has_dst;
    tz_date_t start, end;
} tzrule_t;

// false if `s` isn't a rule this understands (then *r is UTC).
bool tzrule_parse(const char *s, tzrule_t *r);
// The offset east of UTC, in seconds, at UNIX time t.
int32_t tzrule_offset(const tzrule_t *r, int64_t t);
