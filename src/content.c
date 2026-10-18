/* content.c -- see content.h for the folder layout.
 *
 * Reading, scanning and logging use the platform layer under SSS_WINDOWS and
 * plain stdio elsewhere, so tests/test_content.c can exercise the parsers on
 * Linux against a temporary tree.
 */
#include "content.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>
#include <stdlib.h>

#ifdef SSS_WINDOWS
#include <windows.h>
#else
#include <dirent.h>
#include <sys/stat.h>
#endif

static const char *FOLDER_NAMES[CF_COUNT] = {
    "Assets", "Parts", "Texture Pack", "Mods/Custom_", "Logs", "Saving",
};

static char g_app_root[CONTENT_PATH];
static char g_user_root[CONTENT_PATH];
static char g_app_dirs[CF_COUNT][CONTENT_PATH];
static char g_user_dirs[CF_COUNT][CONTENT_PATH];
static int g_ready;

static ContentPart g_parts[CONTENT_MAX_PARTS];
static int g_part_count;
static ContentPack g_packs[CONTENT_MAX_PACKS];
static int g_pack_count;
static ContentMod g_mods[CONTENT_MAX_MODS];
static int g_mod_count;

/* ---------------------------------------------------------------- helpers */

static void copy_str(char *dst, int cap, const char *src) {
    int i = 0;
    if (cap <= 0) return;
    while (src && src[i] && i < cap - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void join_path(char *out, int cap, const char *dir, const char *leaf) {
    int n = 0;
    if (cap <= 0) return;
    if (dir) {
        while (dir[n] && n < cap - 1) {
            out[n] = dir[n];
            n++;
        }
    }
    if (n > 0 && out[n - 1] != '/' && out[n - 1] != '\\' && leaf && leaf[0]) out[n++] = '/';
    for (int i = 0; leaf && leaf[i] && n < cap - 1; i++) out[n++] = leaf[i];
    out[n] = 0;
}

static int ends_with_ci(const char *s, const char *suffix) {
    int n = (int)strlen(s), m = (int)strlen(suffix);
    if (n < m) return 0;
    for (int i = 0; i < m; i++) {
        char a = s[n - m + i], b = suffix[i];
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if (a != b) return 0;
    }
    return 1;
}

static void strip_extension(char *name) {
    char *dot = strrchr(name, '.');
    if (dot && dot != name) *dot = 0;
}

/* ---------------------------------------------------------------- colours */

static int hex_digit(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int content_parse_color(const char *text, SsColor *out) {
    if (!text || !out) return 0;
    while (*text == ' ' || *text == '\t') text++;
    if (*text == '#') {
        unsigned v = 0;
        int n = 0;
        text++;
        while (text[n] && n < 8) {
            int d = hex_digit((unsigned char)text[n]);
            if (d < 0) return 0;
            v = (v << 4) | (unsigned int)d;
            n++;
        }
        if (n == 6) {
            *out = RGBA((int)((v >> 16) & 0xFF), (int)((v >> 8) & 0xFF), (int)(v & 0xFF), 255);
            return 1;
        }
        if (n == 3) {
            int r = (int)((v >> 8) & 0xF), g = (int)((v >> 4) & 0xF), b = (int)(v & 0xF);
            *out = RGBA(r * 17, g * 17, b * 17, 255);
            return 1;
        }
        return 0;
    }
    /* r,g,b */
    int parts[3] = {0, 0, 0};
    int idx = 0;
    const char *p = text;
    while (idx < 3) {
        int value = 0, digits = 0;
        while (*p == ' ' || *p == '\t') p++;
        while (*p >= '0' && *p <= '9') {
            value = value * 10 + (*p - '0');
            digits++;
            p++;
        }
        if (!digits) return 0;
        parts[idx++] = value > 255 ? 255 : value;
        while (*p == ' ') p++;
        if (idx < 3) {
            if (*p != ',') return 0;
            p++;
        }
    }
    *out = RGBA(parts[0], parts[1], parts[2], 255);
    return 1;
}

/* ---------------------------------------------------------------- parsing */

static const char *trim(const char *s, const char *end) {
    while (s < end && (*s == ' ' || *s == '\t')) s++;
    while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) end--;
    return s;
}

/* Calls fn(key, key_len, value, value_len) for every key=value line. */
static void parse_kv(const char *text, int len,
                     void (*fn)(void *, const char *, int, const char *, int), void *ctx) {
    int line = 0;
    while (line < len) {
        int end = line;
        while (end < len && text[end] != '\n' && text[end] != '\r') end++;
        const char *s = trim(text + line, text + end);
        const char *e = text + end;
        if (s < e && *s != '#' && *s != ';' && *s != '[') {
            const char *eq = s;
            while (eq < e && *eq != '=') eq++;
            if (eq < e) {
                const char *key_end = eq;
                while (key_end > s && (key_end[-1] == ' ' || key_end[-1] == '\t')) key_end--;
                const char *v = eq + 1;
                fn(ctx, s, (int)(key_end - s), v, (int)(e - v));
            }
        }
        line = end;
        while (line < len && (text[line] == '\r' || text[line] == '\n')) line++;
    }
}

static double key_double(const char *value, int len) {
    char buf[64];
    int n = len < (int)sizeof(buf) - 1 ? len : (int)sizeof(buf) - 1;
    memcpy(buf, value, (size_t)n);
    buf[n] = 0;
    return strtod(buf, NULL);
}

static int key_int(const char *value, int len) {
    return (int)(key_double(value, len) + (key_double(value, len) < 0 ? -0.5 : 0.5));
}

static void copy_trimmed(char *dst, int cap, const char *value, int len) {
    const char *s = value;
    const char *e = value + len;
    while (s < e && (*s == ' ' || *s == '\t')) s++;
    while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r')) e--;
    int n = (int)(e - s);
    if (n > cap - 1) n = cap - 1;
    memcpy(dst, s, (size_t)n);
    dst[n] = 0;
}

/* parse_kv hands out a slice, not a C string, so copy before parsing. */
static void color_from_value(const char *value, int len, SsColor *out) {
    char buf[64];
    int n = len < (int)sizeof(buf) - 1 ? len : (int)sizeof(buf) - 1;
    memcpy(buf, value, (size_t)n);
    buf[n] = 0;
    content_parse_color(buf, out);
}

typedef struct {
    ContentPart *part;
} PartCtx;

static void part_kv(void *ctx, const char *key, int klen, const char *value, int vlen) {
    PartCtx *pc = (PartCtx *)ctx;
    ContentPart *p = pc->part;
#define KEY(s) (klen == (int)(sizeof(s) - 1) && memcmp(key, s, sizeof(s) - 1) == 0)
    if (KEY("name")) copy_trimmed(p->name, CONTENT_NAME, value, vlen);
    else if (KEY("kind")) copy_trimmed(p->kind, sizeof(p->kind), value, vlen);
    else if (KEY("mass") || KEY("mass_kg")) p->mass_kg = key_double(value, vlen);
    else if (KEY("radius") || KEY("radius_km")) p->radius_km = key_double(value, vlen);
    else if (KEY("orbit") || KEY("orbit_au")) p->orbit_au = key_double(value, vlen);
    else if (KEY("ecc") || KEY("eccentricity")) p->ecc = key_double(value, vlen);
    else if (KEY("color1") || KEY("colour1")) color_from_value(value, vlen, &p->col1);
    else if (KEY("color2") || KEY("colour2")) color_from_value(value, vlen, &p->col2);
    else if (KEY("accent")) color_from_value(value, vlen, &p->accent);
    else if (KEY("rings") || KEY("ringed")) p->ringed = key_int(value, vlen) != 0;
    else if (KEY("ring_in")) p->ring_in = key_double(value, vlen);
    else if (KEY("ring_out")) p->ring_out = key_double(value, vlen);
    else if (KEY("ring_color") || KEY("ring_colour")) color_from_value(value, vlen, &p->ring_col);
    else if (KEY("note") || KEY("description")) copy_trimmed(p->note, CONTENT_NOTE, value, vlen);
#undef KEY
}

static void pack_kv(void *ctx, const char *key, int klen, const char *value, int vlen) {
    ContentPack *p = (ContentPack *)ctx;
#define KEY(s) (klen == (int)(sizeof(s) - 1) && memcmp(key, s, sizeof(s) - 1) == 0)
    if (KEY("name")) copy_trimmed(p->name, CONTENT_NAME, value, vlen);
    else if (KEY("note") || KEY("description")) copy_trimmed(p->note, CONTENT_NOTE, value, vlen);
    else if (KEY("tint") || KEY("colour") || KEY("color")) color_from_value(value, vlen, &p->tint);
    else if (KEY("strength")) {
        int s = key_int(value, vlen);
        p->strength = s < 0 ? 0 : (s > 100 ? 100 : s);
    }
#undef KEY
}

static void part_defaults(ContentPart *p) {
    memset(p, 0, sizeof(*p));
    p->mass_kg = 5.97e24;
    p->radius_km = 6371;
    p->orbit_au = 1.0;
    p->col1 = RGB_A(150, 170, 200);
    p->col2 = RGB_A(60, 76, 104);
    p->accent = RGB_A(120, 180, 255);
    p->ring_col = RGBA(220, 210, 180, 150);
}

static void pack_defaults(ContentPack *p) {
    memset(p, 0, sizeof(*p));
    p->strength = 45;
    p->tint = RGB_A(160, 200, 255);
}

/* ---------------------------------------------------------------- file io */

static int read_text(const char *path, char *buf, int cap) {
    if (!path || cap <= 1) return 0;
#ifdef SSS_WINDOWS
    wchar_t wide[CONTENT_PATH * 2];
    int n = utf8_to_utf16(path, (int)strlen(path), wide, CONTENT_PATH * 2 - 1);
    if (n <= 0) return 0;
    wide[n] = 0;
    HANDLE h = CreateFileW(wide, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    DWORD read = 0;
    int ok = ReadFile(h, buf, (DWORD)(cap - 1), &read, NULL) ? 1 : 0;
    CloseHandle(h);
    buf[ok ? (int)read : 0] = 0;
    return ok ? (int)read : 0;
#else
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t got = fread(buf, 1, (size_t)(cap - 1), f);
    fclose(f);
    buf[got] = 0;
    return (int)got;
#endif
}

static void make_dir(const char *path) {
#ifdef SSS_WINDOWS
    wchar_t wide[CONTENT_PATH * 2];
    int n = utf8_to_utf16(path, (int)strlen(path), wide, CONTENT_PATH * 2 - 1);
    if (n <= 0) return;
    wide[n] = 0;
    CreateDirectoryW(wide, NULL);
#else
    mkdir(path, 0775);
#endif
}

/* Creates "a/b/c" one level at a time. */
static void make_dirs(const char *path) {
    char tmp[CONTENT_PATH];
    copy_str(tmp, sizeof(tmp), path);
    for (int i = 1; tmp[i]; i++) {
        if (tmp[i] == '/' || tmp[i] == '\\') {
            char sep = tmp[i];
            tmp[i] = 0;
            if (tmp[0] && !(i == 2 && tmp[1] == ':')) make_dir(tmp);
            tmp[i] = sep;
        }
    }
    make_dir(tmp);
}

/* --------------------------------------------------------------- scanning */

typedef struct {
    int folder;
    const char *root;
} ScanCtx;

static void add_part_file(const char *path) {
    if (g_part_count >= CONTENT_MAX_PARTS) return;
    char text[8192];
    if (read_text(path, text, sizeof(text)) <= 0) return;
    ContentPart part;
    part_defaults(&part);
    PartCtx ctx = {&part};
    parse_kv(text, (int)strlen(text), part_kv, &ctx);
    if (!part.name[0]) {
        const char *leaf = strrchr(path, '/');
        if (!leaf) leaf = strrchr(path, '\\');
        copy_str(part.name, CONTENT_NAME, leaf ? leaf + 1 : path);
        strip_extension(part.name);
    }
    if (!part.kind[0]) copy_str(part.kind, sizeof(part.kind), "terrestrial");
    copy_str(part.file, CONTENT_PATH, path);
    g_parts[g_part_count++] = part;
}

static void add_pack_file(const char *ini_path, const char *dir) {
    if (g_pack_count >= CONTENT_MAX_PACKS) return;
    char text[4096];
    if (read_text(ini_path, text, sizeof(text)) <= 0) return;
    ContentPack pack;
    pack_defaults(&pack);
    parse_kv(text, (int)strlen(text), pack_kv, &pack);
    if (!pack.name[0]) {
        const char *leaf = strrchr(dir, '/');
        if (!leaf) leaf = strrchr(dir, '\\');
        copy_str(pack.name, CONTENT_NAME, leaf ? leaf + 1 : dir);
    }
    copy_str(pack.file, CONTENT_PATH, dir);
    g_packs[g_pack_count++] = pack;
}

static void add_mod_file(const char *path) {
    if (g_mod_count >= CONTENT_MAX_MODS) return;
    ContentMod mod;
    memset(&mod, 0, sizeof(mod));
    const char *leaf = strrchr(path, '/');
    if (!leaf) leaf = strrchr(path, '\\');
    copy_str(mod.name, CONTENT_NAME, leaf ? leaf + 1 : path);
    strip_extension(mod.name);
    copy_str(mod.file, CONTENT_PATH, path);
    g_mods[g_mod_count++] = mod;
}

static void join_and_add(const char *dir, const char *name, int folder) {
    char path[CONTENT_PATH];
    join_path(path, sizeof(path), dir, name);
    if (folder == CF_PARTS && ends_with_ci(name, ".part")) add_part_file(path);
    else if (folder == CF_MODS && ends_with_ci(name, ".sims")) add_mod_file(path);
}

#ifdef SSS_WINDOWS

static void scan_dir(const char *dir, int folder, int depth) {
    wchar_t wide[CONTENT_PATH * 2];
    int n = utf8_to_utf16(dir, (int)strlen(dir), wide, CONTENT_PATH * 2 - 4);
    if (n <= 0) return;
    wide[n++] = L'\\';
    wide[n++] = L'*';
    wide[n] = 0;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(wide, &fd);
    if (h == INVALID_HANDLE_VALUE) return;
    do {
        const wchar_t *wname = fd.cFileName;
        if (wname[0] == L'.') continue;
        char name[CONTENT_NAME * 4];
        int len = utf16_to_utf8(wname, (int)wcslen(wname), name, sizeof(name));
        if (len <= 0) continue;
        name[len] = 0;
        char path[CONTENT_PATH];
        join_path(path, sizeof(path), dir, name);
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            if (folder == CF_TEXTURE_PACKS) {
                char ini[CONTENT_PATH];
                join_path(ini, sizeof(ini), path, "pack.ini");
                wchar_t wini[CONTENT_PATH * 2];
                int m = utf8_to_utf16(ini, (int)strlen(ini), wini, CONTENT_PATH * 2 - 1);
                if (m > 0) {
                    wini[m] = 0;
                    DWORD attr = GetFileAttributesW(wini);
                    if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
                        add_pack_file(ini, path);
                }
            } else if (depth < 2) {
                scan_dir(path, folder, depth + 1);
            }
        } else {
            join_and_add(dir, name, folder);
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
}

#else

static void scan_dir(const char *dir, int folder, int depth) {
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        const char *name = ent->d_name;
        if (name[0] == '.') continue;
        char path[CONTENT_PATH];
        join_path(path, sizeof(path), dir, name);
        int is_dir = 0;
#ifdef DT_DIR
        if (ent->d_type == DT_DIR) is_dir = 1;
        else if (ent->d_type == DT_UNKNOWN) {
#endif
            struct stat st;
            if (stat(path, &st) == 0 && S_ISDIR(st.st_mode)) is_dir = 1;
#ifdef DT_DIR
        }
#endif
        if (is_dir) {
            if (folder == CF_TEXTURE_PACKS) {
                char ini[CONTENT_PATH];
                join_path(ini, sizeof(ini), path, "pack.ini");
                char probe[8];
                if (read_text(ini, probe, sizeof(probe)) > 0) add_pack_file(ini, path);
            } else if (depth < 2) {
                scan_dir(path, folder, depth + 1);
            }
        } else {
            join_and_add(dir, name, folder);
        }
    }
    closedir(d);
}

#endif

void content_scan(void) {
    g_part_count = g_pack_count = g_mod_count = 0;
    if (!g_ready) return;
    for (int folder = 0; folder < CF_COUNT; folder++) {
        if (folder == CF_LOGS || folder == CF_SAVING || folder == CF_ASSETS) continue;
        if (g_app_dirs[folder][0]) {
            make_dirs(g_app_dirs[folder]);
            scan_dir(g_app_dirs[folder], folder, 0);
        }
        if (g_user_dirs[folder][0] && strcmp(g_user_dirs[folder], g_app_dirs[folder]) != 0) {
            make_dirs(g_user_dirs[folder]);
            scan_dir(g_user_dirs[folder], folder, 0);
        }
    }
}

/* ------------------------------------------------------------------- api */

int content_init(const char *app_root, const char *user_root) {
    g_part_count = g_pack_count = g_mod_count = 0;
    g_app_root[0] = 0;
    g_user_root[0] = 0;
    if (app_root && app_root[0]) copy_str(g_app_root, CONTENT_PATH, app_root);
    if (user_root && user_root[0]) copy_str(g_user_root, CONTENT_PATH, user_root);
    for (int i = 0; i < CF_COUNT; i++) {
        g_app_dirs[i][0] = 0;
        g_user_dirs[i][0] = 0;
        if (g_app_root[0]) join_path(g_app_dirs[i], CONTENT_PATH, g_app_root, FOLDER_NAMES[i]);
        if (g_user_root[0]) join_path(g_user_dirs[i], CONTENT_PATH, g_user_root, FOLDER_NAMES[i]);
    }
    g_ready = g_app_root[0] || g_user_root[0];
    if (!g_ready) return 0;
    for (int i = 0; i < CF_COUNT; i++) {
        if (g_user_dirs[i][0]) make_dirs(g_user_dirs[i]);
        if (g_app_dirs[i][0]) make_dirs(g_app_dirs[i]);
    }
    content_scan();
    return 1;
}

void content_shutdown(void) {
    g_ready = 0;
    g_part_count = g_pack_count = g_mod_count = 0;
}

const char *content_dir(int folder) {
    if (folder < 0 || folder >= CF_COUNT) return "";
    if (g_user_dirs[folder][0]) return g_user_dirs[folder];
    return g_app_dirs[folder];
}

const char *content_app_dir(int folder) {
    if (folder < 0 || folder >= CF_COUNT) return "";
    return g_app_dirs[folder];
}

const char *content_user_root(void) { return g_user_root; }

int content_part_count(void) { return g_part_count; }
const ContentPart *content_part(int index) {
    if (index < 0 || index >= g_part_count) return NULL;
    return &g_parts[index];
}

int content_pack_count(void) { return g_pack_count; }
const ContentPack *content_pack(int index) {
    if (index < 0 || index >= g_pack_count) return NULL;
    return &g_packs[index];
}

int content_mod_count(void) { return g_mod_count; }
const ContentMod *content_mod(int index) {
    if (index < 0 || index >= g_mod_count) return NULL;
    return &g_mods[index];
}

int content_kind_from_name(const char *name) {
    if (!name) return -1;
    /* Lower case and drop spaces, underscores and dashes so "Gas Giant",
     * "gas_giant" and "gasgiant" all mean the same kind. */
    char clean[24];
    int n = 0;
    for (int i = 0; name[i] && n < (int)sizeof(clean) - 1; i++) {
        char c = name[i];
        if (c == ' ' || c == '\t' || c == '_' || c == '-') continue;
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        clean[n++] = c;
    }
    clean[n] = 0;
    if (!n) return -1;
    static const char *NAMES[] = {"star", "terrestrial", "giant", "ice", "dwarf",
                                  "moon", "asteroid", "comet", "blackhole"};
    static const char *ALIASES[] = {"sun", "planet", "gasgiant", "icegiant", "planetoid",
                                    "satellite", "rock", "comet", "singularity"};
    for (int i = 0; i < (int)(sizeof(NAMES) / sizeof(NAMES[0])); i++)
        if (strcmp(clean, NAMES[i]) == 0 || strcmp(clean, ALIASES[i]) == 0) return i;
    return -1;
}

/* ------------------------------------------------------------------ log */

void content_log(const char *fmt, ...) {
    if (!g_ready || !g_user_dirs[CF_LOGS][0]) return;
    char line[512];
    int n = 0;
    /* timestamp: keep it simple and locale free */
#ifdef SSS_WINDOWS
    SYSTEMTIME st;
    GetLocalTime(&st);
    n = snprintf(line, sizeof(line), "%04d-%02d-%02d %02d:%02d:%02d  ", st.wYear, st.wMonth,
                 st.wDay, st.wHour, st.wMinute, st.wSecond);
#else
    time_t now = time(NULL);
    struct tm tmv;
#ifdef _POSIX_VERSION
    localtime_r(&now, &tmv);
#else
    tmv = *localtime(&now);
#endif
    n = snprintf(line, sizeof(line), "%04d-%02d-%02d %02d:%02d:%02d  ", tmv.tm_year + 1900,
                 tmv.tm_mon + 1, tmv.tm_mday, tmv.tm_hour, tmv.tm_min, tmv.tm_sec);
#endif
    if (n < 0) n = 0;
    if (n > (int)sizeof(line) - 4) n = (int)sizeof(line) - 4;
    va_list ap;
    va_start(ap, fmt);
    int m = vsnprintf(line + n, sizeof(line) - (size_t)n - 2, fmt, ap);
    va_end(ap);
    if (m < 0) m = 0;
    int used = n + m;
    if (used > (int)sizeof(line) - 2) used = (int)sizeof(line) - 2;
    line[used++] = '\n';
    line[used] = 0;
    char path[CONTENT_PATH];
    join_path(path, sizeof(path), g_user_dirs[CF_LOGS], "app.log");
#ifdef SSS_WINDOWS
    wchar_t wide[CONTENT_PATH * 2];
    int wl = utf8_to_utf16(path, (int)strlen(path), wide, CONTENT_PATH * 2 - 1);
    if (wl <= 0) return;
    wide[wl] = 0;
    HANDLE h = CreateFileW(wide, FILE_APPEND_DATA, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(h, line, (DWORD)used, &written, NULL);
    CloseHandle(h);
#else
    FILE *f = fopen(path, "ab");
    if (!f) return;
    fwrite(line, 1, (size_t)used, f);
    fclose(f);
#endif
}
