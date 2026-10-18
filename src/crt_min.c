/* crt_min.c -- the small slice of the C runtime this application needs.
 *
 * The Windows build links without the Microsoft C runtime so the executable
 * runs on a stock Windows 7 machine that has no Universal CRT update.  Only
 * these primitives are required: memory/string helpers, a miniature snprintf
 * (formats: %s %c %d %i %u %x %ld %lld plus zero padding and %.Nf), strtod and
 * float wrappers over the double-precision maths exported by msvcrt.dll.
 */
#include "core.h"

#include <stddef.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <windows.h>

#ifdef SSS_WINDOWS

/* ---------------------------------------------------------------- memory */

void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if ((((uintptr_t)d | (uintptr_t)s) & 3u) == 0) {
        size_t words = n >> 2;
        uint32_t *dw = (uint32_t *)d;
        const uint32_t *sw = (const uint32_t *)s;
        for (size_t i = 0; i < words; i++) dw[i] = sw[i];
        d += words << 2;
        s += words << 2;
        n &= 3;
    }
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    const unsigned char *s = (const unsigned char *)src;
    if (d == s || n == 0) return dst;
    if (d < s) return memcpy(dst, src, n);
    d += n;
    s += n;
    while (n--) *--d = *--s;
    return dst;
}

void *memset(void *dst, int value, size_t n) {
    unsigned char *d = (unsigned char *)dst;
    unsigned char v = (unsigned char)value;
    if (((uintptr_t)d & 3u) == 0) {
        uint32_t word = ((uint32_t)v << 24) | ((uint32_t)v << 16) | ((uint32_t)v << 8) | v;
        size_t words = n >> 2;
        uint32_t *dw = (uint32_t *)d;
        for (size_t i = 0; i < words; i++) dw[i] = word;
        d += words << 2;
        n &= 3;
    }
    while (n--) *d++ = v;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *x = (const unsigned char *)a, *y = (const unsigned char *)b;
    while (n--) {
        if (*x != *y) return (int)*x - (int)*y;
        x++;
        y++;
    }
    return 0;
}

void *memchr(const void *p, int c, size_t n) {
    const unsigned char *s = (const unsigned char *)p;
    while (n--) {
        if (*s == (unsigned char)c) return (void *)s;
        s++;
    }
    return NULL;
}

size_t strlen(const char *s) {
    const char *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

int strcmp(const char *a, const char *b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

int strncmp(const char *a, const char *b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

char *strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++) != 0) {}
    return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
    char *d = dst;
    while (n && *src) { *d++ = *src++; n--; }
    while (n--) *d++ = 0;
    return dst;
}

char *strcat(char *dst, const char *src) {
    char *d = dst + strlen(dst);
    while ((*d++ = *src++) != 0) {}
    return dst;
}

char *strchr(const char *s, int c) {
    while (*s) {
        if (*s == (char)c) return (char *)s;
        s++;
    }
    return c == 0 ? (char *)s : NULL;
}

char *strrchr(const char *s, int c) {
    const char *last = NULL;
    while (*s) {
        if (*s == (char)c) last = s;
        s++;
    }
    return (char *)(c == 0 ? s : last);
}

char *strstr(const char *h, const char *n) {
    if (!*n) return (char *)h;
    for (; *h; h++) {
        const char *a = h, *b = n;
        while (*a && *b && *a == *b) { a++; b++; }
        if (!*b) return (char *)h;
    }
    return NULL;
}

/* ------------------------------------------------------------------ ctype */

int isdigit(int c) { return c >= '0' && c <= '9'; }
int isspace(int c) { return c == ' ' || (c >= 9 && c <= 13); }
int isalpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int toupper(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }
int tolower(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }

/* ----------------------------------------------------------------- strtod */

double strtod(const char *s, char **end) {
    const char *p = s;
    int neg = 0;
    while (isspace((unsigned char)*p)) p++;
    if (*p == '+' || *p == '-') { neg = *p == '-'; p++; }
    double value = 0;
    int digits = 0;
    while (isdigit((unsigned char)*p)) { value = value * 10.0 + (*p - '0'); p++; digits++; }
    if (*p == '.') {
        p++;
        double scale = 0.1;
        while (isdigit((unsigned char)*p)) {
            value += (*p - '0') * scale;
            scale *= 0.1;
            p++;
            digits++;
        }
    }
    if (digits && (*p == 'e' || *p == 'E')) {
        const char *save = p;
        p++;
        int eneg = 0;
        if (*p == '+' || *p == '-') { eneg = *p == '-'; p++; }
        int exp = 0;
        if (isdigit((unsigned char)*p)) {
            while (isdigit((unsigned char)*p)) { exp = exp * 10 + (*p - '0'); p++; }
            if (exp > 320) exp = 320;
            double mul = 1;
            for (int i = 0; i < exp; i++) mul *= 10.0;
            value = eneg ? value / mul : value * mul;
        } else {
            p = save;
        }
    }
    if (end) *end = (char *)(digits ? p : s);
    return neg ? -value : value;
}

double atof(const char *s) { return strtod(s, NULL); }

long strtol(const char *s, char **end, int base) {
    const char *p = s;
    int neg = 0;
    long value = 0;
    int digits = 0;
    if (base == 0) base = 10;
    while (isspace((unsigned char)*p)) p++;
    if (*p == '+' || *p == '-') { neg = *p == '-'; p++; }
    while (1) {
        int c = (unsigned char)*p, d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
        else break;
        if (d >= base) break;
        value = value * base + d;
        p++;
        digits++;
    }
    if (end) *end = (char *)(digits ? p : s);
    return neg ? -value : value;
}

/* --------------------------------------------------------------- printf */

typedef struct {
    char *dst;
    int cap;
    int len;
} Out;

static void out_ch(Out *o, char c) {
    if (o->len < o->cap - 1) o->dst[o->len] = c;
    o->len++;
}

static void out_str(Out *o, const char *s, int n) {
    for (int i = 0; i < n; i++) out_ch(o, s[i]);
}

static void out_uint(Out *o, unsigned long long value, unsigned base, int upper, int width,
                     char pad) {
    char buf[32];
    int n = 0;
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    if (value == 0) buf[n++] = '0';
    while (value) {
        buf[n++] = digits[value % (unsigned)base];
        value /= (unsigned)base;
    }
    for (int i = n; i < width; i++) out_ch(o, pad);
    while (n > 0) out_ch(o, buf[--n]);
}

static void out_int(Out *o, long long value, int width, char pad) {
    if (value < 0) {
        out_ch(o, '-');
        out_uint(o, (unsigned long long)(-(value + 1)) + 1ull, 10, 0, width - 1, pad);
    } else {
        out_uint(o, (unsigned long long)value, 10, 0, width, pad);
    }
}

static void out_fixed(Out *o, double value, int precision, int width, char pad) {
    if (value != value) {
        out_str(o, "nan", 3);
        return;
    }
    int neg = value < 0;
    if (neg) value = -value;
    if (value > 1e17) value = 1e17;
    unsigned long long scale = 1;
    for (int i = 0; i < precision; i++) scale *= 10ull;
    unsigned long long whole = (unsigned long long)value;
    double frac = (value - (double)whole) * (double)scale;
    unsigned long long f = (unsigned long long)(frac + 0.5);
    if (f >= scale) { f -= scale; whole += 1; }
    int extra = (neg ? 1 : 0) + (precision > 0 ? precision + 1 : 0);
    for (int i = 0; i < width - extra; i++) out_ch(o, pad);
    if (neg) out_ch(o, '-');
    out_uint(o, whole, 10, 0, 0, ' ');
    if (precision > 0) {
        out_ch(o, '.');
        char buf[24];
        for (int i = precision - 1; i >= 0; i--) {
            buf[i] = (char)('0' + (int)(f % 10ull));
            f /= 10ull;
        }
        out_str(o, buf, precision);
    }
}

int vsnprintf(char *dst, size_t cap, const char *fmt, va_list ap) {
    Out o;
    o.dst = dst;
    o.cap = (int)cap;
    o.len = 0;
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') {
            out_ch(&o, *p);
            continue;
        }
        p++;
        char pad = ' ';
        int width = 0;
        int precision = -1;
        int lng = 0;
        int left = 0;
        for (;; p++) {
            if (*p == '0') pad = '0';
            else if (*p == '-') left = 1;
            else if (*p == '+' || *p == ' ') { }
            else if (*p == '#') { }
            else break;
        }
        while (*p >= '0' && *p <= '9') { width = width * 10 + (*p - '0'); p++; }
        if (*p == '.') {
            p++;
            precision = 0;
            while (*p >= '0' && *p <= '9') { precision = precision * 10 + (*p - '0'); p++; }
        }
        while (*p == 'l' || *p == 'h' || *p == 'z' || *p == 'j' || *p == 't') {
            if (*p == 'l') lng++;
            p++;
        }
        char spec = *p;
        if (!spec) break;
        if (spec == 's') {
            const char *s = va_arg(ap, const char *);
            int n = (int)strlen(s);
            if (precision >= 0 && n > precision) n = precision;
            if (!left) for (int i = n; i < width; i++) out_ch(&o, ' ');
            out_str(&o, s, n);
            if (left) for (int i = n; i < width; i++) out_ch(&o, ' ');
        } else if (spec == 'c') {
            out_ch(&o, (char)va_arg(ap, int));
        } else if (spec == 'd' || spec == 'i') {
            long long v = (lng >= 2) ? va_arg(ap, long long) : (long long)va_arg(ap, int);
            out_int(&o, v, width, pad);
        } else if (spec == 'u') {
            unsigned long long v = (lng >= 2) ? va_arg(ap, unsigned long long)
                                              : (unsigned long long)va_arg(ap, unsigned int);
            out_uint(&o, v, 10, 0, width, pad);
        } else if (spec == 'x' || spec == 'X') {
            unsigned long long v = (lng >= 2) ? va_arg(ap, unsigned long long)
                                              : (unsigned long long)va_arg(ap, unsigned int);
            out_uint(&o, v, 16, spec == 'X', width, pad);
        } else if (spec == 'f' || spec == 'F') {
            out_fixed(&o, va_arg(ap, double), precision < 0 ? 6 : precision, width, pad);
        } else if (spec == 'g' || spec == 'G') {
            out_fixed(&o, va_arg(ap, double), precision < 0 ? 6 : precision, width, pad);
        } else if (spec == '%') {
            out_ch(&o, '%');
        } else {
            out_ch(&o, '%');
            out_ch(&o, spec);
        }
    }
    if (o.cap > 0) o.dst[o.len < o.cap ? o.len : o.cap - 1] = 0;
    return o.len;
}

int snprintf(char *dst, size_t cap, const char *fmt, ...) {
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(dst, cap, fmt, ap);
    va_end(ap);
    return n;
}

int sprintf(char *dst, const char *fmt, ...) {
    va_list ap;
    int n;
    va_start(ap, fmt);
    n = vsnprintf(dst, 0x7fffffff, fmt, ap);
    va_end(ap);
    return n;
}

/* ------------------------------------------------- 64-bit division helpers */

/* i686 has no 64-bit divide instruction; clang emits calls to these. */
unsigned long long __udivdi3(unsigned long long n, unsigned long long d) {
    unsigned long long q = 0, r = 0;
    if (d == 0) return 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1ull);
        if (r >= d) {
            r -= d;
            q |= (1ull << i);
        }
    }
    return q;
}

unsigned long long __umoddi3(unsigned long long n, unsigned long long d) {
    unsigned long long r = 0;
    if (d == 0) return 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((n >> i) & 1ull);
        if (r >= d) r -= d;
    }
    return r;
}

long long __divdi3(long long n, long long d) {
    int neg = (n < 0) != (d < 0);
    unsigned long long un = (unsigned long long)(n < 0 ? -n : n);
    unsigned long long ud = (unsigned long long)(d < 0 ? -d : d);
    unsigned long long q = __udivdi3(un, ud);
    return neg ? -(long long)q : (long long)q;
}

long long __moddi3(long long n, long long d) {
    unsigned long long un = (unsigned long long)(n < 0 ? -n : n);
    unsigned long long ud = (unsigned long long)(d < 0 ? -d : d);
    unsigned long long r = __umoddi3(un, ud);
    return (n < 0) ? -(long long)r : (long long)r;
}

__attribute__((used)) size_t wcslen(const wchar_t *s) {
    const wchar_t *p = s;
    while (*p) p++;
    return (size_t)(p - s);
}

/* ------------------------------------------------------------- stack probe */

#if defined(__i386__)
/* Symbol name is forced to __alloca (clang's i686 stack-probe helper). */
__attribute__((naked, used)) void sss_alloca_helper(size_t size) __asm__("__alloca");
__attribute__((naked, used)) void sss_alloca_helper(size_t size) {
    __asm__ volatile(
        "popl %ecx\n\t"
        "movl %esp, %edx\n\t"
        "subl %eax, %edx\n\t"
        "andl $-16, %edx\n\t"
        "movl %edx, %esp\n\t"
        "movl %edx, %eax\n\t"
        "pushl %ecx\n\t"
        "ret\n\t");
}
#endif

/* ------------------------------------------------------------------ math */

/* The Microsoft C runtime exports the double-precision maths, but binding to
 * it through an import library would drag in runtime startup objects.  These
 * thin lazy bindings call the same entry points in msvcrt.dll, which exists on
 * every Windows version since NT 4 and is already loaded by GDI+. */

typedef double (*DMathFn)(double);
typedef double (*DMathFn2)(double, double);

static void *g_crt;
static DMathFn f_sin, f_cos, f_tan, f_asin, f_acos, f_atan, f_exp, f_log, f_log10;
static DMathFn2 f_atan2, f_pow, f_fmod, f_hypot;

static void *crt_symbol(const char *name) {
    if (!g_crt) {
        g_crt = (void *)GetModuleHandleW(L"msvcrt.dll");
        if (!g_crt) g_crt = (void *)LoadLibraryW(L"msvcrt.dll");
    }
    if (!g_crt) return NULL;
    return (void *)GetProcAddress((HMODULE)g_crt, name);
}

static void *need(void **slot, const char *name) {
    if (!*slot) *slot = crt_symbol(name);
    return *slot;
}

double sin(double x) { need((void **)&f_sin, "sin"); return f_sin ? f_sin(x) : 0.0; }
double cos(double x) { need((void **)&f_cos, "cos"); return f_cos ? f_cos(x) : 1.0; }
double tan(double x) { need((void **)&f_tan, "tan"); return f_tan ? f_tan(x) : 0.0; }
double asin(double x) { need((void **)&f_asin, "asin"); return f_asin ? f_asin(x) : 0.0; }
double acos(double x) { need((void **)&f_acos, "acos"); return f_acos ? f_acos(x) : 0.0; }
double atan(double x) { need((void **)&f_atan, "atan"); return f_atan ? f_atan(x) : 0.0; }
double exp(double x) { need((void **)&f_exp, "exp"); return f_exp ? f_exp(x) : 1.0; }
double log(double x) { need((void **)&f_log, "log"); return f_log ? f_log(x) : 0.0; }
double log10(double x) { need((void **)&f_log10, "log10"); return f_log10 ? f_log10(x) : 0.0; }
double atan2(double y, double x) {
    need((void **)&f_atan2, "atan2");
    return f_atan2 ? f_atan2(y, x) : 0.0;
}
double pow(double x, double y) {
    need((void **)&f_pow, "pow");
    return f_pow ? f_pow(x, y) : 0.0;
}
double fmod(double x, double y) {
    need((void **)&f_fmod, "fmod");
    if (f_fmod) return f_fmod(x, y);
    return 0.0;
}
double hypot(double x, double y) {
    need((void **)&f_hypot, "hypot");
    if (f_hypot) return f_hypot(x, y);
    return __builtin_sqrt(x * x + y * y);
}
double sqrt(double x) { return __builtin_sqrt(x); }
double fabs(double x) { return __builtin_fabs(x); }

double floor(double x) {
    if (x >= 0.0) {
        double t = (double)(long long)x;
        return t;
    }
    double t = (double)(long long)x;
    return (t == x) ? t : t - 1.0;
}

double ceil(double x) {
    double t = (double)(long long)x;
    if (t == x) return t;
    return (x > 0.0) ? t + 1.0 : t;
}

double round(double x) { return (x < 0.0) ? ceil(x - 0.5) : floor(x + 0.5); }
double trunc(double x) { return (double)(long long)x; }

/* Float variants: msvcrt only exports the double entry points. */
float sinf(float x) { return (float)sin((double)x); }
float cosf(float x) { return (float)cos((double)x); }
float tanf(float x) { return (float)tan((double)x); }
float asinf(float x) { return (float)asin((double)x); }
float acosf(float x) { return (float)acos((double)x); }
float atanf(float x) { return (float)atan((double)x); }
float atan2f(float y, float x) { return (float)atan2((double)y, (double)x); }
float powf(float x, float y) { return (float)pow((double)x, (double)y); }
float fmodf(float x, float y) { return (float)fmod((double)x, (double)y); }
float floorf(float x) { return (float)floor((double)x); }
float ceilf(float x) { return (float)ceil((double)x); }
float fabsf(float x) { return (float)fabs((double)x); }
float expf(float x) { return (float)exp((double)x); }
float logf(float x) { return (float)log((double)x); }
float log10f(float x) { return (float)log10((double)x); }
float hypotf(float x, float y) { return (float)hypot((double)x, (double)y); }
float roundf(float x) { return (float)round((double)x); }
float sqrtf(float x) { return (float)sqrt((double)x); }
long lrintf(float x) { return (long)round((double)x); }

#endif /* SSS_WINDOWS */
