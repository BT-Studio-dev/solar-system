/* content.h -- drop-in content folders: Parts, Texture Pack, Mods, Assets,
 * plus the writable Logs and Saving folders.
 *
 * Layout (the same names are used next to the executable and in the user's
 * %APPDATA% folder, so a portable copy and an installed copy behave alike):
 *
 *   Assets/                 images, fonts, anything the shell can show
 *   Parts/                  body templates for the "Create body" panel (.part)
 *   Texture Pack/<name>/    pack.ini plus optional textures; recolours the scene
 *   Mods/Custom_/           extra scenarios (.sims), listed in the Library tab
 *   Logs/app.log            timestamped application log
 *   Saving/                 default folder for saved simulations (.sims)
 *
 * The parser is deliberately small and tolerant: "key=value" lines, '#' at the
 * start of a line is a comment, unknown keys are ignored so new keys can be
 * added without breaking older builds.
 */
#ifndef SSS_CONTENT_H
#define SSS_CONTENT_H

#include "core.h"

#define CONTENT_MAX_PARTS 64
#define CONTENT_MAX_PACKS 16
#define CONTENT_MAX_MODS 64
#define CONTENT_PATH 260
#define CONTENT_NAME 64
#define CONTENT_NOTE 96

typedef enum {
    CF_ASSETS = 0,
    CF_PARTS,
    CF_TEXTURE_PACKS,
    CF_MODS,
    CF_LOGS,
    CF_SAVING,
    CF_COUNT
} ContentFolder;

typedef struct {
    char name[CONTENT_NAME];
    char kind[16];              /* star, terrestrial, giant, ice, dwarf, asteroid, comet */
    double mass_kg;
    double radius_km;
    double orbit_au;            /* 0 = no orbit field shown */
    double ecc;
    SsColor col1, col2, accent;
    int ringed;
    double ring_in, ring_out;
    SsColor ring_col;
    char note[CONTENT_NOTE];
    char file[CONTENT_PATH];    /* UTF-8 path the part was read from */
} ContentPart;

typedef struct {
    char name[CONTENT_NAME];
    char note[CONTENT_NOTE];
    SsColor tint;
    int strength;               /* 0-100, how far body colours move to the tint */
    char file[CONTENT_PATH];
} ContentPack;

typedef struct {
    char name[CONTENT_NAME];
    char file[CONTENT_PATH];
} ContentMod;

/* app_root: folder next to the executable (may be read-only when installed).
 * user_root: writable per-user folder. Either may be NULL. */
int content_init(const char *app_root, const char *user_root);
void content_shutdown(void);

/* Rescan the content folders. Safe to call at any time. */
void content_scan(void);

/* UTF-8 path of a folder. content_dir() prefers the writable user root,
 * content_app_dir() the folder next to the executable ("" when unknown). */
const char *content_dir(int folder);
const char *content_app_dir(int folder);
/* UTF-8 path of the writable root ("...\\Solar System Sandbox"). */
const char *content_user_root(void);

int content_part_count(void);
const ContentPart *content_part(int index);
int content_pack_count(void);
const ContentPack *content_pack(int index);
int content_mod_count(void);
const ContentMod *content_mod(int index);

/* "giant" -> BT_GIANT. Returns -1 for unknown names. */
int content_kind_from_name(const char *name);

/* Appends one line "<time> <message>" to <user root>/Logs/app.log. */
void content_log(const char *fmt, ...);

/* Colour helper shared with the parser and the UI ("#rrggbb" or "r,g,b"). */
int content_parse_color(const char *text, SsColor *out);

#endif
