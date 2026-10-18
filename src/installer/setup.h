/* setup.h -- shared definitions for the Solar System Sandbox installer.
 *
 * The installer is a self-contained Win32 program: the application files are
 * linked into it as a read-only payload section, so a single .exe both installs
 * and (when copied into the install directory) uninstalls the product.
 */
#ifndef SSS_SETUP_H
#define SSS_SETUP_H

#include "core.h"

#define SETUP_MAX_ENTRIES 64
#define SETUP_MAX_PATH 260

typedef struct {
    char name[200];            /* UTF-8, '/' separated, relative to the install dir */
    unsigned int size;
    unsigned int crc;
    const unsigned char *data;
} PayloadEntry;

typedef struct {
    const unsigned char *data;
    unsigned int size;
    unsigned int used;         /* bytes consumed by the entry table */
    int count;
    PayloadEntry entries[SETUP_MAX_ENTRIES];
    char product[96];
    char version[32];
    char publisher[96];
    char exe[64];
} Payload;

typedef void (*SetupProgress)(void *ctx, int percent, const wchar_t *status);

unsigned int sss_crc32(const unsigned char *data, unsigned int len);

/* Parses and validates the blob.  Returns 1 on success. */
int payload_open(Payload *p, const unsigned char *data, unsigned int size);
const char *payload_meta(const Payload *p, const char *key);

/* Converts an entry's UTF-8 relative name into a wide string. */
const wchar_t *payload_entry_name(const PayloadEntry *e, wchar_t *buf, int cap);

/* Platform layer (install.c).  Both return 1 on success; on failure a short
 * human readable reason is stored in err. */
int setup_install(const Payload *p, const wchar_t *dir, int desktop_shortcut,
                  SetupProgress progress, void *ctx, wchar_t *err, int err_cap);
int setup_uninstall(const Payload *p, const wchar_t *dir, SetupProgress progress, void *ctx,
                    wchar_t *err, int err_cap);

/* Fills dir with the default install location ("%ProgramFiles%\\..."). */
void setup_default_dir(wchar_t *dir, int cap);

/* Reads InstallLocation from the uninstall registry key (1 on success). */
int setup_read_install_dir(wchar_t *dir, int cap);

/* Folder containing the running executable (for Uninstall.exe). */
int setup_self_dir(wchar_t *dir, int cap);

/* Removes the uninstall entry from Programs and Features (used by both paths). */
void setup_forget_install(void);

#endif /* SSS_SETUP_H */
