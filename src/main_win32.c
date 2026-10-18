/* main_win32.c -- native Windows shell: window, DPI, input, files, rendering.
 *
 * Targets Windows 7 SP1 and later, 32-bit.  Uses only APIs present on
 * Windows 7; no CRT, no MSVCRT/UCRT dependency (MinGW runtime is static).
 */
#include "app.h"
#include "theme.h"
#include "resource.h"

#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <gdiplus/gdiplus.h>
#include <math.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define WINDOW_CLASS L"SolarSystemSandboxWindow"
#define APP_NAME L"Solar System Sandbox"
#define MIN_W 900
#define MIN_H 600

static App *g_app;
static HWND g_hwnd;
static HMENU g_menu;
static HDC g_back_dc;
static int g_back_w, g_back_h;
static SsGfx *g_gfx;
static SsSurface *g_surface;
static ULONGLONG g_last_tick;
static ULONGLONG g_freq;
static double g_fps_acc;
static int g_fps_frames;
static ULONGLONG g_last_input;
static ULONGLONG g_last_paint;
static ULONG_PTR g_gdiplus;

#define WM_APP_TICK (WM_APP + 1)
#define WM_APP_TOAST (WM_APP + 2)
#define IDT_TICK 1
#define IDT_TOAST 2

/* ------------------------------------------------------------- utilities */

static void show_error(const wchar_t *text) {
    MessageBoxW(g_hwnd, text, APP_NAME, MB_OK | MB_ICONWARNING);
}

static int get_dpi_for_window(HWND hwnd) {
    /* Windows 8.1+ exposes GetDpiForWindow; fall back gracefully on Win7. */
    typedef UINT(WINAPI * GetDpiFn)(HWND);
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        GetDpiFn fn = (GetDpiFn)(void *)GetProcAddress(user32, "GetDpiForWindow");
        if (fn) {
            UINT dpi = fn(hwnd);
            if (dpi >= 96 && dpi <= 480) return (int)dpi;
        }
    }
    HDC dc = GetDC(hwnd);
    int dpi = 96;
    if (dc) {
        dpi = GetDeviceCaps(dc, LOGPIXELSX);
        ReleaseDC(hwnd, dc);
    }
    if (dpi < 96) dpi = 96;
    if (dpi > 240) dpi = 240;
    return dpi;
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static void recreate_backbuffer(int w, int h);

/* ---------------------------------------------------------------- render */

static void recreate_backbuffer(int w, int h) {
    if (g_surface) {
        if (g_gfx) gfx_destroy(g_gfx);
        surf_destroy(g_surface);
        g_surface = NULL;
        g_gfx = NULL;
    }
    if (g_back_dc) {
        DeleteDC(g_back_dc);
        g_back_dc = NULL;
    }
    g_back_w = w;
    g_back_h = h;
    g_surface = surf_create(w, h);
    if (g_surface) {
        g_gfx = gfx_create(g_surface);
        g_back_dc = (HDC)surf_native_dc(g_surface);
    }
    gfx_set_text_scale(g_app ? g_app->dpi : 1.0f);
    gfx_flush_fonts();
}

/* The surface owns a DIB that GDI+ draws into; blitting it is one GDI call. */
static void present(HDC dest, int w, int h) {
    if (!g_surface) return;
    HDC src = (HDC)surf_native_dc(g_surface);
    if (!src) return;
    BitBlt(dest, 0, 0, w, h, src, 0, 0, SRCCOPY);
}

static void render_now(void) {
    if (!g_gfx) return;
    app_draw(g_app, g_gfx);
    HDC dc = GetDC(g_hwnd);
    if (dc) {
        present(dc, g_app->width, g_app->height);
        ReleaseDC(g_hwnd, dc);
    }
}

/* ------------------------------------------------------------- startup log */

static void copy_wide(wchar_t *dst, int cap, const wchar_t *src);

/* One breadcrumb line per stage, appended to %TEMP%\SolarSandbox-startup.log
 * and closed again straight away, so a run that never reaches the message loop
 * still leaves a usable trace on the user's machine. */
static void trace(const char *fmt, ...) {
    wchar_t dir[MAX_PATH];
    if (!GetTempPathW(MAX_PATH, dir)) return;
    wchar_t path[MAX_PATH];
    copy_wide(path, MAX_PATH, dir);
    const wchar_t *leaf = L"SolarSandbox-startup.log";
    int len = (int)wcslen(path);
    for (int i = 0; leaf[i] && len < MAX_PATH - 1; i++) path[len++] = leaf[i];
    path[len] = 0;

    char line[768];
    SYSTEMTIME st;
    GetLocalTime(&st);
    int off = snprintf(line, sizeof(line), "%04d-%02d-%02d %02d:%02d:%02d  ", st.wYear, st.wMonth,
                       st.wDay, st.wHour, st.wMinute, st.wSecond);
    if (off < 0) off = 0;
    if (off > (int)sizeof(line) - 4) off = (int)sizeof(line) - 4;
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(line + off, sizeof(line) - (size_t)off - 3, fmt, ap);
    va_end(ap);
    if (n < 0) n = 0;
    off += n;
    if (off > (int)sizeof(line) - 3) off = (int)sizeof(line) - 3;
    line[off++] = '\r';
    line[off++] = '\n';

    static int created;
    DWORD mode = created ? OPEN_ALWAYS : CREATE_ALWAYS;
    created = 1;
    HANDLE h = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, mode,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD wrote = 0;
    WriteFile(h, line, (DWORD)off, &wrote, NULL);
    CloseHandle(h);
}

/* "--safe" skips the saved theme and the content scan: a way in when a folder
 * or a settings file is making startup crawl. */
static int has_flag(const wchar_t *flag) {
    const wchar_t *cmd = GetCommandLineW();
    if (!cmd || !flag) return 0;
    for (const wchar_t *p = cmd; *p; p++) {
        if (p != cmd && p[-1] != L' ' && p[-1] != L'\t' && p[-1] != L'"') continue;
        int i = 0;
        while (flag[i]) {
            wchar_t c = p[i];
            wchar_t f = flag[i];
            if (c >= L'A' && c <= L'Z') c = (wchar_t)(c + 32);
            if (f >= L'A' && f <= L'Z') f = (wchar_t)(f + 32);
            if (c != f) break;
            i++;
        }
        if (!flag[i]) return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------- dialogs */

static void copy_wide(wchar_t *dst, int cap, const wchar_t *src) {
    int i = 0;
    while (src[i] && i < cap - 1) { dst[i] = src[i]; i++; }
    dst[i] = 0;
}

/* ASCII/UTF-8 -> wide, no locale dependence (all UI strings here are ASCII). */
static void widen(wchar_t *dst, int cap, const char *src) {
    int i = 0;
    while (src[i] && i < cap - 1) { dst[i] = (wchar_t)(unsigned char)src[i]; i++; }
    dst[i] = 0;
}


/* UTF-8 <-> UTF-16 for the content folders, which store UTF-8 paths. */
static int wide_to_utf8(char *dst, int cap, const wchar_t *src) {
    dst[0] = 0;
    if (!src || !src[0] || cap < 2) return 0;
    int n = WideCharToMultiByte(CP_UTF8, 0, src, -1, dst, cap, NULL, NULL);
    if (n <= 0) { dst[0] = 0; return 0; }
    return n - 1;
}

static int utf8_to_wide(const char *src, wchar_t *dst, int cap) {
    dst[0] = 0;
    if (!src || !src[0] || cap < 2) return 0;
    int n = MultiByteToWideChar(CP_UTF8, 0, src, -1, dst, cap);
    if (n <= 0) { dst[0] = 0; return 0; }
    return n - 1;
}

static int exe_dir(wchar_t *path, int cap) {
    DWORD n = GetModuleFileNameW(NULL, path, (DWORD)cap);
    if (n == 0 || n >= (DWORD)cap) return 0;
    for (int i = (int)n - 1; i >= 0; i--) {
        if (path[i] == L'\\' || path[i] == L'/') {
            path[i] = 0;
            return 1;
        }
    }
    return 0;
}

static int appdata_dir(wchar_t *path, int cap) {
    wchar_t base[MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, base) != S_OK) return 0;
    copy_wide(path, cap, base);
    int n = (int)wcslen(path);
    const wchar_t *tail = L"\\Solar System Sandbox";
    for (int i = 0; tail[i] && n < cap - 1; i++) path[n++] = tail[i];
    path[n] = 0;
    CreateDirectoryW(path, NULL);
    return 1;
}

static int save_file_dialog(wchar_t *path, int cap) {
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    {
        const char *dir = content_dir(CF_SAVING);
        char utf8[CONTENT_PATH * 2];
        if (dir && dir[0])
            snprintf(utf8, sizeof(utf8), "%s\\SolarSystem.sims", dir);
        else
            snprintf(utf8, sizeof(utf8), "SolarSystem.sims");
        if (!utf8_to_wide(utf8, path, cap)) copy_wide(path, cap, L"SolarSystem.sims");
    }
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = L"Simulation (*.sims)\0*.sims\0All files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = cap;
    ofn.lpstrDefExt = L"sims";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    return GetSaveFileNameW(&ofn) ? 1 : 0;
}

static int open_file_dialog(wchar_t *path, int cap) {
    OPENFILENAMEW ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    path[0] = 0;
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd;
    ofn.lpstrFilter = L"Simulation (*.sims)\0*.sims\0All files\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = cap;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    return GetOpenFileNameW(&ofn) ? 1 : 0;
}

/* Minimal wide formatter: substitutes the first three %ls arguments in order. */
static void format_wide(wchar_t *dst, int cap, const wchar_t *fmt, const wchar_t *a,
                        const wchar_t *b, const wchar_t *c) {
    const wchar_t *args[3] = {a, b, c};
    int arg = 0;
    int n = 0;
    for (const wchar_t *p = fmt; *p && n < cap - 1; p++) {
        if (p[0] == L'%' && p[1] == L'l' && p[2] == L's') {
            const wchar_t *str = (arg < 3 && args[arg]) ? args[arg] : L"";
            for (int i = 0; str[i] && n < cap - 1; i++) dst[n++] = str[i];
            arg++;
            p += 2;
        } else {
            dst[n++] = *p;
        }
    }
    dst[n] = 0;
}

static void write_text_file(const wchar_t *path, const char *text, int len) {
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        show_error(L"Could not create the file.");
        return;
    }
    DWORD written = 0;
    WriteFile(h, text, (DWORD)len, &written, NULL);
    CloseHandle(h);
}

static char *read_text_file(const wchar_t *path, int *out_len) {
    HANDLE h = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return NULL;
    DWORD size = GetFileSize(h, NULL);
    if (size == INVALID_FILE_SIZE || size > 4u * 1024u * 1024u) {
        CloseHandle(h);
        return NULL;
    }
    char *buf = (char *)sss_alloc(size + 1);
    if (!buf) {
        CloseHandle(h);
        return NULL;
    }
    DWORD read = 0;
    if (!ReadFile(h, buf, size, &read, NULL)) {
        CloseHandle(h);
        sss_free(buf);
        return NULL;
    }
    CloseHandle(h);
    buf[read] = 0;
    if (out_len) *out_len = (int)read;
    return buf;
}

/* ---------------------------------------------------------------- app hooks */

static void hook_save(App *app) {
    wchar_t path[MAX_PATH];
    if (!save_file_dialog(path, MAX_PATH)) return;
    int cap = 6 * 1024 * 1024;
    char *buf = (char *)sss_alloc(cap);
    if (!buf) return;
    int len = app_save_text(app, buf, cap);
    write_text_file(path, buf, len);
    sss_free(buf);
    content_log("saved simulation (%d bytes)", len);
    app_toast(app, "Simulation saved", 0);
    render_now();
}

static void hook_load(App *app) {
    wchar_t path[MAX_PATH];
    if (!open_file_dialog(path, MAX_PATH)) return;
    char *text = read_text_file(path, NULL);
    if (!text) {
        app_toast(app, "Could not read that file", 1);
        return;
    }
    app_load_text(app, text);
    sss_free(text);
    content_log("loaded a simulation from disk");
    render_now();
}

static void take_screenshot(App *app) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    wchar_t path[MAX_PATH];
    char ascii[64];
    snprintf(ascii, sizeof(ascii), "SolarSandbox-%04d%02d%02d-%02d%02d%02d.bmp", st.wYear, st.wMonth,
             st.wDay, st.wHour, st.wMinute, st.wSecond);
    widen(path, MAX_PATH, ascii);
    int w = app->width, h = app->height;
    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;
    ZeroMemory(&fh, sizeof(fh));
    ZeroMemory(&ih, sizeof(ih));
    ih.biSize = sizeof(ih);
    ih.biWidth = w;
    ih.biHeight = -h; /* the surface DIB is top-down */
    ih.biPlanes = 1;
    ih.biBitCount = 32;
    ih.biCompression = BI_RGB;
    ih.biSizeImage = (DWORD)w * (DWORD)h * 4;
    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + ih.biSizeImage;
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file != INVALID_HANDLE_VALUE) {
        DWORD written;
        WriteFile(file, &fh, sizeof(fh), &written, NULL);
        WriteFile(file, &ih, sizeof(ih), &written, NULL);
        WriteFile(file, surf_pixels(g_surface), ih.biSizeImage, &written, NULL);
        CloseHandle(file);
        content_log("screenshot written as %s", ascii);
        app_toast(app, "Screenshot saved next to the app", 0);
    }
}

static void hook_screenshot(App *app) { take_screenshot(app); }
static void hook_quit(App *app) { (void)app; PostMessageW(g_hwnd, WM_CLOSE, 0, 0); }
static void hook_minimize(App *app) { (void)app; ShowWindow(g_hwnd, SW_MINIMIZE); }
static void hook_maximize(App *app) {
    (void)app;
    ShowWindow(g_hwnd, IsZoomed(g_hwnd) ? SW_RESTORE : SW_MAXIMIZE);
}

static void hook_file_save_menu(App *app) { hook_save(app); }

/* Read a UTF-8 path for the content API (parts, packs and mods). */
static char *hook_read_text(const char *utf8_path, int *out_len) {
    wchar_t wide[CONTENT_PATH * 2];
    if (!utf8_to_wide(utf8_path, wide, CONTENT_PATH * 2)) return NULL;
    return read_text_file(wide, out_len);
}

/* Open a folder (or file) with the shell so users can drop content in. */
static void hook_open_path(App *app, const char *utf8_path) {
    (void)app;
    wchar_t wide[CONTENT_PATH * 2];
    if (!utf8_to_wide(utf8_path, wide, CONTENT_PATH * 2)) return;
    ShellExecuteW(g_hwnd, L"open", wide, NULL, NULL, SW_SHOWNORMAL);
}

/* -------------------------------------------------------------- settings */

/* Settings live in %APPDATA%\Solar System Sandbox\settings.ini.  The theme is
 * the only preference so far, but reading is tolerant of unknown keys. */
static int settings_path(wchar_t *path, int cap) {
    if (!appdata_dir(path, cap)) return 0;
    int n = (int)wcslen(path);
    const wchar_t *file = L"\\settings.ini";
    for (int i = 0; file[i] && n < cap - 1; i++) path[n++] = file[i];
    path[n] = 0;
    return 1;
}

static void hook_theme(App *app, int index) {
    (void)app;
    wchar_t path[MAX_PATH];
    if (settings_path(path, MAX_PATH)) sss_theme_pref_save(path, index);
    content_log("theme set to '%s'", sss_theme_at(index)->id);
}

/* ---------------------------------------------------------------- menu */

static void build_menu(void) {
    g_menu = CreateMenu();
    HMENU file = CreatePopupMenu();
    HMENU view = CreatePopupMenu();
    HMENU help = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, IDM_FILE_SAVE, L"&Save simulation...\tCtrl+S");
    AppendMenuW(file, MF_STRING, IDM_FILE_LOAD, L"&Open simulation...\tCtrl+O");
    AppendMenuW(file, MF_SEPARATOR, 0, NULL);
    AppendMenuW(file, MF_STRING, IDM_FILE_SHOT, L"Save &screenshot\tF12");
    AppendMenuW(file, MF_SEPARATOR, 0, NULL);
    AppendMenuW(file, MF_STRING, IDM_FILE_FOLDERS, L"Open content &folders");
    AppendMenuW(file, MF_STRING, IDM_FILE_RESCAN, L"&Rescan content");
    AppendMenuW(file, MF_SEPARATOR, 0, NULL);
    AppendMenuW(file, MF_STRING, IDM_FILE_EXIT, L"E&xit\tAlt+F4");
    AppendMenuW(view, MF_STRING, IDM_VIEW_ORBITS, L"Show &orbits\tO");
    AppendMenuW(view, MF_STRING, IDM_VIEW_LABELS, L"Show &labels\tL");
    AppendMenuW(view, MF_STRING, IDM_VIEW_TRAILS, L"Show &trails\tT");
    AppendMenuW(view, MF_STRING, IDM_VIEW_MOONS, L"Show &moons\tM");
    AppendMenuW(view, MF_SEPARATOR, 0, NULL);
    AppendMenuW(view, MF_STRING, IDM_VIEW_SCALE, L"&True scale");
    AppendMenuW(view, MF_STRING, IDM_VIEW_RESET, L"&Reset view\tR");
    AppendMenuW(view, MF_SEPARATOR, 0, NULL);
    AppendMenuW(view, MF_STRING, IDM_VIEW_THEME, L"&Colour theme...\tF3");
    AppendMenuW(view, MF_SEPARATOR, 0, NULL);
    AppendMenuW(view, MF_STRING, IDM_VIEW_SIM, L"&Simulation controls...\tC");
    AppendMenuW(help, MF_STRING, IDM_HELP_HELP, L"&Keyboard shortcuts\tF1");
    AppendMenuW(help, MF_STRING, IDM_HELP_LOG, L"Open &log file");
    AppendMenuW(help, MF_STRING, IDM_HELP_STARTUP, L"Open &startup log");
    AppendMenuW(help, MF_SEPARATOR, 0, NULL);
    AppendMenuW(help, MF_STRING, IDM_HELP_ABOUT, L"&About Solar System Sandbox");
    AppendMenuW(g_menu, MF_POPUP, (UINT_PTR)file, L"&File");
    AppendMenuW(g_menu, MF_POPUP, (UINT_PTR)view, L"&View");
    AppendMenuW(g_menu, MF_POPUP, (UINT_PTR)help, L"&Help");
}

/* ---------------------------------------------------------------- input */

static void send_event(const UiEvent *ev) {
    g_last_input = GetTickCount64();
    app_event(g_app, ev);
}

static int key_from_wparam(WPARAM wp) {
    switch (wp) {
        case VK_SPACE: return UI_KEY_SPACE;
        case VK_ESCAPE: return UI_KEY_ESC;
        case VK_RETURN: return UI_KEY_ENTER;
        case VK_DELETE: return UI_KEY_DELETE;
        case VK_F1: return UI_KEY_F1;
        case VK_F2: return UI_KEY_F2;
        case VK_F3: return UI_KEY_F3;
        case VK_F4: return UI_KEY_F4;
        case VK_F5: return UI_KEY_F5;
        case VK_F6: return UI_KEY_F6;
        case VK_F7: return UI_KEY_F7;
        case VK_F8: return UI_KEY_F8;
        case VK_F9: return UI_KEY_F9;
        case VK_F10: return UI_KEY_F10;
        case VK_F11: return UI_KEY_F11;
        case VK_F12: return UI_KEY_F12;
        case VK_LEFT: return UI_KEY_LEFT;
        case VK_RIGHT: return UI_KEY_RIGHT;
        case VK_UP: return UI_KEY_UP;
        case VK_DOWN: return UI_KEY_DOWN;
        case VK_ADD: return UI_KEY_PLUS;
        case VK_SUBTRACT: return UI_KEY_MINUS;
        default: break;
    }
    if (wp >= '0' && wp <= '9') return (int)wp;
    if (wp >= 'A' && wp <= 'Z') return (int)wp;
    return 0;
}

/* ---------------------------------------------------------------- entry */

/* With no C runtime linked, this is the process entry point (as set by the
 * linker).  It hands control to WinMain and terminates with its result. */
void WinMainCRTStartup(void);

void WinMainCRTStartup(void) {
    int result = WinMain(GetModuleHandleW(NULL), NULL, NULL, SW_SHOWDEFAULT);
    ExitProcess((UINT)result);
}

/* ---------------------------------------------------------------- window */

static void update_dpi(void) {
    int dpi = get_dpi_for_window(g_hwnd);
    float scale = (float)dpi / 96.0f;
    if (scale < 1.0f) scale = 1.0f;
    if (fabsf(scale - g_app->dpi) > 0.001f) {
        g_app->dpi = scale;
        gfx_set_text_scale(scale);
        gfx_flush_fonts();
        RECT rc;
        GetClientRect(g_hwnd, &rc);
        app_resize(g_app, rc.right - rc.left, rc.bottom - rc.top);
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    UiEvent ev;
    ZeroMemory(&ev, sizeof(ev));
    switch (msg) {
        case WM_CREATE:
            g_hwnd = hwnd;
            return 0;
        case WM_SIZE: {
            int w = LOWORD(lp), h = HIWORD(lp);
            if (w < 1 || h < 1) return 0;
            if (!g_app) return 0;
            update_dpi();
            app_resize(g_app, w, h);
            if (w != g_back_w || h != g_back_h) recreate_backbuffer(w, h);
            return 0;
        }
        case WM_GETMINMAXINFO: {
            MINMAXINFO *mmi = (MINMAXINFO *)lp;
            mmi->ptMinTrackSize.x = (LONG)(MIN_W * g_app->dpi);
            mmi->ptMinTrackSize.y = (LONG)(MIN_H * g_app->dpi);
            return 0;
        }
        case WM_DPICHANGED: {
            /* Windows 8.1+ */
            RECT *rc = (RECT *)lp;
            SetWindowPos(hwnd, NULL, rc->left, rc->top, rc->right - rc->left,
                         rc->bottom - rc->top, SWP_NOZORDER | SWP_NOACTIVATE);
            update_dpi();
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            if (g_gfx && g_back_dc)
                BitBlt(dc, 0, 0, g_app->width, g_app->height, g_back_dc, 0, 0, SRCCOPY);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_MOUSEMOVE: {
            ev.type = UI_EV_MOVE;
            ev.x = (float)GET_X_LPARAM(lp);
            ev.y = (float)GET_Y_LPARAM(lp);
            send_event(&ev);
            { TRACKMOUSEEVENT tme; tme.cbSize = sizeof(tme); tme.dwFlags = TME_LEAVE;
              tme.hwndTrack = hwnd; tme.dwHoverTime = 0; TrackMouseEvent(&tme); }
            return 0;
        }
        case WM_MOUSELEAVE: {
            ev.type = UI_EV_MOVE;
            ev.x = -1000;
            ev.y = -1000;
            send_event(&ev);
            return 0;
        }
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            if (msg != WM_LBUTTONDOWN) SetFocus(hwnd);
            ev.type = UI_EV_DOWN;
            ev.x = (float)GET_X_LPARAM(lp);
            ev.y = (float)GET_Y_LPARAM(lp);
            ev.button = msg == WM_LBUTTONDOWN ? UI_MB_LEFT
                                              : (msg == WM_RBUTTONDOWN ? UI_MB_RIGHT : UI_MB_MIDDLE);
            SetCapture(hwnd);
            send_event(&ev);
            return 0;
        }
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: {
            ev.type = UI_EV_UP;
            ev.x = (float)GET_X_LPARAM(lp);
            ev.y = (float)GET_Y_LPARAM(lp);
            ev.button = msg == WM_LBUTTONUP ? UI_MB_LEFT
                                            : (msg == WM_RBUTTONUP ? UI_MB_RIGHT : UI_MB_MIDDLE);
            send_event(&ev);
            ReleaseCapture();
            return 0;
        }
        case WM_LBUTTONDBLCLK: {
            ev.type = UI_EV_DBLCLICK;
            ev.x = (float)GET_X_LPARAM(lp);
            ev.y = (float)GET_Y_LPARAM(lp);
            send_event(&ev);
            return 0;
        }
        case WM_MOUSEWHEEL: {
            POINT p;
            p.x = GET_X_LPARAM(lp);
            p.y = GET_Y_LPARAM(lp);
            ScreenToClient(hwnd, &p);
            ev.type = UI_EV_WHEEL;
            ev.x = (float)p.x;
            ev.y = (float)p.y;
            ev.wheel = (float)GET_WHEEL_DELTA_WPARAM(wp) / 120.0f;
            send_event(&ev);
            return 0;
        }
        case WM_SETCURSOR: {
            if (LOWORD(lp) == HTCLIENT) {
                SetCursor(LoadCursorW(NULL, (LPCWSTR)IDC_ARROW));
                return TRUE;
            }
            break;
        }
        case WM_KEYDOWN: {
            int key = key_from_wparam(wp);
            int ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            if (ctrl && (wp == 'S' || wp == 's')) {
                hook_save(g_app);
                return 0;
            }
            if (ctrl && (wp == 'O' || wp == 'o')) {
                hook_load(g_app);
                return 0;
            }
            if (key) {
                ev.type = UI_EV_KEY;
                ev.key = key;
                ev.shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                ev.ctrl = ctrl;
                ev.alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
                send_event(&ev);
            }
            return 0;
        }
        case WM_COMMAND: {
            switch (LOWORD(wp)) {
                case IDM_FILE_SAVE: hook_save(g_app); break;
                case IDM_FILE_LOAD: hook_load(g_app); break;
                case IDM_FILE_SHOT: take_screenshot(g_app); break;
                case IDM_FILE_EXIT: PostMessageW(hwnd, WM_CLOSE, 0, 0); break;
                case IDM_FILE_FOLDERS: hook_open_path(g_app, content_user_root()); break;
                case IDM_FILE_RESCAN: app_rescan_content(g_app); break;
                case IDM_VIEW_ORBITS: g_app->ui.show_orbits = !g_app->ui.show_orbits; break;
                case IDM_VIEW_LABELS: g_app->ui.show_labels = !g_app->ui.show_labels; break;
                case IDM_VIEW_TRAILS: g_app->ui.show_trails = !g_app->ui.show_trails; break;
                case IDM_VIEW_MOONS: g_app->ui.show_moons = !g_app->ui.show_moons; break;
                case IDM_VIEW_SCALE:
                    g_app->ui.true_scale = !g_app->ui.true_scale;
                    break;
                case IDM_VIEW_RESET: app_reset_view(g_app); break;
                case IDM_VIEW_THEME: app_next_theme(g_app); break;
                case IDM_VIEW_SIM:
                    g_app->ui.panel_open = !g_app->ui.panel_open;
                    g_app->ui.picker_open = 0;
                    if (g_app->ui.panel_open) app_sync_date_fields(g_app);
                    break;
                case IDM_HELP_HELP: g_app->ui.help_open = 1; break;
                case IDM_HELP_LOG: {
                    const char *log = content_dir(CF_LOGS);
                    if (log && log[0]) {
                        char utf8[CONTENT_PATH * 2];
                        snprintf(utf8, sizeof(utf8), "%s\\app.log", log);
                        hook_open_path(g_app, utf8);
                    }
                    break;
                }
                case IDM_HELP_STARTUP: {
                    wchar_t tmp[MAX_PATH];
                    wchar_t path[MAX_PATH];
                    if (GetTempPathW(MAX_PATH, tmp)) {
                        copy_wide(path, MAX_PATH, tmp);
                        const wchar_t *leaf = L"SolarSandbox-startup.log";
                        int n = (int)wcslen(path);
                        for (int i = 0; leaf[i] && n < MAX_PATH - 1; i++) path[n++] = leaf[i];
                        path[n] = 0;
                        ShellExecuteW(hwnd, L"open", path, NULL, NULL, SW_SHOWNORMAL);
                    }
                    break;
                }
                case IDM_HELP_ABOUT: {
                    wchar_t msg[640];
                    format_wide(msg, 640,
                                L"%ls " SSS_VERSION_WSTRING L"\n\n"
                                L"A real-time solar system and gravity sandbox for Windows 7 "
                                L"and later.\n\nPlanetary positions use the JPL approximate "
                                L"ephemerides (valid 1800-2050).\n\nRendering: GDI+   "
                                L"Architecture: %ls",
                                APP_NAME, sizeof(void *) == 8 ? L"64-bit" : L"32-bit", APP_NAME);
                    MessageBoxW(hwnd, msg, APP_NAME, MB_OK | MB_ICONINFORMATION);
                    break;
                }
                default: break;
            }
            return 0;
        }
        case WM_APP_TICK: {
            ULONGLONG now = GetTickCount64();
            double dt = 1.0 / 60.0;
            if (g_last_tick) dt = (double)(now - g_last_tick) / 1000.0;
            g_last_tick = now;
            if (dt <= 0) dt = 1.0 / 60.0;
            if (dt > 0.25) dt = 0.25;
            /* idle frames do not advance the simulation or animations */
            if (now - g_last_input > 500 && g_app->sim.paused) dt = 0;
            app_tick(g_app, dt);
            g_fps_acc += dt;
            g_fps_frames++;
            if (g_fps_acc > 0.4) {
                g_app->fps = (float)(g_fps_frames / g_fps_acc);
                g_fps_acc = 0;
                g_fps_frames = 0;
            }
            if (IsIconic(hwnd)) return 0;
            if (now - g_last_paint < 14 && dt == 0) return 0;
            g_last_paint = now;
            render_now();
            return 0;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_ACTIVATE:
            if (wp == WA_INACTIVE) g_last_input = GetTickCount64();
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, IDT_TICK);
            PostQuitMessage(0);
            return 0;
        default: break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ----------------------------------------------------------------- main */

/* MinGW's Unicode startup expects wWinMain; this alias keeps that contract
 * without depending on the wide startup stub. */
int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmdline, int show) {
    (void)prev;
    (void)cmdline;
    trace("WinMain: start (version %s)", SSS_VERSION_STRING);
    int safe = has_flag(L"--safe") || (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    if (safe)
        trace("safe mode: holding Shift or --safe, so the saved theme and content scan are skipped");
    SetProcessDPIAware();

    GdiplusStartupInput gsi;
    gsi.GdiplusVersion = 1;
    gsi.DebugEventCallback = NULL;
    gsi.SuppressBackgroundThread = FALSE;
    gsi.SuppressExternalCodecs = TRUE;
    if (GdiplusStartup(&g_gdiplus, &gsi, NULL) != Ok) {
        trace("GDI+ failed to start");
        show_error(L"GDI+ could not start on this system.");
        return 1;
    }
    trace("GDI+ started");

    if (!safe) {
        wchar_t path[MAX_PATH];
        if (settings_path(path, MAX_PATH)) sss_theme_pref_load(path);
    }
    trace("theme preference loaded (index %d)", sss_theme_index());

    g_app = (App *)sss_alloc(sizeof(App));
    if (!g_app) {
        trace("out of memory allocating the application state");
        show_error(L"Not enough memory to start Solar System Sandbox.");
        return 1;
    }
    trace("application state allocated (%u bytes)", (unsigned)sizeof(App));
    app_init(g_app, 1280, 800);
    trace("app_init done (%d bodies)", g_app->sim.count);
    g_app->on_save = hook_file_save_menu;
    g_app->on_load = hook_load;
    g_app->on_screenshot = hook_screenshot;
    g_app->on_quit = hook_quit;
    g_app->on_minimize = hook_minimize;
    g_app->on_maximize = hook_maximize;
    g_app->on_theme = hook_theme;
    g_app->read_text = hook_read_text;
    g_app->open_path = hook_open_path;

    /* Windows 7 netbooks and 1 GB VMs get the lighter render path and a 30 fps
     * timer; the simulation itself is single-threaded either way. */
    unsigned cpu_count = 0, ram_mb = 0;
    {
        SYSTEM_INFO si;
        MEMORYSTATUSEX mem;
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&mem, sizeof(mem));
        mem.dwLength = sizeof(mem);
        GetSystemInfo(&si);
        GlobalMemoryStatusEx(&mem);
        cpu_count = (unsigned)si.dwNumberOfProcessors;
        ram_mb = (unsigned)(mem.ullTotalPhys >> 20);
        if (si.dwNumberOfProcessors <= 1 || ram_mb < 1536) g_app->ui.low_power = 1;
        trace("system: %u CPUs, %u MB RAM%s", cpu_count, ram_mb,
              g_app->ui.low_power ? ", low-power render" : "");
    }

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(IDI_APP));
    if (!wc.hIcon) wc.hIcon = LoadIconW(NULL, (LPCWSTR)IDI_APPLICATION);
    wc.hIconSm = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON,
                                   GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON),
                                   LR_DEFAULTCOLOR);
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = WINDOW_CLASS;
    if (!RegisterClassExW(&wc)) {
        trace("RegisterClassEx failed (%lu)", (unsigned long)GetLastError());
        show_error(L"Could not register the window class.");
        return 1;
    }
    trace("window class registered");

    build_menu();

    RECT rc = {0, 0, 1280, 800};
    AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, TRUE, 0);
    int w = rc.right - rc.left, h = rc.bottom - rc.top;
    int screen_w = GetSystemMetrics(SM_CXSCREEN), screen_h = GetSystemMetrics(SM_CYSCREEN);
    if (w > screen_w - 40) w = screen_w - 40;
    if (h > screen_h - 80) h = screen_h - 80;
    int x = (screen_w - w) / 2, y = (screen_h - h) / 2;

    HWND hwnd = CreateWindowExW(0, WINDOW_CLASS, APP_NAME, WS_OVERLAPPEDWINDOW, x, y, w, h, NULL,
                                g_menu, instance, NULL);
    if (!hwnd) {
        trace("CreateWindowEx failed (%lu)", (unsigned long)GetLastError());
        show_error(L"Could not create the application window.");
        return 1;
    }
    trace("window created (%dx%d at %d,%d)", w, h, x, y);
    g_hwnd = hwnd;
    update_dpi();

    RECT client;
    GetClientRect(hwnd, &client);
    app_resize(g_app, client.right - client.left, client.bottom - client.top);
    recreate_backbuffer(g_app->width, g_app->height);
    trace("back buffer %dx%d %s", g_app->width, g_app->height, g_surface ? "ready" : "FAILED");
    if (!g_surface) {
        show_error(L"Could not create the drawing surface. Close some programs and try again.");
        DestroyWindow(hwnd);
        return 1;
    }

    ShowWindow(hwnd, show == SW_SHOWDEFAULT ? SW_SHOWNORMAL : show);
    UpdateWindow(hwnd);
    trace("window shown");

    g_last_tick = GetTickCount64();
    QueryPerformanceFrequency((LARGE_INTEGER *)&g_freq);
    SetTimer(hwnd, IDT_TICK, g_app->ui.low_power ? 33 : 16, NULL);

    render_now();
    trace("first frame drawn");

    if (!safe) {
        /* Scanning the content folders happens after the window is on screen:
         * a slow or unreachable folder can no longer hide the whole app. */
        wchar_t app_w[MAX_PATH], user_w[MAX_PATH];
        char app_utf8[CONTENT_PATH * 2], user_utf8[CONTENT_PATH * 2];
        app_w[0] = user_w[0] = 0;
        exe_dir(app_w, MAX_PATH);
        appdata_dir(user_w, MAX_PATH);
        wide_to_utf8(app_utf8, sizeof(app_utf8), app_w);
        wide_to_utf8(user_utf8, sizeof(user_utf8), user_w);
        trace("content roots: app '%s', user '%s'", app_utf8, user_utf8);
        ULONGLONG scan_start = GetTickCount64();
        content_init(app_utf8, user_utf8);
        trace("content scanned in %lu ms: %d parts, %d packs, %d mods",
              (unsigned long)(GetTickCount64() - scan_start), content_part_count(),
              content_pack_count(), content_mod_count());
        content_log("system: %u CPU, %u MB RAM%s", cpu_count, ram_mb,
                    g_app->ui.low_power ? ", low-power render" : "");
        content_log("Solar System Sandbox %s started (app '%s', user '%s')", SSS_VERSION_STRING,
                    app_utf8, user_utf8);
        InvalidateRect(hwnd, NULL, FALSE);
    }

    trace("entering the message loop");
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    gfx_flush_fonts();
    if (g_gfx) gfx_destroy(g_gfx);
    if (g_surface) surf_destroy(g_surface);
    content_log("shutting down");
    content_shutdown();
    app_shutdown(g_app);
    sss_free(g_app);
    g_app = NULL;
    GdiplusShutdown(g_gdiplus);
    return 0;
}
