/* install.c -- the platform layer of the Solar System Sandbox setup program.
 *
 * Everything here is plain Win32 available on Windows 7: files, registry and
 * shell shortcuts (IShellLink).  No COM GUID library is linked; the three
 * identifiers we need are defined locally to keep the import surface small.
 */
#include "setup.h"

#include <windows.h>
#include <shlobj.h>
#include <string.h>

/* {00021401-0000-0000-C000-000000000046} */
static const GUID sss_CLSID_ShellLink = {
    0x00021401, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
/* {000214F9-0000-0000-C000-000000000046} */
static const GUID sss_IID_IShellLinkW = {
    0x000214F9, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
/* {0000010B-0000-0000-C000-000000000046} */
static const GUID sss_IID_IPersistFile = {
    0x0000010B, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

#define UNINSTALL_KEY L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Solar System Sandbox"

/* ------------------------------------------------------------- strings */

static void wcopy(wchar_t *dst, int cap, const wchar_t *src) {
    int i = 0;
    if (cap <= 0) return;
    while (src && src[i] && i < cap - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;
}

static void wcat(wchar_t *dst, int cap, const wchar_t *src) {
    int n = (int)wcslen(dst);
    int i = 0;
    if (cap <= 0) return;
    while (src && src[i] && n < cap - 1) dst[n++] = src[i++];
    dst[n] = 0;
}

static int wesame(const wchar_t *a, const wchar_t *b) {
    for (;;) {
        wchar_t ca = *a++, cb = *b++;
        if (ca >= L'A' && ca <= L'Z') ca = (wchar_t)(ca + 32);
        if (cb >= L'A' && cb <= L'Z') cb = (wchar_t)(cb + 32);
        if (ca != cb) return 0;
        if (!ca) return 1;
    }
}

static void ascii_to_w(const char *src, wchar_t *dst, int cap) {
    int i = 0;
    if (cap <= 0) return;
    while (src && src[i] && i < cap - 1) {
        dst[i] = (wchar_t)(unsigned char)src[i];
        i++;
    }
    dst[i] = 0;
}

static void wuint(wchar_t *dst, int cap, unsigned int value) {
    wchar_t tmp[16];
    int n = 0;
    if (cap <= 0) return;
    if (value == 0) tmp[n++] = L'0';
    while (value > 0) {
        tmp[n++] = (wchar_t)(L'0' + (value % 10u));
        value /= 10u;
    }
    int i = 0;
    while (n > 0 && i < cap - 1) dst[i++] = tmp[--n];
    dst[i] = 0;
}

/* Append a relative UTF-8 payload name ("sub/file.ext") to dir. */
static void join_entry(const wchar_t *dir, const PayloadEntry *e, wchar_t *out, int cap) {
    wchar_t rel[SETUP_MAX_PATH];
    payload_entry_name(e, rel, SETUP_MAX_PATH);
    wcopy(out, cap, dir);
    size_t n = wcslen(out);
    if (n && out[n - 1] != L'\\') wcat(out, cap, L"\\");
    for (int i = 0; rel[i] && (int)wcslen(out) < cap - 1; i++)
        wcat(out, cap, rel[i] == L'/' ? L"\\" : (wchar_t[2]){rel[i], 0});
}

/* ------------------------------------------------------------- files */

static int ensure_dir(wchar_t *path) {
    wchar_t tmp[SETUP_MAX_PATH];
    wcopy(tmp, SETUP_MAX_PATH, path);
    for (int i = 3; tmp[i]; i++) {
        if (tmp[i] == L'\\') {
            tmp[i] = 0;
            CreateDirectoryW(tmp, NULL);
            tmp[i] = L'\\';
        }
    }
    if (CreateDirectoryW(tmp, NULL)) return 1;
    DWORD err = GetLastError();
    return err == ERROR_ALREADY_EXISTS;
}

/* Create every folder leading to path (payload entries may be nested, e.g.
 * "Texture Pack/Warm Dusk/pack.ini"). */
static void ensure_parent_dirs(const wchar_t *path) {
    wchar_t tmp[SETUP_MAX_PATH];
    wcopy(tmp, SETUP_MAX_PATH, path);
    for (int i = (int)wcslen(tmp) - 1; i > 2; i--) {
        if (tmp[i] == L'\\' || tmp[i] == L'/') {
            tmp[i] = 0;
            ensure_dir(tmp);
            return;
        }
    }
}

static int write_file(const wchar_t *path, const unsigned char *data, unsigned int size) {
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL,
                           NULL);
    if (h == INVALID_HANDLE_VALUE) return 0;
    unsigned int done = 0;
    int ok = 1;
    while (done < size) {
        DWORD chunk = size - done > 65536u ? 65536u : (DWORD)(size - done);
        DWORD wrote = 0;
        if (!WriteFile(h, data + done, chunk, &wrote, NULL) || wrote != chunk) {
            ok = 0;
            break;
        }
        done += wrote;
    }
    CloseHandle(h);
    if (!ok) DeleteFileW(path);
    return ok;
}

static int copy_self_to(const wchar_t *dest) {
    wchar_t src[SETUP_MAX_PATH];
    if (!GetModuleFileNameW(NULL, src, SETUP_MAX_PATH)) return 0;
    if (wesame(src, dest)) return 1;
    return CopyFileW(src, dest, FALSE) ? 1 : 0;
}

/* ------------------------------------------------------------- shortcuts */

static int make_shortcut(const wchar_t *lnk, const wchar_t *target, const wchar_t *workdir,
                         const wchar_t *desc, const wchar_t *args) {
    IShellLinkW *link = NULL;
    HRESULT hr = CoCreateInstance(&sss_CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                  &sss_IID_IShellLinkW, (void **)&link);
    if (FAILED(hr) || !link) return 0;
    link->lpVtbl->SetPath(link, target);
    if (workdir) link->lpVtbl->SetWorkingDirectory(link, workdir);
    if (desc) link->lpVtbl->SetDescription(link, desc);
    if (args) link->lpVtbl->SetArguments(link, args);
    link->lpVtbl->SetIconLocation(link, target, 0);

    IPersistFile *file = NULL;
    hr = link->lpVtbl->QueryInterface(link, &sss_IID_IPersistFile, (void **)&file);
    int ok = 0;
    if (SUCCEEDED(hr) && file) {
        ok = SUCCEEDED(file->lpVtbl->Save(file, lnk, TRUE));
        file->lpVtbl->Release(file);
    }
    link->lpVtbl->Release(link);
    return ok;
}

static void start_menu_dir(const wchar_t *product, wchar_t *out, int cap) {
    wchar_t base[SETUP_MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_COMMON_PROGRAMS, NULL, SHGFP_TYPE_CURRENT, base) != S_OK &&
        SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, SHGFP_TYPE_CURRENT, base) != S_OK)
        wcopy(base, SETUP_MAX_PATH, L"C:\\ProgramData\\Microsoft\\Windows\\Start Menu\\Programs");
    wcopy(out, cap, base);
    wcat(out, cap, L"\\");
    wcat(out, cap, product);
}

static void create_links(const Payload *p, const wchar_t *dir, int desktop) {
    wchar_t product[96];
    ascii_to_w(p->product, product, 96);
    wchar_t exe[SETUP_MAX_PATH];
    wcopy(exe, SETUP_MAX_PATH, dir);
    wcat(exe, SETUP_MAX_PATH, L"\\");
    wcat(exe, SETUP_MAX_PATH, L"SolarSandbox.exe");

    wchar_t menu[SETUP_MAX_PATH];
    start_menu_dir(product, menu, SETUP_MAX_PATH);
    CreateDirectoryW(menu, NULL);

    wchar_t lnk[SETUP_MAX_PATH];
    wcopy(lnk, SETUP_MAX_PATH, menu);
    wcat(lnk, SETUP_MAX_PATH, L"\\");
    wcat(lnk, SETUP_MAX_PATH, product);
    wcat(lnk, SETUP_MAX_PATH, L".lnk");
    make_shortcut(lnk, exe, dir, product, NULL);

    wchar_t unexe[SETUP_MAX_PATH];
    wcopy(unexe, SETUP_MAX_PATH, dir);
    wcat(unexe, SETUP_MAX_PATH, L"\\Uninstall.exe");
    wcopy(lnk, SETUP_MAX_PATH, menu);
    wcat(lnk, SETUP_MAX_PATH, L"\\Uninstall ");
    wcat(lnk, SETUP_MAX_PATH, product);
    wcat(lnk, SETUP_MAX_PATH, L".lnk");
    make_shortcut(lnk, unexe, dir, L"Remove Solar System Sandbox", L"/uninstall");

    if (desktop) {
        wchar_t desk[SETUP_MAX_PATH];
        if (SHGetFolderPathW(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, desk) ==
            S_OK) {
            wcopy(lnk, SETUP_MAX_PATH, desk);
            wcat(lnk, SETUP_MAX_PATH, L"\\");
            wcat(lnk, SETUP_MAX_PATH, product);
            wcat(lnk, SETUP_MAX_PATH, L".lnk");
            make_shortcut(lnk, exe, dir, product, NULL);
        }
    }
}

static void remove_links(const wchar_t *product, int desktop) {
    wchar_t menu[SETUP_MAX_PATH];
    start_menu_dir(product, menu, SETUP_MAX_PATH);
    wchar_t lnk[SETUP_MAX_PATH];
    wcopy(lnk, SETUP_MAX_PATH, menu);
    wcat(lnk, SETUP_MAX_PATH, L"\\");
    wcat(lnk, SETUP_MAX_PATH, product);
    wcat(lnk, SETUP_MAX_PATH, L".lnk");
    DeleteFileW(lnk);
    wcopy(lnk, SETUP_MAX_PATH, menu);
    wcat(lnk, SETUP_MAX_PATH, L"\\Uninstall ");
    wcat(lnk, SETUP_MAX_PATH, product);
    wcat(lnk, SETUP_MAX_PATH, L".lnk");
    DeleteFileW(lnk);
    RemoveDirectoryW(menu);

    wchar_t desk[SETUP_MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_COMMON_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, desk) == S_OK) {
        wcopy(lnk, SETUP_MAX_PATH, desk);
        wcat(lnk, SETUP_MAX_PATH, L"\\");
        wcat(lnk, SETUP_MAX_PATH, product);
        wcat(lnk, SETUP_MAX_PATH, L".lnk");
        DeleteFileW(lnk);
    }
    (void)desktop;
}

/* ------------------------------------------------------------- registry */

static void set_string(HKEY key, const wchar_t *name, const wchar_t *value) {
    RegSetValueExW(key, name, 0, REG_SZ, (const BYTE *)value,
                   (DWORD)((wcslen(value) + 1) * sizeof(wchar_t)));
}

static void set_dword(HKEY key, const wchar_t *name, DWORD value) {
    RegSetValueExW(key, name, 0, REG_DWORD, (const BYTE *)&value, sizeof(value));
}

static void write_registry(const Payload *p, const wchar_t *dir) {
    HKEY key = NULL;
    DWORD disposition = 0;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, UNINSTALL_KEY, 0, NULL, 0, KEY_WRITE, NULL, &key,
                        &disposition) != ERROR_SUCCESS)
        return;

    wchar_t product[96], version[32], publisher[96];
    ascii_to_w(p->product, product, 96);
    ascii_to_w(p->version, version, 32);
    ascii_to_w(p->publisher, publisher, 96);

    wchar_t exe[SETUP_MAX_PATH];
    wcopy(exe, SETUP_MAX_PATH, dir);
    wcat(exe, SETUP_MAX_PATH, L"\\SolarSandbox.exe");

    wchar_t uninstall[SETUP_MAX_PATH + 32];
    wcopy(uninstall, SETUP_MAX_PATH + 32, L"\"");
    wcat(uninstall, SETUP_MAX_PATH + 32, dir);
    wcat(uninstall, SETUP_MAX_PATH + 32, L"\\Uninstall.exe\" /uninstall");

    wchar_t icon[SETUP_MAX_PATH + 8];
    wcopy(icon, SETUP_MAX_PATH + 8, exe);
    wcat(icon, SETUP_MAX_PATH + 8, L",0");

    set_string(key, L"DisplayName", product);
    set_string(key, L"DisplayVersion", version);
    set_string(key, L"Publisher", publisher);
    set_string(key, L"InstallLocation", dir);
    set_string(key, L"DisplayIcon", icon);
    set_string(key, L"UninstallString", uninstall);
    set_string(key, L"URLInfoAbout", L"https://github.com/testID001772/solar-system-sandbox-simulator");
    set_dword(key, L"EstimatedSize", (DWORD)((p->size + 262144u) / 1024u));
    set_dword(key, L"NoModify", 1);
    set_dword(key, L"NoRepair", 1);
    RegCloseKey(key);
}

void setup_forget_install(void) {
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, UNINSTALL_KEY);
}

int setup_read_install_dir(wchar_t *dir, int cap) {
    HKEY key = NULL;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, UNINSTALL_KEY, 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS)
        return 0;
    DWORD type = 0;
    DWORD size = (DWORD)cap * sizeof(wchar_t);
    int ok = RegQueryValueExW(key, L"InstallLocation", NULL, &type, (BYTE *)dir, &size) ==
                 ERROR_SUCCESS && type == REG_SZ && size >= sizeof(wchar_t);
    RegCloseKey(key);
    if (ok) {
        if (size >= (DWORD)cap * sizeof(wchar_t)) dir[cap - 1] = 0;
    } else {
        dir[0] = 0;
    }
    return ok;
}

int setup_self_dir(wchar_t *dir, int cap) {
    wchar_t path[SETUP_MAX_PATH];
    if (!GetModuleFileNameW(NULL, path, SETUP_MAX_PATH)) {
        dir[0] = 0;
        return 0;
    }
    int last = -1;
    for (int i = 0; path[i]; i++)
        if (path[i] == L'\\') last = i;
    if (last <= 0) {
        dir[0] = 0;
        return 0;
    }
    path[last] = 0;
    wcopy(dir, cap, path);
    return 1;
}

void setup_default_dir(wchar_t *dir, int cap) {
    wchar_t base[SETUP_MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_PROGRAM_FILES, NULL, SHGFP_TYPE_CURRENT, base) != S_OK)
        wcopy(base, SETUP_MAX_PATH, L"C:\\Program Files");
    wcopy(dir, cap, base);
    wcat(dir, cap, L"\\Solar System Sandbox");
}

/* ------------------------------------------------------------- folders */

static void make_content_folders(const wchar_t *root) {
    static const wchar_t *FOLDERS[] = {L"Assets",  L"Parts",        L"Texture Pack",
                                       L"Mods",    L"Mods\\Custom_", L"Logs",
                                       L"Saving"};
    if (!root || !root[0]) return;
    wchar_t path[SETUP_MAX_PATH];
    for (int i = 0; i < (int)(sizeof(FOLDERS) / sizeof(FOLDERS[0])); i++) {
        wcopy(path, SETUP_MAX_PATH, root);
        wcat(path, SETUP_MAX_PATH, L"\\");
        wcat(path, SETUP_MAX_PATH, FOLDERS[i]);
        ensure_dir(path);
    }
}

/* %APPDATA%\Solar System Sandbox, the writable root. */
static int user_root(wchar_t *path, int cap) {
    wchar_t base[SETUP_MAX_PATH];
    if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, base) != S_OK) return 0;
    wcopy(path, cap, base);
    wcat(path, cap, L"\\Solar System Sandbox");
    return 1;
}

/* ------------------------------------------------------------- install */

int setup_install(const Payload *p, const wchar_t *dir, int desktop_shortcut,
                  SetupProgress progress, void *ctx, wchar_t *err, int err_cap) {
    if (!p || !dir || !dir[0]) {
        wcopy(err, err_cap, L"No installation folder was given.");
        return 0;
    }
    wchar_t root[SETUP_MAX_PATH];
    wcopy(root, SETUP_MAX_PATH, dir);
    size_t n = wcslen(root);
    while (n > 3 && (root[n - 1] == L'\\' || root[n - 1] == L'/')) root[--n] = 0;

    if (progress) progress(ctx, 3, L"Creating the installation folder...");
    if (!ensure_dir(root)) {
        wcopy(err, err_cap,
              L"The installation folder could not be created. Run setup with administrator "
              L"rights or pick another folder.");
        return 0;
    }

    int total = 0;
    for (int i = 0; i < p->count; i++)
        if (strcmp(p->entries[i].name, "setup.ini") != 0) total++;

    int done = 0;
    for (int i = 0; i < p->count; i++) {
        const PayloadEntry *e = &p->entries[i];
        if (strcmp(e->name, "setup.ini") == 0) continue;
        if (sss_crc32(e->data, e->size) != e->crc) {
            wcopy(err, err_cap, L"The installer payload is damaged. Download setup again.");
            return 0;
        }
        wchar_t path[SETUP_MAX_PATH];
        join_entry(root, e, path, SETUP_MAX_PATH);
        ensure_parent_dirs(path);
        if (progress) {
            wchar_t status[SETUP_MAX_PATH + 32];
            wcopy(status, SETUP_MAX_PATH + 32, L"Copying ");
            payload_entry_name(e, status + wcslen(status), SETUP_MAX_PATH);
            wcat(status, SETUP_MAX_PATH + 32, L"...");
            progress(ctx, 5 + (done * 65) / (total > 0 ? total : 1), status);
        }
        if (!write_file(path, e->data, e->size)) {
            wcopy(err, err_cap, L"A file could not be written. Is the disk full?");
            return 0;
        }
        done++;
    }

    /* The six content folders also live in the writable per-user root, so the
     * app can always write logs and saves even in a read-only install. */
    {
        wchar_t user[SETUP_MAX_PATH];
        if (user_root(user, SETUP_MAX_PATH)) make_content_folders(user);
        make_content_folders(root);
    }

    if (progress) progress(ctx, 75, L"Creating shortcuts...");
    wchar_t unexe[SETUP_MAX_PATH];
    wcopy(unexe, SETUP_MAX_PATH, root);
    wcat(unexe, SETUP_MAX_PATH, L"\\Uninstall.exe");
    if (!copy_self_to(unexe)) {
        wcopy(err, err_cap, L"The uninstaller could not be copied into the installation folder.");
        return 0;
    }

    create_links(p, root, desktop_shortcut);

    if (progress) progress(ctx, 90, L"Registering the application...");
    write_registry(p, root);

    if (progress) progress(ctx, 100, L"Installation complete.");
    return 1;
}

/* ------------------------------------------------------------- uninstall */

static void schedule_self_delete(void) {
    wchar_t self[SETUP_MAX_PATH];
    if (!GetModuleFileNameW(NULL, self, SETUP_MAX_PATH)) return;
    wchar_t cmd[1024];
    wcopy(cmd, 1024, L"/c ping -n 3 127.0.0.1 >nul & del /f /q \"");
    wcat(cmd, 1024, self);
    wcat(cmd, 1024, L"\"");
    wchar_t exe[SETUP_MAX_PATH];
    if (!GetSystemDirectoryW(exe, SETUP_MAX_PATH)) return;
    wcat(exe, SETUP_MAX_PATH, L"\\cmd.exe");
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    if (CreateProcessW(exe, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
}

int setup_uninstall(const Payload *p, const wchar_t *dir, SetupProgress progress, void *ctx,
                    wchar_t *err, int err_cap) {
    wchar_t root[SETUP_MAX_PATH];
    wcopy(root, SETUP_MAX_PATH, dir);
    size_t n = wcslen(root);
    while (n > 3 && (root[n - 1] == L'\\' || root[n - 1] == L'/')) root[--n] = 0;

    wchar_t product[96];
    ascii_to_w(p->product, product, 96);

    if (progress) progress(ctx, 5, L"Removing shortcuts...");
    remove_links(product, 1);

    if (progress) progress(ctx, 20, L"Removing the application from Windows...");
    setup_forget_install();

    if (progress) progress(ctx, 35, L"Deleting files...");
    wchar_t parent_dirs[SETUP_MAX_ENTRIES][SETUP_MAX_PATH];
    int parent_count = 0;
    for (int i = 0; i < p->count; i++) {
        const PayloadEntry *e = &p->entries[i];
        if (strcmp(e->name, "setup.ini") == 0) continue;
        wchar_t path[SETUP_MAX_PATH];
        join_entry(root, e, path, SETUP_MAX_PATH);
        DeleteFileW(path);
        if (progress) progress(ctx, 35 + (i * 40) / (p->count > 0 ? p->count : 1), L"Deleting files...");
    }

    wchar_t unexe[SETUP_MAX_PATH];
    wcopy(unexe, SETUP_MAX_PATH, root);
    wcat(unexe, SETUP_MAX_PATH, L"\\Uninstall.exe");
    wchar_t self[SETUP_MAX_PATH];
    int self_is_uninstaller = 0;
    if (GetModuleFileNameW(NULL, self, SETUP_MAX_PATH) && wesame(self, unexe)) self_is_uninstaller = 1;
    if (!self_is_uninstaller) DeleteFileW(unexe);

    /* Remove any sub folders that the payload created, deepest first. */
    for (int i = 0; i < p->count; i++) {
        const PayloadEntry *e = &p->entries[i];
        if (strcmp(e->name, "setup.ini") == 0) continue;
        wchar_t path[SETUP_MAX_PATH];
        join_entry(root, e, path, SETUP_MAX_PATH);
        wchar_t *slash = NULL;
        for (wchar_t *c = path; *c; c++)
            if (*c == L'\\') slash = c;
        if (slash && slash > path + wcslen(root)) {
            *slash = 0;
            int seen = 0;
            for (int k = 0; k < parent_count; k++)
                if (wesame(parent_dirs[k], path)) seen = 1;
            if (!seen && parent_count < SETUP_MAX_ENTRIES) {
                wcopy(parent_dirs[parent_count++], SETUP_MAX_PATH, path);
            }
        }
    }
    /* Deepest first, so "Texture Pack\\Warm Dusk" goes before "Texture Pack". */
    for (int i = 0; i < parent_count; i++) {
        for (int j = i + 1; j < parent_count; j++) {
            if (wcslen(parent_dirs[j]) > wcslen(parent_dirs[i])) {
                wchar_t tmp[SETUP_MAX_PATH];
                wcopy(tmp, SETUP_MAX_PATH, parent_dirs[i]);
                wcopy(parent_dirs[i], SETUP_MAX_PATH, parent_dirs[j]);
                wcopy(parent_dirs[j], SETUP_MAX_PATH, tmp);
            }
        }
    }
    for (int i = 0; i < parent_count; i++) RemoveDirectoryW(parent_dirs[i]);
    RemoveDirectoryW(root);

    if (progress) progress(ctx, 100, L"Solar System Sandbox has been removed.");
    if (self_is_uninstaller) schedule_self_delete();
    (void)err;
    (void)err_cap;
    return 1;
}
