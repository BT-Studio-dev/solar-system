/* setup_ui.c -- the setup window: a small Windows 7 style wizard.
 *
 * The header is painted once into a bitmap (blue Aero-style gradient with a
 * glossy band, product icon and title); the body uses ordinary static, edit,
 * checkbox and button controls, so the installer picks up the native Windows 7
 * visual style from the common-controls v6 manifest.
 */
#include "setup.h"
#include "theme.h"

#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include <shellapi.h>
#include <string.h>

#define WIN_W 640
#define WIN_H 448
#define HEADER_H 96
#define FOOTER_Y 384

#define IDC_HEADING 1001
#define IDC_BODY 1002
#define IDC_PATH_LABEL 1003
#define IDC_PATH 1004
#define IDC_BROWSE 1005
#define IDC_DESKTOP 1006
#define IDC_LAUNCH 1007
#define IDC_PROGRESS 1008
#define IDC_STATUS 1009
#define IDC_DETAIL 1010
#define IDC_PRIMARY 1011
#define IDC_SECONDARY 1012
#define IDC_NOTE 1013
#define IDC_THEME 1014
#define IDC_THEME_LABEL 1015

enum {
    PAGE_WELCOME = 0,
    PAGE_PROGRESS = 1,
    PAGE_DONE = 2,
    PAGE_REMOVE = 3,
    PAGE_FAILED = 4
};

extern const unsigned char sss_payload[];
extern const unsigned int sss_payload_size;
extern const unsigned int sss_payload_crc;

static HINSTANCE g_inst;
static HWND g_hwnd;
static HWND g_heading, g_body, g_path_label, g_path, g_browse, g_desktop, g_launch, g_progress,
    g_status, g_detail, g_note, g_theme_label, g_theme, g_primary, g_secondary;
static HFONT g_font_ui, g_font_bold, g_font_small, g_font_head, g_font_sub;
static HBRUSH g_white, g_footer;
static HBITMAP g_header;
static HDC g_header_dc;
static int g_page;
static int g_busy;
static int g_uninstall;
static int g_theme_index;
static wchar_t g_settings[MAX_PATH];
static Payload g_payload;
static wchar_t g_error[512];

static void wcopy_local(wchar_t *dst, int cap, const wchar_t *src) {
    int i = 0;
    if (cap <= 0) return;
    while (src && src[i] && i < cap - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void wcat_local(wchar_t *dst, int cap, const wchar_t *src) {
    int n = (int)wcslen(dst);
    if (cap <= 0) return;
    while (src && *src && n < cap - 1) dst[n++] = *src++;
    dst[n] = 0;
}

static void ascii_w(const char *src, wchar_t *dst, int cap) {
    int i = 0;
    if (cap <= 0) return;
    while (src && src[i] && i < cap - 1) {
        dst[i] = (wchar_t)(unsigned char)src[i];
        i++;
    }
    dst[i] = 0;
}

/* ------------------------------------------------------------- painting */

static COLORREF mix(COLORREF a, COLORREF b, int t) {
    int r = GetRValue(a) + ((GetRValue(b) - GetRValue(a)) * t) / 255;
    int g = GetGValue(a) + ((GetGValue(b) - GetGValue(a)) * t) / 255;
    int bl = GetBValue(a) + ((GetBValue(b) - GetBValue(a)) * t) / 255;
    return RGB(r, g, bl);
}

static void make_brushes(void);

static COLORREF color_of(SsColor c) {
    return RGB(COL_R(c), COL_G(c), COL_B(c));
}

static void paint_header(HDC dc) {
    const SsTheme *th = sss_theme();
    COLORREF top = color_of(col_blend(th->titlebar_top, th->accent_deep, 0.55f));
    COLORREF bottom = color_of(col_blend(th->accent, th->accent_deep, 0.25f));
    COLORREF text = color_of(th->title_rgb);
    COLORREF shine = color_of(th->accent_hi);
    for (int y = 0; y < HEADER_H; y++) {
        int t = (y * 255) / (HEADER_H - 1);
        COLORREF c = mix(top, bottom, t);
        /* Aero gloss: a soft white sheen across the upper third. */
        if (y < 46) {
            int gloss = (46 - y) * 34 / 46;
            c = mix(c, RGB(255, 255, 255), gloss);
        }
        if (y == 0) c = mix(c, shine, 40);
        if (y >= HEADER_H - 2) c = mix(c, RGB(8, 26, 48), 70);
        RECT r = {0, y, WIN_W, y + 1};
        HBRUSH brush = CreateSolidBrush(c);
        FillRect(dc, &r, brush);
        DeleteObject(brush);
    }

    /* Thin highlight along the very top of the band. */
    RECT highlight = {0, 0, WIN_W, 1};
    HBRUSH sb = CreateSolidBrush(shine);
    FillRect(dc, &highlight, sb);
    DeleteObject(sb);

    HICON icon = (HICON)LoadImageW(g_inst, MAKEINTRESOURCEW(1), IMAGE_ICON, 56, 56,
                                   LR_DEFAULTCOLOR);
    if (icon) {
        DrawIconEx(dc, 24, 20, icon, 56, 56, 0, NULL, DI_NORMAL);
        DestroyIcon(icon);
    }

    wchar_t product[96];
    ascii_w(g_payload.product, product, 96);

    SetBkMode(dc, TRANSPARENT);
    HFONT old = (HFONT)SelectObject(dc, g_font_head);
    SetTextColor(dc, text);
    RECT tr = {96, 16, WIN_W - 24, 52};
    DrawTextW(dc, product, -1, &tr, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    wchar_t sub[160];
    wcopy_local(sub, 160, L"Version ");
    wchar_t ver[32];
    ascii_w(g_payload.version, ver, 32);
    wcat_local(sub, 160, ver);
    wcat_local(sub, 160, L"   |   Setup");
    SelectObject(dc, g_font_sub);
    SetTextColor(dc, color_of(th->title_sub));
    RECT sr = {98, 52, WIN_W - 24, 78};
    DrawTextW(dc, sub, -1, &sr, DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    SelectObject(dc, old);
}

static void build_header_bitmap(HDC screen) {
    g_header_dc = CreateCompatibleDC(screen);
    g_header = CreateCompatibleBitmap(screen, WIN_W, HEADER_H);
    SelectObject(g_header_dc, g_header);
    paint_header(g_header_dc);
}

/* ------------------------------------------------------------- controls */

static HWND make_control(const wchar_t *cls, const wchar_t *text, DWORD style, int x, int y, int w,
                         int h, int id) {
    HWND hwnd = CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style, x, y, w, h, g_hwnd,
                                (HMENU)(INT_PTR)id, g_inst, NULL);
    if (hwnd) SendMessageW(hwnd, WM_SETFONT, (WPARAM)g_font_ui, TRUE);
    return hwnd;
}

static void make_controls(void) {
    g_heading = make_control(L"STATIC", L"", SS_LEFT, 28, 110, WIN_W - 56, 26, IDC_HEADING);
    SendMessageW(g_heading, WM_SETFONT, (WPARAM)g_font_bold, TRUE);

    g_body = make_control(L"STATIC", L"", SS_LEFT, 28, 142, WIN_W - 56, 58, IDC_BODY);

    g_path_label = make_control(L"STATIC", L"Install location", SS_LEFT, 28, 214, 300, 18,
                                IDC_PATH_LABEL);
    g_path = make_control(L"EDIT", L"", ES_LEFT | ES_AUTOHSCROLL | WS_TABSTOP, 28, 234, 420, 26,
                          IDC_PATH);
    g_browse = make_control(L"BUTTON", L"Browse...", BS_PUSHBUTTON | WS_TABSTOP, 456, 233, 156, 28,
                            IDC_BROWSE);
    g_desktop = make_control(L"BUTTON", L"Create a desktop shortcut", BS_AUTOCHECKBOX | WS_TABSTOP,
                             28, 272, 320, 22, IDC_DESKTOP);
    g_launch = make_control(L"BUTTON", L"Launch Solar System Sandbox", BS_AUTOCHECKBOX | WS_TABSTOP,
                            28, 258, 320, 22, IDC_LAUNCH);

    g_theme_label = make_control(L"STATIC", L"Colour theme", SS_LEFT, 28, 306, 96, 22,
                                 IDC_THEME_LABEL);
    g_theme = make_control(L"BUTTON", L"", BS_PUSHBUTTON | WS_TABSTOP, 132, 302, 176, 28,
                           IDC_THEME);

    g_note = make_control(L"STATIC",
                          L"Setup needs administrator rights and installs the app for all users "
                          L"on this computer.",
                          SS_LEFT, 28, 342, WIN_W - 56, 18, IDC_NOTE);
    SendMessageW(g_note, WM_SETFONT, (WPARAM)g_font_small, TRUE);

    g_status = make_control(L"STATIC", L"", SS_LEFT, 28, 150, WIN_W - 56, 20, IDC_STATUS);
    g_detail = make_control(L"STATIC", L"", SS_LEFT, 28, 212, WIN_W - 56, 60, IDC_DETAIL);
    g_progress = CreateWindowExW(0, PROGRESS_CLASSW, L"", WS_CHILD | PBS_SMOOTH, 28, 180,
                                 WIN_W - 56, 20, g_hwnd, (HMENU)(INT_PTR)IDC_PROGRESS, g_inst,
                                 NULL);

    g_primary = make_control(L"BUTTON", L"Install", BS_DEFPUSHBUTTON | WS_TABSTOP, WIN_W - 128, 400,
                             100, 30, IDC_PRIMARY);
    g_secondary = make_control(L"BUTTON", L"Cancel", BS_PUSHBUTTON | WS_TABSTOP, WIN_W - 236, 400,
                               100, 30, IDC_SECONDARY);
}

static void show(HWND hwnd, int visible) {
    if (hwnd) ShowWindow(hwnd, visible ? SW_SHOW : SW_HIDE);
}

static void check(HWND hwnd, int on) {
    if (hwnd) SendMessageW(hwnd, BM_SETCHECK, on ? BST_CHECKED : BST_UNCHECKED, 0);
}

static int checked(HWND hwnd) {
    return hwnd && SendMessageW(hwnd, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

static void set_text(HWND hwnd, const wchar_t *text) {
    if (hwnd) SetWindowTextW(hwnd, text);
}

static void apply_page(void) {
    wchar_t product[96], heading[192], body[768];
    ascii_w(g_payload.product, product, 96);
    int welcome = g_page == PAGE_WELCOME;
    int busy = g_page == PAGE_PROGRESS;
    int done = g_page == PAGE_DONE;
    int remove = g_page == PAGE_REMOVE;
    int failed = g_page == PAGE_FAILED;

    show(g_heading, !busy);
    show(g_body, !busy);
    show(g_path_label, welcome);
    show(g_path, welcome);
    show(g_browse, welcome);
    show(g_desktop, welcome);
    show(g_launch, done && !g_uninstall);
    show(g_note, welcome);
    show(g_theme_label, welcome);
    show(g_theme, welcome);
    if (welcome) {
        wchar_t label[64];
        ascii_w(sss_theme()->name, label, 64);
        set_text(g_theme, label);
    }
    show(g_status, busy);
    show(g_detail, busy);
    show(g_progress, busy);
    show(g_primary, 1);
    show(g_secondary, !busy && !done && !failed);

    if (welcome) {
        wcopy_local(heading, 192, L"Welcome to ");
        wcat_local(heading, 192, product);
        wcat_local(heading, 192, L" Setup");
        wcopy_local(body, 768, L"This wizard installs ");
        wcat_local(body, 768, product);
        wcat_local(body, 768, L" ");
        wchar_t ver[32];
        ascii_w(g_payload.version, ver, 32);
        wcat_local(body, 768, ver);
        wcat_local(body, 768,
                   L" on your computer.\n\nChoose an install location and click Install. "
                   L"Administrator rights are required.");
        set_text(g_primary, L"Install");
        set_text(g_secondary, L"Cancel");
    } else if (busy) {
        set_text(g_primary, L"Installing...");
    } else if (done) {
        if (g_uninstall) {
            wcopy_local(heading, 192, L"Removal complete");
            wcopy_local(body, 768, L"");
            wcat_local(body, 768, product);
            wcat_local(body, 768, L" has been removed from this computer.");
        } else {
            wcopy_local(heading, 192, L"Setup complete");
            wcopy_local(body, 768, L"");
            wcat_local(body, 768, product);
            wcat_local(body, 768, L" is installed.\n\nYou can start it from the Start menu, "
                                   L"under All Programs > ");
            wcat_local(body, 768, product);
            wcat_local(body, 768, L".\n\nOpen the Library tab (F4) to try the sample parts, "
                                   L"texture packs and mod.");
        }
        set_text(g_primary, g_uninstall ? L"Close" : L"Finish");
    } else if (remove) {
        wcopy_local(heading, 192, L"Remove ");
        wcat_local(heading, 192, product);
        wcat_local(heading, 192, L"?");
        wcopy_local(body, 768, L"Setup will remove ");
        wcat_local(body, 768, product);
        wcat_local(body, 768, L" and its shortcuts from this computer. Your saved simulations "
                               L"stay where you put them.\n\nClick Remove to continue.");
        set_text(g_primary, L"Remove");
        set_text(g_secondary, L"Cancel");
    } else if (failed) {
        wcopy_local(heading, 192, L"Setup could not finish");
        wcopy_local(body, 768, g_error);
        set_text(g_primary, L"Close");
    }

    set_text(g_heading, heading);
    set_text(g_body, body);
    EnableWindow(g_primary, !busy);
    EnableWindow(g_secondary, !busy);
}

/* The body static has to be tall enough for every paragraph on the page;
 * a fixed height used to clip the second welcome paragraph in half.  The
 * launch checkbox follows the body so the two never collide. */
static void layout_page(void) {
    int body_h;
    switch (g_page) {
        case PAGE_DONE:
            body_h = 104;
            break;
        case PAGE_REMOVE:
            body_h = 88;
            break;
        case PAGE_FAILED: {
            int lines = ((int)wcslen(g_error) + 88) / 89;
            body_h = 24 + lines * 18;
            if (body_h < 72) body_h = 72;
            if (body_h > 200) body_h = 200;
            break;
        }
        default:
            body_h = 58;
            break;
    }
    MoveWindow(g_body, 28, 142, WIN_W - 56, body_h, TRUE);
    if (g_page == PAGE_DONE)
        MoveWindow(g_launch, 28, 142 + body_h + 12, 320, 22, TRUE);
}

static void set_page(int page) {
    g_page = page;
    apply_page();
    layout_page();
    InvalidateRect(g_hwnd, NULL, FALSE);
}

/* ------------------------------------------------------------- install */

static void pump(void) {
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
}

static void progress_cb(void *ctx, int percent, const wchar_t *status) {
    (void)ctx;
    SendMessageW(g_progress, PBM_SETPOS, (WPARAM)(percent < 0 ? 0 : percent), 0);
    if (status) set_text(g_status, status);
    pump();
}

static void launch_app(void) {
    wchar_t dir[SETUP_MAX_PATH];
    GetWindowTextW(g_path, dir, SETUP_MAX_PATH);
    wchar_t exe[SETUP_MAX_PATH + 32];
    wcopy_local(exe, SETUP_MAX_PATH + 32, L"\"");
    wcat_local(exe, SETUP_MAX_PATH + 32, dir);
    wcat_local(exe, SETUP_MAX_PATH + 32, L"\\SolarSandbox.exe\"");

    /* Setup runs elevated; ask the shell to start the app at the user's own
     * integrity level instead of inheriting our administrator token. */
    wchar_t explorer[MAX_PATH];
    if (GetWindowsDirectoryW(explorer, MAX_PATH)) {
        wcat_local(explorer, MAX_PATH, L"\\explorer.exe");
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;
        ZeroMemory(&si, sizeof(si));
        ZeroMemory(&pi, sizeof(pi));
        si.cb = sizeof(si);
        if (CreateProcessW(explorer, exe, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return;
        }
    }
    ShellExecuteW(NULL, L"open", exe, NULL, dir, SW_SHOWNORMAL);
}

static void start_install(void) {
    wchar_t dir[SETUP_MAX_PATH];
    GetWindowTextW(g_path, dir, SETUP_MAX_PATH);
    if (!dir[0]) {
        MessageBoxW(g_hwnd, L"Choose a folder to install into.", L"Solar System Sandbox Setup",
                    MB_OK | MB_ICONWARNING);
        return;
    }
    g_busy = 1;
    set_page(PAGE_PROGRESS);
    SendMessageW(g_progress, PBM_SETPOS, 0, 0);
    set_text(g_detail, L"This can take a few seconds.");
    int ok = setup_install(&g_payload, dir, checked(g_desktop), progress_cb, NULL, g_error, 512);
    g_busy = 0;
    if (ok) {
        check(g_launch, 1);
        set_page(PAGE_DONE);
    } else {
        set_page(PAGE_FAILED);
    }
}

static void start_uninstall(void) {
    wchar_t dir[SETUP_MAX_PATH];
    if (!setup_read_install_dir(dir, SETUP_MAX_PATH) && !setup_self_dir(dir, SETUP_MAX_PATH))
        setup_default_dir(dir, SETUP_MAX_PATH);
    g_busy = 1;
    set_page(PAGE_PROGRESS);
    set_text(g_detail, L"Removing shortcuts and files.");
    int ok = setup_uninstall(&g_payload, dir, progress_cb, NULL, g_error, 512);
    g_busy = 0;
    if (ok) {
        g_uninstall = 1;
        set_page(PAGE_DONE);
    } else {
        set_page(PAGE_FAILED);
    }
}

/* ------------------------------------------------------------- events */

static void browse_folder(void) {
    BROWSEINFOW bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.hwndOwner = g_hwnd;
    bi.lpszTitle = L"Select the folder where Solar System Sandbox should be installed:";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    LPITEMIDLIST pidl = SHBrowseForFolderW(&bi);
    if (!pidl) return;
    wchar_t path[SETUP_MAX_PATH];
    if (SHGetPathFromIDListW(pidl, path)) SetWindowTextW(g_path, path);
    CoTaskMemFree(pidl);
}

static BOOL CALLBACK repaint_child(HWND child, LPARAM unused) {
    (void)unused;
    InvalidateRect(child, NULL, TRUE);
    return TRUE;
}

static void repaint_all(void) {
    InvalidateRect(g_hwnd, NULL, TRUE);
    EnumChildWindows(g_hwnd, repaint_child, 0);
}

static void apply_theme(int index) {
    sss_theme_set(index);
    g_theme_index = index;
    make_brushes();
    if (g_header_dc) {
        DeleteDC(g_header_dc);
        g_header_dc = NULL;
    }
    if (g_header) {
        DeleteObject(g_header);
        g_header = NULL;
    }
    HDC screen = GetDC(NULL);
    build_header_bitmap(screen);
    ReleaseDC(NULL, screen);
    apply_page();
    repaint_all();
}

static void choose_theme(void) {
    int count = sss_theme_count();
    HMENU menu = CreatePopupMenu();
    if (!menu) return;
    for (int i = 0; i < count; i++) {
        const SsTheme *theme = sss_theme_at(i);
        wchar_t name[64];
        ascii_w(theme->name, name, 64);
        AppendMenuW(menu, MF_STRING | (i == sss_theme_index() ? MF_CHECKED : 0), 4000 + i, name);
    }
    RECT r;
    GetWindowRect(g_theme, &r);
    SetForegroundWindow(g_hwnd);
    int cmd = (int)TrackPopupMenu(
        menu, TPM_RETURNCMD | TPM_LEFTBUTTON | TPM_TOPALIGN | TPM_LEFTALIGN, r.left, r.bottom, 0,
        g_hwnd, NULL);
    DestroyMenu(menu);
    if (cmd >= 4000 && cmd < 4000 + count) apply_theme(cmd - 4000);
}

static void on_command(int id) {
    switch (id) {
        case IDC_BROWSE:
            if (!g_busy) browse_folder();
            break;
        case IDC_THEME:
            if (!g_busy) choose_theme();
            break;
        case IDC_PRIMARY:
            if (g_busy) break;
            if (g_page == PAGE_WELCOME) start_install();
            else if (g_page == PAGE_REMOVE) start_uninstall();
            else if (g_page == PAGE_DONE) {
                if (!g_uninstall && checked(g_launch)) launch_app();
                DestroyWindow(g_hwnd);
            } else if (g_page == PAGE_FAILED) DestroyWindow(g_hwnd);
            break;
        case IDC_SECONDARY:
            if (!g_busy) DestroyWindow(g_hwnd);
            break;
        default:
            break;
    }
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE:
            g_hwnd = hwnd;
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd, &ps);
            if (g_header_dc) BitBlt(dc, 0, 0, WIN_W, HEADER_H, g_header_dc, 0, 0, SRCCOPY);
            RECT body = {0, HEADER_H, WIN_W, FOOTER_Y};
            FillRect(dc, &body, g_white);
            RECT footer = {0, FOOTER_Y, WIN_W, WIN_H};
            FillRect(dc, &footer, g_footer);
            RECT top = {0, FOOTER_Y, WIN_W, FOOTER_Y + 1};
            HBRUSH line = CreateSolidBrush(RGB(206, 206, 206));
            FillRect(dc, &top, line);
            DeleteObject(line);
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_CTLCOLORSTATIC:
        case WM_CTLCOLORBTN: {
            HDC dc = (HDC)wp;
            if ((HWND)lp == g_note) SetTextColor(dc, color_of(sss_theme()->text_muted));
            else SetTextColor(dc, color_of(sss_theme()->text));
            SetBkColor(dc, color_of(sss_theme()->panel_bottom));
            return (LRESULT)g_white;
        }
        case WM_COMMAND:
            on_command(LOWORD(wp));
            return 0;
        case WM_CLOSE:
            if (g_busy) return 0;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            break;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

/* ------------------------------------------------------------- startup */

static void make_brushes(void) {
    const SsTheme *th = sss_theme();
    if (g_white) DeleteObject(g_white);
    if (g_footer) DeleteObject(g_footer);
    g_white = CreateSolidBrush(color_of(th->panel_bottom));
    g_footer = CreateSolidBrush(color_of(col_blend(th->window_bottom, th->panel_bottom, 0.5f)));
}

static HFONT make_font(int px, int weight) {
    return CreateFontW(-px, 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                       OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                       DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
}

static int is_uninstall_request(void) {
    const wchar_t *cmd = GetCommandLineW();
    if (!cmd) return 0;
    for (const wchar_t *p = cmd; *p; p++) {
        if (*p != L'/' && *p != L'-') continue;
        const wchar_t *q = p + 1;
        const wchar_t *word = L"uninstall";
        int i = 0;
        while (word[i] && q[i]) {
            wchar_t c = q[i];
            if (c >= L'A' && c <= L'Z') c = (wchar_t)(c + 32);
            if (c != word[i]) break;
            i++;
        }
        if (!word[i]) return 1;
    }
    return 0;
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmdline, int show) {
    (void)prev;
    (void)cmdline;
    g_inst = instance;

    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    if (!payload_open(&g_payload, sss_payload, sss_payload_size)) {
        MessageBoxW(NULL, L"The setup payload is damaged. Download setup again.",
                    L"Solar System Sandbox Setup", MB_OK | MB_ICONERROR);
        return 1;
    }
    if (sss_crc32(sss_payload, sss_payload_size) != sss_payload_crc) {
        MessageBoxW(NULL, L"The setup payload failed its integrity check. Download setup again.",
                    L"Solar System Sandbox Setup", MB_OK | MB_ICONERROR);
        return 1;
    }

    INITCOMMONCONTROLSEX icc;
    icc.dwSize = sizeof(icc);
    icc.dwICC = ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&icc);

    {
        wchar_t appdata[MAX_PATH];
        if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appdata) == S_OK) {
            wcopy_local(g_settings, MAX_PATH, appdata);
            wcat_local(g_settings, MAX_PATH, L"\\Solar System Sandbox");
            CreateDirectoryW(g_settings, NULL);
            wcat_local(g_settings, MAX_PATH, L"\\settings.ini");
            sss_theme_pref_load(g_settings);
            g_theme_index = sss_theme_index();
        }
    }

    g_font_ui = make_font(13, FW_NORMAL);
    g_font_bold = make_font(17, FW_SEMIBOLD);
    g_font_small = make_font(12, FW_NORMAL);
    g_font_head = make_font(23, FW_SEMIBOLD);
    g_font_sub = make_font(13, FW_NORMAL);
    make_brushes();

    WNDCLASSEXW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = instance;
    wc.hIcon = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    wc.hIconSm = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, 16, 16,
                                   LR_DEFAULTCOLOR);
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.hbrBackground = NULL;
    wc.lpszClassName = L"SolarSandboxSetupWizard";
    if (!RegisterClassExW(&wc)) return 1;

    RECT rc = {0, 0, WIN_W, WIN_H};
    AdjustWindowRectEx(&rc, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU, FALSE, 0);
    int ww = rc.right - rc.left, wh = rc.bottom - rc.top;
    int x = (GetSystemMetrics(SM_CXSCREEN) - ww) / 2;
    int y = (GetSystemMetrics(SM_CYSCREEN) - wh) / 2;
    if (y < 0) y = 0;

    wchar_t title[128];
    ascii_w(g_payload.product, title, 96);
    wcat_local(title, 128, L" Setup");

    g_hwnd = CreateWindowExW(WS_EX_APPWINDOW, L"SolarSandboxSetupWizard", title,
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_CLIPCHILDREN, x, y, ww,
                             wh, NULL, NULL, instance, NULL);
    if (!g_hwnd) return 1;

    HDC screen = GetDC(NULL);
    build_header_bitmap(screen);
    ReleaseDC(NULL, screen);

    make_controls();

    wchar_t dir[SETUP_MAX_PATH];
    setup_default_dir(dir, SETUP_MAX_PATH);
    SetWindowTextW(g_path, dir);
    check(g_desktop, 1);

    g_uninstall = is_uninstall_request();
    if (g_uninstall) {
        set_page(PAGE_REMOVE);
    } else {
        set_page(PAGE_WELCOME);
    }

    ShowWindow(g_hwnd, show == SW_SHOWDEFAULT ? SW_SHOWNORMAL : show);
    UpdateWindow(g_hwnd);
    SetFocus(g_primary);

    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        if (!IsDialogMessageW(g_hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (g_header_dc) DeleteDC(g_header_dc);
    if (g_header) DeleteObject(g_header);
    DeleteObject(g_font_ui);
    DeleteObject(g_font_bold);
    DeleteObject(g_font_small);
    DeleteObject(g_font_head);
    DeleteObject(g_font_sub);
    DeleteObject(g_white);
    DeleteObject(g_footer);
    if (g_settings[0]) sss_theme_pref_save(g_settings, sss_theme_index());
    CoUninitialize();
    return 0;
}

void WinMainCRTStartup(void);
void WinMainCRTStartup(void) {
    int result = WinMain(GetModuleHandleW(NULL), NULL, NULL, SW_SHOWDEFAULT);
    ExitProcess((UINT)result);
}
