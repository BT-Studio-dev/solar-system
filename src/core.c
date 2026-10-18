/* core.c -- implementation of platform-neutral helpers. */
#include "core.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

/* ------------------------------------------------------------- allocations */

#ifdef SSS_WINDOWS
#include <windows.h>
void *sss_alloc(size_t bytes) { return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bytes); }
void *sss_realloc(void *ptr, size_t bytes) {
    if (!ptr) return sss_alloc(bytes);
    return HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, ptr, bytes);
}
void sss_free(void *ptr) { if (ptr) HeapFree(GetProcessHeap(), 0, ptr); }
#else
void *sss_alloc(size_t bytes) { return calloc(1, bytes); }
void *sss_realloc(void *ptr, size_t bytes) {
    void *p = realloc(ptr, bytes);
    return p;
}
void sss_free(void *ptr) { free(ptr); }
#endif

/* ----------------------------------------------------------- string builder */

void str_init(Str *s, char *storage, int cap) {
    s->data = storage;
    s->cap = cap;
    s->len = 0;
    if (cap > 0) storage[0] = 0;
}

void str_clear(Str *s) {
    s->len = 0;
    if (s->cap > 0) s->data[0] = 0;
}

void str_addn(Str *s, const char *text, int n) {
    if (!text || n <= 0 || s->len >= s->cap - 1) return;
    if (n > s->cap - 1 - s->len) n = s->cap - 1 - s->len;
    memcpy(s->data + s->len, text, (size_t)n);
    s->len += n;
    s->data[s->len] = 0;
}

void str_add(Str *s, const char *text) {
    if (!text) return;
    str_addn(s, text, (int)strlen(text));
}

void str_addc(Str *s, char c) { str_addn(s, &c, 1); }

void str_add_pad(Str *s, int spaces) {
    while (spaces-- > 0) str_addc(s, ' ');
}

void str_add_int(Str *s, long long value) {
    char tmp[32];
    int n = 0;
    unsigned long long u;
    int neg = value < 0;
    u = neg ? (unsigned long long)(-(value + 1)) + 1ull : (unsigned long long)value;
    do {
        tmp[n++] = (char)('0' + (int)(u % 10));
        u /= 10;
    } while (u && n < (int)sizeof(tmp) - 2);
    if (neg) str_addc(s, '-');
    while (n > 0) str_addc(s, tmp[--n]);
}

void str_add_commas(Str *s, long long value) {
    char tmp[32];
    int n = 0;
    unsigned long long u;
    int neg = value < 0;
    u = neg ? (unsigned long long)(-(value + 1)) + 1ull : (unsigned long long)value;
    do {
        tmp[n++] = (char)('0' + (int)(u % 10));
        u /= 10;
    } while (u && n < (int)sizeof(tmp) - 2);
    if (neg) str_addc(s, '-');
    for (int i = n - 1; i >= 0; i--) {
        str_addc(s, tmp[i]);
        if (i > 0 && (i % 3) == 0) str_addc(s, ',');
    }
}

void str_add_float(Str *s, double value, int decimals) {
    if (decimals < 0) decimals = 0;
    if (decimals > 9) decimals = 9;
    int neg = value < 0;
    double v = neg ? -value : value;
    if (!(v == v)) { str_add(s, "n/a"); return; }
    if (v >= 1e15) {
        /* scientific notation keeps ephemeris-scale values exact and parseable */
        int exp = 0;
        while (v >= 10.0) { v /= 10.0; exp++; }
        str_add_float(s, neg ? -v : v, decimals < 6 ? 6 : decimals);
        str_addc(s, 'e');
        str_addc(s, '+');
        str_add_int(s, exp);
        return;
    }
    unsigned long long scale = 1;
    for (int i = 0; i < decimals; i++) scale *= 10ull;
    unsigned long long iv = (unsigned long long)(v + 0.5 / (double)scale);
    unsigned long long frac = 0;
    if (decimals > 0) {
        double f = (v - (double)(unsigned long long)v) * (double)scale;
        frac = (unsigned long long)(f + 0.5);
        if (frac >= scale) { frac = 0; iv += 1; }
    }
    if (neg && (iv != 0 || frac != 0)) str_addc(s, '-');
    str_add_int(s, (long long)iv);
    if (decimals > 0) {
        str_addc(s, '.');
        char tmp[12];
        for (int i = decimals - 1; i >= 0; i--) {
            tmp[i] = (char)('0' + (int)(frac % 10ull));
            frac /= 10ull;
        }
        str_addn(s, tmp, decimals);
    }
}

const char *str_cstr(Str *s) { return s->data; }

/* ------------------------------------------------------------ utf helpers */

int utf8_to_utf16(const char *in, int in_len, wchar_t *out, int out_cap) {
    int i = 0, o = 0;
    if (in_len < 0) in_len = (int)strlen(in);
    while (i < in_len) {
        unsigned char c = (unsigned char)in[i++];
        uint32_t cp;
        if (c < 0x80) cp = c;
        else if ((c & 0xE0) == 0xC0 && i < in_len) cp = ((uint32_t)(c & 0x1F) << 6) | (in[i++] & 0x3F);
        else if ((c & 0xF0) == 0xE0 && i + 1 < in_len)
            cp = ((uint32_t)(c & 0x0F) << 12) | ((uint32_t)(in[i] & 0x3F) << 6) | (in[i + 1] & 0x3F), i += 2;
        else if ((c & 0xF8) == 0xF0 && i + 2 < in_len) {
            cp = ((uint32_t)(c & 0x07) << 18) | ((uint32_t)(in[i] & 0x3F) << 12) |
                 ((uint32_t)(in[i + 1] & 0x3F) << 6) | (in[i + 2] & 0x3F);
            i += 3;
        } else cp = 0xFFFD;
        if (o + 1 >= out_cap) break;
        if (cp >= 0x10000) {
            if (o + 2 >= out_cap) break;
            cp -= 0x10000;
            out[o++] = (wchar_t)(0xD800 + (cp >> 10));
            out[o++] = (wchar_t)(0xDC00 + (cp & 0x3FF));
        } else {
            out[o++] = (wchar_t)cp;
        }
    }
    if (out_cap > 0) out[o < out_cap ? o : out_cap - 1] = 0;
    return o;
}

int utf16_to_utf8(const wchar_t *in, int in_len, char *out, int out_cap) {
    int i = 0, o = 0;
    if (in_len < 0) {
        in_len = 0;
        while (in[in_len]) in_len++;
    }
    while (i < in_len) {
        uint32_t cp = (uint16_t)in[i++];
        if (cp >= 0xD800 && cp <= 0xDBFF && i < in_len) {
            uint32_t lo = (uint16_t)in[i];
            if (lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                i++;
            }
        }
        unsigned char buf[4];
        int n;
        if (cp < 0x80) { buf[0] = (unsigned char)cp; n = 1; }
        else if (cp < 0x800) { buf[0] = (unsigned char)(0xC0 | (cp >> 6)); buf[1] = (unsigned char)(0x80 | (cp & 0x3F)); n = 2; }
        else if (cp < 0x10000) {
            buf[0] = (unsigned char)(0xE0 | (cp >> 12));
            buf[1] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
            buf[2] = (unsigned char)(0x80 | (cp & 0x3F));
            n = 3;
        } else {
            buf[0] = (unsigned char)(0xF0 | (cp >> 18));
            buf[1] = (unsigned char)(0x80 | ((cp >> 12) & 0x3F));
            buf[2] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
            buf[3] = (unsigned char)(0x80 | (cp & 0x3F));
            n = 4;
        }
        if (o + n >= out_cap) break;
        memcpy(out + o, buf, (size_t)n);
        o += n;
    }
    if (out_cap > 0) out[o < out_cap ? o : out_cap - 1] = 0;
    return o;
}

/* ---------------------------------------------------------------- math */

double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
double lerpd(double a, double b, double t) { return a + (b - a) * t; }
double smoothstepd(double t) {
    t = clampd(t, 0, 1);
    return t * t * (3 - 2 * t);
}
float lerpf(float a, float b, float t) { return a + (b - a) * t; }
float ease_out_cubic(float t) {
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    float f = 1.0f - t;
    return 1.0f - f * f * f;
}

/* ----------------------------------------------------------------- dates */

double wall_days_since_j2000(void) {
#ifdef SSS_WINDOWS
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    unsigned long long t = ((unsigned long long)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    /* 100ns ticks since 1601-01-01; JD of that epoch is 2305812.5 */
    double seconds = (double)t / 1e7;
    return 2305812.5 + seconds / 86400.0 - 2451545.0;
#else
    extern long time(long *);
    long secs = time(0);
    return (double)secs / 86400.0 + 2440587.5 - 2451545.0;
#endif
}

double date_to_jd(int year, int month, int day, int hour, int minute, int second) {
    /* Julian Day Number for the Gregorian calendar, then J2000 offset. */
    int a = (14 - month) / 12;
    int y = year + 4800 - a;
    int m = month + 12 * a - 3;
    long jdn = day + (153 * m + 2) / 5 + 365L * y + y / 4 - y / 100 + y / 400 - 32045;
    double frac = ((double)hour - 12.0 + (double)minute / 60.0 + (double)second / 3600.0) / 24.0;
    return (double)jdn + frac - 2451545.0;
}

void jd_to_date(double days, int *year, int *month, int *day, int *hour, int *minute, int *second) {
    double jd = 2451545.0 + days + 0.5;
    double Zf = floor(jd);
    double F = jd - Zf;
    long long Z = (long long)Zf;
    long long A = Z;
    if (Z >= 2299161) {
        long long alpha = (long long)((Zf - 1867216.25) / 36524.25);
        A = Z + 1 + alpha - alpha / 4;
    }
    long long B = A + 1524;
    long long C = (long long)((B - 122.1) / 365.25);
    long long D = (long long)(365.25 * (double)C);
    long long E = (long long)((B - D) / 30.6001);
    double dayf = (double)(B - D - (long long)(30.6001 * (double)E)) + F;
    int mon = (int)(E < 14 ? E - 1 : E - 13);
    int yr = (int)(mon > 2 ? C - 4716 : C - 4715);
    int dayi = (int)dayf;
    double frac = dayf - dayi;
    int secs = (int)(frac * 86400.0 + 0.5);
    if (secs >= 86400) { secs -= 86400; dayi += 1; }
    int hh = secs / 3600, mm = (secs / 60) % 60, ss = secs % 60;
    if (year) *year = yr;
    if (month) *month = mon;
    if (day) *day = dayi;
    if (hour) *hour = hh;
    if (minute) *minute = mm;
    if (second) *second = ss;
}

/* --------------------------------------------------------------- formatting */

const char *fmt_compact(double v, int decimals, char *buf, int cap) {
    Str s;
    str_init(&s, buf, cap);
    double a = v < 0 ? -v : v;
    if (a >= 1e12) { str_add_float(&s, v / 1e12, decimals); str_add(&s, "T"); }
    else if (a >= 1e9) { str_add_float(&s, v / 1e9, decimals); str_add(&s, "B"); }
    else if (a >= 1e6) { str_add_float(&s, v / 1e6, decimals); str_add(&s, "M"); }
    else if (a >= 1e4) { str_add_float(&s, v / 1e3, decimals); str_add(&s, "k"); }
    else str_add_float(&s, v, decimals);
    return buf;
}

const char *fmt_commas(double v, int decimals, char *buf, int cap) {
    Str s;
    str_init(&s, buf, cap);
    int neg = v < 0;
    double a = neg ? -v : v;
    double ip = floor(a);
    int frac = (int)((a - ip) * 1000.0 + 0.5);
    unsigned long long scale = 1;
    for (int i = 0; i < decimals; i++) scale *= 10ull;
    if (decimals == 0) {
        if (neg) str_addc(&s, '-');
        str_add_commas(&s, (long long)(a + 0.5));
        return buf;
    }
    if (frac >= 1000) { frac = 0; ip += 1; }
    char tmp[8];
    for (int i = 0; i < 3; i++) { tmp[2 - i] = (char)('0' + frac % 10); frac /= 10; }
    tmp[3] = 0;
    if (neg && (ip > 0 || frac > 0)) str_addc(&s, '-');
    str_add_commas(&s, (long long)ip);
    str_addc(&s, '.');
    str_addn(&s, tmp, decimals);
    return buf;
}

const char *fmt_hm(double hours, char *buf, int cap) {
    Str s;
    str_init(&s, buf, cap);
    if (hours < 0) { str_addc(&s, '-'); hours = -hours; }
    if (hours < 48) {
        int h = (int)hours;
        int m = (int)((hours - h) * 60.0 + 0.5);
        str_add_int(&s, h);
        str_add(&s, "h ");
        str_add_int(&s, m);
        str_add(&s, "m");
    } else if (hours < 24 * 365.25) {
        double d = hours / 24.0;
        str_add_float(&s, d, d < 10 ? 1 : 0);
        str_add(&s, "d");
    } else {
        str_add_float(&s, hours / (24.0 * 365.25), 2);
        str_add(&s, "y");
    }
    return buf;
}
