/* core.h -- platform-neutral foundation types and helpers.
 *
 * Solar System Sandbox Simulator
 * Target: Windows 7 SP1 and later (32-bit), also builds for the headless
 * software-rendering harness on Linux for automated screenshot tests.
 */
#ifndef SSS_CORE_H
#define SSS_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#ifdef _WIN32
#define SSS_WINDOWS 1
#else
#define SSS_LINUX 1
#endif

#define SSS_VERSION_MAJOR 1
#define SSS_VERSION_MINOR 0
#define SSS_VERSION_PATCH 0
#define SSS_VERSION_STRING "1.0.0"
#define SSS_VERSION_WSTRING L"1.0.0"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define TAU (2.0 * M_PI)
#define DEG2RAD (M_PI / 180.0)
#define RAD2DEG (180.0 / M_PI)

/* ------------------------------------------------------------------ basics */

typedef struct {
    float x, y;
} Vec2;

typedef struct {
    double x, y, z;
} Vec3;

typedef struct {
    float x, y, w, h;
} SsRect;

typedef struct {
    float l, t, r, b;
} Insets;

/* 0xAARRGGBB */
typedef uint32_t SsColor;

#define RGB_A(r, g, b) ((SsColor)(0xFF000000u | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b)))
#define RGBA(r, g, b, a) ((SsColor)(((uint32_t)(a) << 24) | ((uint32_t)(r) << 16) | ((uint32_t)(g) << 8) | (uint32_t)(b)))
#define COL_A(c) ((uint8_t)(((c) >> 24) & 0xFF))
#define COL_R(c) ((uint8_t)(((c) >> 16) & 0xFF))
#define COL_G(c) ((uint8_t)(((c) >> 8) & 0xFF))
#define COL_B(c) ((uint8_t)((c) & 0xFF))

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(v, lo, hi) ((v) < (lo) ? (lo) : ((v) > (hi) ? (hi) : (v)))
#define ABSF(v) ((v) < 0 ? -(v) : (v))
#define ARRAY_LEN(a) ((int)(sizeof(a) / sizeof((a)[0])))

static inline int ifloor(double v) {
    int i = (int)v;
    return (v < 0 && (double)i != v) ? i - 1 : i;
}

static inline SsColor col_blend(SsColor a, SsColor b, float t) {
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
    int aa = (int)COL_A(a), ar = (int)COL_R(a), ag = (int)COL_G(a), ab = (int)COL_B(a);
    int ba = (int)COL_A(b), br = (int)COL_R(b), bg = (int)COL_G(b), bb = (int)COL_B(b);
    return RGBA(ar + (int)((br - ar) * t), ag + (int)((bg - ag) * t),
                ab + (int)((bb - ab) * t), aa + (int)((ba - aa) * t));
}

static inline SsColor col_alpha(SsColor c, float mul) {
    int a = (int)(COL_A(c) * (mul < 0 ? 0 : (mul > 1 ? 1 : mul)) + 0.5f);
    return (c & 0x00FFFFFFu) | ((uint32_t)a << 24);
}

static inline SsColor col_with_alpha(SsColor c, int a) {
    return (c & 0x00FFFFFFu) | ((uint32_t)CLAMP(a, 0, 255) << 24);
}

static inline SsRect rect_make(float x, float y, float w, float h) {
    SsRect r;
    r.x = x; r.y = y; r.w = w; r.h = h;
    return r;
}

static inline SsRect rect_inset(SsRect r, float d) {
    r.x += d; r.y += d; r.w -= d * 2; r.h -= d * 2;
    if (r.w < 0) r.w = 0;
    if (r.h < 0) r.h = 0;
    return r;
}

static inline SsRect rect_insets(SsRect r, Insets in) {
    r.x += in.l; r.y += in.t; r.w -= in.l + in.r; r.h -= in.t + in.b;
    return r;
}

static inline SsRect rect_offset(SsRect r, float dx, float dy) {
    r.x += dx; r.y += dy;
    return r;
}

static inline int rect_contains(SsRect r, float x, float y) {
    return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

static inline int rect_intersect(SsRect a, SsRect b, SsRect *out) {
    float l = MAX(a.x, b.x), t = MAX(a.y, b.y);
    float r = MIN(a.x + a.w, b.x + b.w), bo = MIN(a.y + a.h, b.y + b.h);
    if (r <= l || bo <= t) return 0;
    if (out) { out->x = l; out->y = t; out->w = r - l; out->h = bo - t; }
    return 1;
}

static inline SsRect rect_center(SsRect outer, float w, float h) {
    return rect_make(outer.x + (outer.w - w) * 0.5f, outer.y + (outer.h - h) * 0.5f, w, h);
}

static inline SsRect rect_split_top(SsRect *r, float h) {
    SsRect top = rect_make(r->x, r->y, r->w, h);
    r->y += h; r->h -= h;
    return top;
}

static inline SsRect rect_with_h(SsRect r, float h) {
    r.h = h;
    return r;
}

static inline Vec2 vec2(float x, float y) {
    Vec2 v; v.x = x; v.y = y;
    return v;
}

static inline float vec2_len(Vec2 v) { return (float)__builtin_sqrt((double)(v.x * v.x + v.y * v.y)); }

/* ------------------------------------------------------------- allocations */
/* The app avoids the CRT heap on Windows; these map to the platform allocator. */

void *sss_alloc(size_t bytes);
void *sss_realloc(void *ptr, size_t bytes);
void sss_free(void *ptr);

/* ------------------------------------------------------------- string builder */

typedef struct {
    char *data;
    int len;
    int cap;
} Str;

void str_init(Str *s, char *storage, int cap);
void str_clear(Str *s);
void str_add(Str *s, const char *text);
void str_addn(Str *s, const char *text, int n);
void str_addc(Str *s, char c);
void str_add_int(Str *s, long long value);
void str_add_float(Str *s, double value, int decimals);
void str_add_commas(Str *s, long long value);
void str_add_pad(Str *s, int spaces);
const char *str_cstr(Str *s);

/* UTF-16 <-> UTF-8 helpers (Win32 plumbing). */
int utf8_to_utf16(const char *in, int in_len, wchar_t *out, int out_cap);
int utf16_to_utf8(const wchar_t *in, int in_len, char *out, int out_cap);

/* ---------------------------------------------------------------- math */

double clampd(double v, double lo, double hi);
double lerpd(double a, double b, double t);
double smoothstepd(double t);
float lerpf(float a, float b, float t);
float ease_out_cubic(float t);

/* Wall clock as days since J2000 (UTC). */
double wall_days_since_j2000(void);

/* Days <-> calendar (proleptic Gregorian, J2000 = 2000-01-01 12:00 UTC). */
void jd_to_date(double days_since_j2000, int *year, int *month, int *day,
                int *hour, int *minute, int *second);
double date_to_jd(int year, int month, int day, int hour, int minute, int second);

/* Human readable helpers (static buffers, not re-entrant; fine for UI use). */
const char *fmt_compact(double v, int decimals, char *buf, int cap); /* 1.23k / 4.5M */
const char *fmt_commas(double v, int decimals, char *buf, int cap);
const char *fmt_hm(double hours, char *buf, int cap);

#endif /* SSS_CORE_H */
