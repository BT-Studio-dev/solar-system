/* theme.c -- the palette table and the current selection.
 *
 * Five themes ship with the app; the first one is the original deep-space
 * "Aero Night" look, the others demonstrate the same shell and scene with
 * entirely different palettes.  Adding a theme means appending one entry.
 */
#include "theme.h"

#include <string.h>

#ifdef SSS_WINDOWS
#include <windows.h>
#endif

static const SsTheme THEMES[] = {
    /* ---------------------------------------------------------- Aero Night */
    {
        "aero-night", "Aero Night", "Windows 7 glass over deep space", 1,
        /* shell */
        RGB_A(11, 17, 31), RGB_A(15, 25, 46),
        RGBA(32, 50, 82, 255), RGBA(17, 27, 46, 255), RGBA(132, 178, 255, 40),
        RGB_A(232, 240, 255), RGB_A(150, 172, 205), RGBA(255, 255, 255, 16),
        /* panels */
        RGBA(24, 37, 62, 246), RGBA(15, 24, 41, 246), RGBA(124, 170, 240, 46),
        RGBA(255, 255, 255, 10), RGB_A(126, 164, 224),
        RGB_A(228, 237, 252), RGB_A(158, 176, 205), RGB_A(112, 130, 158),
        RGB_A(86, 102, 128),
        RGBA(255, 255, 255, 10), RGBA(255, 255, 255, 4), RGBA(255, 255, 255, 16),
        /* accents */
        RGB_A(86, 160, 255), RGB_A(140, 196, 255), RGB_A(34, 86, 164),
        RGB_A(104, 214, 168), RGB_A(255, 196, 110), RGB_A(255, 112, 112),
        RGB_A(178, 148, 255),
        /* transport and buttons */
        RGBA(20, 32, 54, 255), RGBA(14, 22, 38, 255), RGBA(120, 160, 226, 34),
        RGBA(255, 255, 255, 20),
        RGBA(46, 66, 102, 255), RGBA(27, 41, 66, 255), RGBA(148, 186, 245, 60),
        RGBA(62, 88, 132, 255), RGBA(36, 54, 86, 255), RGBA(168, 206, 255, 110),
        RGBA(20, 34, 56, 255), RGBA(28, 44, 70, 255),
        RGB_A(66, 132, 224), RGB_A(30, 84, 164), RGBA(170, 214, 255, 120),
        /* close */
        RGB_A(226, 76, 70), RGB_A(176, 48, 44),
        /* viewport */
        RGB_A(6, 9, 17), RGB_A(12, 20, 36),
        RGBA(28, 52, 104, 34), RGBA(44, 26, 78, 30),
        RGB_A(206, 226, 255), RGB_A(255, 226, 186), RGB_A(190, 220, 255),
        RGB_A(120, 160, 220), RGB_A(170, 205, 255),
        RGB_A(188, 204, 228), RGB_A(158, 176, 202), RGBA(0, 0, 0, 120),
        RGBA(232, 240, 255, 220), RGBA(140, 162, 196, 190),
        RGBA(10, 16, 28, 210), RGBA(120, 160, 220, 70),
        RGB_A(228, 237, 252), RGB_A(158, 176, 205),
        255, 110,
        RGB_A(255, 255, 255), RGB_A(255, 255, 255),
        RGBA(255, 255, 255, 6), RGBA(255, 255, 255, 16), RGBA(255, 255, 255, 14),
        RGBA(255, 255, 255, 16), RGB_A(235, 242, 255),
        RGBA(18, 28, 46, 225), RGBA(10, 17, 30, 225), RGBA(130, 172, 240, 46),
        RGB_A(228, 237, 252), RGB_A(168, 186, 212),
    },

    /* ------------------------------------------------------------ Aero Day */
    {
        "aero-day", "Aero Day", "Light glass and a blueprint-style chart", 0,
        RGB_A(223, 238, 251), RGB_A(201, 223, 243),
        RGBA(244, 249, 255, 255), RGBA(207, 228, 248, 255), RGBA(90, 130, 180, 90),
        RGB_A(26, 51, 82), RGB_A(85, 112, 142), RGBA(255, 255, 255, 90),
        RGBA(250, 253, 255, 248), RGBA(232, 242, 252, 248), RGBA(120, 160, 205, 90),
        RGBA(255, 255, 255, 120), RGB_A(47, 97, 158),
        RGB_A(28, 51, 80), RGB_A(71, 96, 126), RGB_A(113, 136, 159),
        RGB_A(147, 167, 186),
        RGBA(150, 185, 225, 26), RGBA(150, 185, 225, 12), RGBA(90, 130, 180, 50),
        RGB_A(42, 111, 214), RGB_A(90, 151, 232), RGB_A(28, 79, 156),
        RGB_A(31, 138, 92), RGB_A(184, 116, 26), RGB_A(192, 57, 43),
        RGB_A(106, 79, 174),
        RGBA(238, 246, 255, 255), RGBA(214, 230, 248, 255), RGBA(90, 130, 180, 70),
        RGBA(60, 90, 130, 40),
        RGBA(255, 255, 255, 255), RGBA(219, 232, 247, 255), RGBA(110, 150, 195, 110),
        RGBA(242, 249, 255, 255), RGBA(207, 227, 248, 255), RGBA(70, 120, 190, 170),
        RGBA(195, 216, 240, 255), RGBA(215, 230, 246, 255),
        RGB_A(99, 161, 234), RGB_A(45, 111, 201), RGBA(255, 255, 255, 150),
        RGB_A(217, 79, 69), RGB_A(185, 58, 51),
        /* viewport: paper chart */
        RGB_A(238, 246, 255), RGB_A(217, 231, 245),
        RGBA(255, 255, 255, 90), RGBA(180, 205, 235, 80),
        RGB_A(70, 100, 140), RGB_A(160, 110, 60), RGB_A(255, 214, 150),
        RGB_A(84, 120, 168), RGB_A(40, 90, 160),
        RGB_A(27, 51, 80), RGB_A(68, 94, 124), RGBA(255, 255, 255, 170),
        RGBA(20, 45, 75, 235), RGBA(60, 85, 115, 220),
        RGBA(255, 255, 255, 235), RGBA(90, 130, 180, 120),
        RGB_A(23, 48, 79), RGB_A(76, 102, 131),
        70, 26,
        RGB_A(255, 255, 255), RGB_A(255, 255, 255),
        RGBA(30, 70, 120, 8), RGBA(30, 70, 120, 20), RGBA(30, 70, 120, 30),
        RGBA(40, 80, 130, 26), RGB_A(255, 255, 255),
        RGBA(38, 56, 84, 232), RGBA(22, 36, 58, 238), RGBA(150, 190, 240, 90),
        RGB_A(240, 246, 255), RGB_A(196, 212, 232),
    },

    /* ---------------------------------------------------------------- Luna */
    {
        "luna", "Luna Blue", "XP-era blue chrome with cream panels", 0,
        RGB_A(236, 233, 216), RGB_A(214, 210, 199),
        RGBA(10, 95, 214, 255), RGBA(63, 140, 243, 255), RGBA(255, 255, 255, 80),
        RGB_A(255, 255, 255), RGBA(214, 232, 255, 235), RGBA(255, 255, 255, 40),
        RGBA(252, 250, 240, 250), RGBA(236, 232, 216, 250), RGBA(120, 140, 170, 110),
        RGBA(255, 255, 255, 110), RGB_A(29, 79, 156),
        RGB_A(26, 31, 38), RGB_A(76, 85, 96), RGB_A(116, 128, 140),
        RGB_A(150, 160, 170),
        RGBA(90, 120, 170, 22), RGBA(90, 120, 170, 8), RGBA(120, 140, 170, 60),
        RGB_A(26, 99, 200), RGB_A(63, 140, 243), RGB_A(10, 74, 158),
        RGB_A(47, 143, 63), RGB_A(192, 122, 16), RGB_A(194, 42, 34),
        RGB_A(106, 79, 174),
        RGBA(250, 248, 240, 255), RGBA(232, 228, 214, 255), RGBA(150, 160, 175, 90),
        RGBA(60, 70, 90, 40),
        RGBA(255, 255, 255, 255), RGBA(228, 225, 212, 255), RGBA(120, 140, 170, 120),
        RGBA(247, 244, 232, 255), RGBA(217, 213, 198, 255), RGBA(80, 110, 160, 170),
        RGBA(214, 210, 196, 255), RGBA(239, 236, 223, 255),
        RGB_A(63, 140, 243), RGB_A(10, 95, 214), RGBA(255, 255, 255, 150),
        RGB_A(224, 80, 72), RGB_A(182, 48, 40),
        /* viewport: night sky */
        RGB_A(5, 10, 24), RGB_A(13, 24, 44),
        RGBA(40, 70, 130, 34), RGBA(70, 45, 110, 28),
        RGB_A(207, 224, 255), RGB_A(255, 230, 194), RGB_A(190, 220, 255),
        RGB_A(130, 160, 200), RGB_A(180, 210, 255),
        RGB_A(210, 222, 238), RGB_A(150, 170, 195), RGBA(0, 0, 0, 130),
        RGBA(240, 246, 255, 230), RGBA(150, 170, 195, 200),
        RGBA(8, 14, 26, 225), RGBA(140, 175, 230, 90),
        RGB_A(235, 242, 255), RGB_A(160, 180, 205),
        255, 110,
        RGB_A(255, 255, 255), RGB_A(255, 255, 255),
        RGBA(30, 60, 110, 8), RGBA(30, 60, 110, 18), RGBA(30, 60, 110, 28),
        RGBA(40, 70, 120, 30), RGB_A(255, 255, 255),
        RGBA(24, 40, 68, 235), RGBA(14, 24, 44, 240), RGBA(140, 180, 235, 80),
        RGB_A(236, 244, 255), RGB_A(186, 204, 228),
    },

    /* -------------------------------------------------------------- Aurora */
    {
        "aurora", "Aurora", "Teal and violet over polar night", 1,
        RGB_A(9, 12, 30), RGB_A(16, 20, 44),
        RGBA(38, 20, 74, 255), RGBA(16, 36, 60, 255), RGBA(120, 255, 230, 60),
        RGB_A(232, 255, 250), RGB_A(150, 200, 205), RGBA(180, 255, 240, 14),
        RGBA(20, 28, 48, 248), RGBA(12, 18, 34, 248), RGBA(90, 220, 210, 70),
        RGBA(180, 255, 240, 10), RGB_A(96, 232, 210),
        RGB_A(224, 250, 246), RGB_A(150, 190, 192), RGB_A(110, 150, 155),
        RGB_A(85, 120, 128),
        RGBA(160, 255, 235, 12), RGBA(160, 255, 235, 5), RGBA(90, 220, 210, 40),
        RGB_A(64, 224, 196), RGB_A(130, 255, 228), RGB_A(20, 120, 120),
        RGB_A(140, 240, 160), RGB_A(255, 206, 120), RGB_A(255, 120, 140),
        RGB_A(190, 150, 255),
        RGBA(16, 24, 42, 255), RGBA(10, 16, 30, 255), RGBA(90, 220, 210, 50),
        RGBA(180, 255, 240, 24),
        RGBA(34, 56, 78, 255), RGBA(20, 36, 54, 255), RGBA(90, 220, 210, 70),
        RGBA(46, 78, 96, 255), RGBA(28, 50, 68, 255), RGBA(140, 255, 235, 120),
        RGBA(14, 30, 44, 255), RGBA(22, 40, 56, 255),
        RGB_A(64, 224, 196), RGB_A(16, 140, 140), RGBA(200, 255, 245, 140),
        RGB_A(230, 90, 120), RGB_A(180, 50, 80),
        /* viewport */
        RGB_A(4, 10, 18), RGB_A(8, 24, 32),
        RGBA(20, 120, 110, 34), RGBA(90, 40, 130, 34),
        RGB_A(200, 255, 245), RGB_A(255, 220, 170), RGB_A(150, 255, 225),
        RGB_A(90, 180, 175), RGB_A(150, 255, 235),
        RGB_A(190, 235, 230), RGB_A(130, 175, 175), RGBA(0, 0, 0, 120),
        RGBA(220, 255, 250, 225), RGBA(130, 175, 175, 200),
        RGBA(6, 18, 24, 220), RGBA(120, 230, 215, 80),
        RGB_A(224, 250, 246), RGB_A(150, 190, 192),
        255, 110,
        RGB_A(4, 32, 30), RGB_A(240, 255, 250),
        RGBA(180, 255, 240, 6), RGBA(180, 255, 240, 16), RGBA(180, 255, 240, 22),
        RGBA(180, 255, 240, 18), RGB_A(235, 255, 250),
        RGBA(14, 24, 40, 228), RGBA(8, 15, 28, 234), RGBA(90, 220, 210, 70),
        RGB_A(224, 250, 246), RGB_A(158, 200, 198),
    },

    /* ------------------------------------------------------- High Contrast */
    {
        "high-contrast", "High Contrast", "Black, white and yellow", 1,
        RGB_A(0, 0, 0), RGB_A(0, 0, 0),
        RGBA(0, 0, 0, 255), RGBA(0, 0, 0, 255), RGBA(255, 255, 255, 255),
        RGB_A(255, 255, 255), RGB_A(255, 255, 0), RGBA(255, 255, 255, 0),
        RGBA(0, 0, 0, 255), RGBA(0, 0, 0, 255), RGBA(255, 255, 255, 255),
        RGBA(255, 255, 255, 0), RGB_A(255, 255, 0),
        RGB_A(255, 255, 255), RGB_A(255, 255, 0), RGB_A(0, 255, 255),
        RGB_A(176, 176, 176),
        RGBA(0, 0, 0, 255), RGBA(0, 0, 0, 255), RGBA(255, 255, 255, 255),
        RGB_A(255, 255, 0), RGB_A(0, 255, 255), RGB_A(255, 255, 0),
        RGB_A(0, 255, 0), RGB_A(255, 255, 0), RGB_A(255, 0, 0),
        RGB_A(255, 0, 255),
        RGBA(0, 0, 0, 255), RGBA(0, 0, 0, 255), RGBA(255, 255, 255, 255),
        RGBA(128, 128, 128, 255),
        RGBA(0, 0, 0, 255), RGBA(0, 0, 0, 255), RGBA(255, 255, 255, 255),
        RGBA(51, 51, 51, 255), RGBA(51, 51, 51, 255), RGBA(255, 255, 0, 255),
        RGBA(85, 85, 85, 255), RGBA(85, 85, 85, 255),
        RGB_A(0, 0, 0), RGB_A(0, 0, 0), RGBA(255, 255, 0, 255),
        RGB_A(255, 0, 0), RGB_A(160, 0, 0),
        /* viewport */
        RGB_A(0, 0, 0), RGB_A(0, 0, 0),
        RGBA(0, 0, 0, 0), RGBA(0, 0, 0, 0),
        RGB_A(255, 255, 255), RGB_A(255, 255, 0), RGB_A(255, 255, 255),
        RGB_A(255, 255, 0), RGB_A(0, 255, 255),
        RGB_A(255, 255, 255), RGB_A(255, 255, 0), RGBA(0, 0, 0, 255),
        RGBA(255, 255, 255, 255), RGBA(255, 255, 0, 255),
        RGBA(0, 0, 0, 255), RGBA(255, 255, 255, 255),
        RGB_A(255, 255, 255), RGB_A(255, 255, 0),
        255, 0,
        RGB_A(0, 0, 0), RGB_A(255, 255, 0),
        RGBA(0, 0, 0, 0), RGBA(51, 51, 51, 255), RGBA(85, 85, 85, 255),
        RGB_A(51, 51, 51), RGB_A(255, 255, 0),
        RGBA(0, 0, 0, 246), RGBA(0, 0, 0, 246), RGBA(255, 255, 255, 255),
        RGB_A(255, 255, 255), RGB_A(255, 255, 0),
    },
};

#define THEME_COUNT ((int)(sizeof(THEMES) / sizeof(THEMES[0])))

static int g_current;

int sss_theme_count(void) { return THEME_COUNT; }

const SsTheme *sss_theme_at(int index) {
    if (index < 0 || index >= THEME_COUNT) index = 0;
    return &THEMES[index];
}

const SsTheme *sss_theme(void) { return &THEMES[g_current]; }

int sss_theme_index(void) { return g_current; }

void sss_theme_set(int index) {
    if (index < 0) index = 0;
    if (index >= THEME_COUNT) index = THEME_COUNT - 1;
    g_current = index;
}

static int same_id(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a++, cb = *b++;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb + 32);
        if (ca != cb) return 0;
    }
    return *a == 0 && *b == 0;
}

int sss_theme_find(const char *id) {
    if (!id) return -1;
    for (int i = 0; i < THEME_COUNT; i++)
        if (same_id(THEMES[i].id, id)) return i;
    return -1;
}

#ifdef SSS_WINDOWS

int sss_theme_pref_load(const wchar_t *ini_path) {
    if (!ini_path) return -1;
    HANDLE h = CreateFileW(ini_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return -1;
    char buf[512];
    DWORD read = 0;
    int found = -1;
    if (ReadFile(h, buf, sizeof(buf) - 1, &read, NULL)) {
        buf[read] = 0;
        const char *key = "theme=";
        const char *p = buf;
        while (*p) {
            const char *line = p;
            while (*p && *p != '\n' && *p != '\r') p++;
            if (strncmp(line, key, 6) == 0) {
                char id[40];
                int n = 0;
                const char *v = line + 6;
                while (v < p && n < (int)sizeof(id) - 1) id[n++] = *v++;
                while (n > 0 && (id[n - 1] == ' ' || id[n - 1] == '\t')) n--;
                id[n] = 0;
                int index = sss_theme_find(id);
                if (index >= 0) {
                    g_current = index;
                    found = index;
                }
            }
            while (*p == '\n' || *p == '\r') p++;
        }
    }
    CloseHandle(h);
    return found;
}

void sss_theme_pref_save(const wchar_t *ini_path, int index) {
    if (!ini_path) return;
    const SsTheme *t = sss_theme_at(index);
    char line[96];
    int n = 0;
    const char *a = "theme=";
    while (*a) line[n++] = *a++;
    for (const char *c = t->id; *c && n < (int)sizeof(line) - 3; c++) line[n++] = *c;
    line[n++] = '\r';
    line[n++] = '\n';
    HANDLE h = CreateFileW(ini_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                           NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(h, line, (DWORD)n, &written, NULL);
    CloseHandle(h);
}

#else

int sss_theme_pref_load(const wchar_t *ini_path) {
    (void)ini_path;
    return -1;
}

void sss_theme_pref_save(const wchar_t *ini_path, int index) {
    (void)ini_path;
    (void)index;
}

#endif
