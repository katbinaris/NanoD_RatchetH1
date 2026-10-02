#include "tzrule.h"

#include <ctype.h>

// A zone name: letters, or anything in <> ("<+03>"). Skips it; false if there's none.
static bool name(const char **s) {
    const char *p = *s;
    if (*p == '<') {
        while (*p && *p != '>') p++;
        if (*p != '>' || p - *s < 2) return false;
        *s = p + 1;
        return true;
    }
    while (isalpha((unsigned char)*p)) p++;
    if (p - *s < 3) return false;
    *s = p;
    return true;
}

// [+-]hh[:mm[:ss]] -> seconds (hours up to 167, as POSIX allows for rule times).
static bool hms(const char **s, int32_t *out) {
    const char *p = *s;
    int sign = 1;
    if (*p == '+' || *p == '-') sign = *p++ == '-' ? -1 : 1;
    if (!isdigit((unsigned char)*p)) return false;
    int32_t v[3] = {0, 0, 0};
    for (int i = 0; i < 3; i++) {
        if (i > 0) {
            if (*p != ':') break;
            p++;
        }
        if (!isdigit((unsigned char)*p)) return false;
        while (isdigit((unsigned char)*p)) v[i] = v[i] * 10 + (*p++ - '0');
    }
    if (v[0] > 167 || v[1] > 59 || v[2] > 59) return false;
    *out = sign * (v[0] * 3600 + v[1] * 60 + v[2]);
    *s = p;
    return true;
}

static bool num(const char **s, int lo, int hi, uint8_t *out) {
    const char *p = *s;
    int v = 0;
    if (!isdigit((unsigned char)*p)) return false;
    while (isdigit((unsigned char)*p)) v = v * 10 + (*p++ - '0');
    if (v < lo || v > hi) return false;
    *out = (uint8_t)v;
    *s = p;
    return true;
}

// ",Mm.w.d[/time]" -- the only form tzfile footers use for rules.
static bool rule_date(const char **s, tz_date_t *d) {
    const char *p = *s;
    if (*p++ != ',' || *p++ != 'M') return false;
    if (!num(&p, 1, 12, &d->m) || *p++ != '.' || !num(&p, 1, 5, &d->w) || *p++ != '.' || !num(&p, 0, 6, &d->d)) return false;
    d->secs = 2 * 3600;
    if (*p == '/') {
        p++;
        if (!hms(&p, &d->secs)) return false;
    }
    *s = p;
    return true;
}

bool tzrule_parse(const char *s, tzrule_t *r) {
    *r = (tzrule_t){0};
    int32_t off;
    if (!s || !name(&s) || !hms(&s, &off)) return false;
    r->std_off = r->dst_off = -off; // POSIX counts west
    if (*s == '\0') return true;
    if (!name(&s)) return false;
    r->dst_off = r->std_off + 3600;
    if (*s != ',' && *s != '\0') {
        if (!hms(&s, &off)) return false;
        r->dst_off = -off;
    }
    if (*s == '\0') return false; // daylight time with no dates: nothing to go by
    if (!rule_date(&s, &r->start) || !rule_date(&s, &r->end) || *s != '\0') {
        *r = (tzrule_t){0};
        return false;
    }
    r->has_dst = true;
    return true;
}

// Days since 1970-01-01 of a civil date (Howard Hinnant's days_from_civil).
static int64_t days_from_civil(int64_t y, int m, int d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    int64_t yoe = y - era * 400;
    int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static int year_of(int64_t t) { // the civil year of UNIX time t
    int64_t z = (t >= 0 ? t : t - 86399) / 86400 + 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    int64_t doe = z - era * 146097;
    int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int64_t mp = (5 * doy + 2) / 153;
    return (int)(yoe + era * 400 + (mp >= 10));
}

static bool leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

// The local midnight (as seconds since the epoch, offset 0) the Mm.w.d rule falls on in year y.
static int64_t rule_day(const tz_date_t *d, int y) {
    static const uint8_t mdays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int64_t first = days_from_civil(y, d->m, 1);
    int wd = (int)(((first + 4) % 7 + 7) % 7); // 1970-01-01 was a Thursday
    int day = 1 + (d->d - wd + 7) % 7 + 7 * (d->w - 1);
    int len = mdays[d->m - 1] + (d->m == 2 && leap(y));
    while (day > len) day -= 7; // week 5 = the last one
    return (first + day - 1) * 86400;
}

int32_t tzrule_offset(const tzrule_t *r, int64_t t) {
    if (!r->has_dst) return r->std_off;
    int y = year_of(t + r->std_off);
    int64_t start = rule_day(&r->start, y) + r->start.secs - r->std_off; // switches in standard time
    int64_t end = rule_day(&r->end, y) + r->end.secs - r->dst_off;       // ...and back in daylight time
    bool dst = start < end ? (t >= start && t < end) : !(t >= end && t < start); // north : south
    return dst ? r->dst_off : r->std_off;
}
