/* tests/test_content.c -- the drop-in content folders: parts, texture packs,
 * mods, log writing and the tiny key=value parser. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "content.h"
#include "sim.h"

static int fails = 0;
static void unbuf(void) { setvbuf(stdout, NULL, _IONBF, 0); }
#define CHECK(cond, msg, ...) do { if (!(cond)) { printf("FAIL: " msg "\n", ##__VA_ARGS__); fails++; } else { printf("ok   : " msg "\n", ##__VA_ARGS__); } } while (0)

static void mkdir_p(const char *path) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (int i = 1; tmp[i]; i++) {
        if (tmp[i] == '/') {
            tmp[i] = 0;
            mkdir(tmp, 0775);
            tmp[i] = '/';
        }
    }
    mkdir(tmp, 0775);
}

static void write_file(const char *path, const char *text) {
    FILE *f = fopen(path, "wb");
    if (!f) { printf("FAIL: cannot write %s\n", path); fails++; return; }
    fwrite(text, 1, strlen(text), f);
    fclose(f);
}

static int file_has(const char *path, const char *needle) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    static char buf[8192];
    size_t n = fread(buf, 1, sizeof(buf) - 1, f);
    buf[n] = 0;
    fclose(f);
    return strstr(buf, needle) != NULL;
}

int main(void) {
    unbuf();
    const char *base = "build/tmp/content_test";
    char app[512], user[512], p[640];
    mkdir_p(base);
    snprintf(app, sizeof(app), "%s/app", base);
    snprintf(user, sizeof(user), "%s/user", base);

    /* a clean slate */
    char cmd[640];
    snprintf(cmd, sizeof(cmd), "rm -rf '%s'", base);
    if (system(cmd) != 0) { /* first run has nothing to delete */ }
    mkdir_p(base);
    mkdir_p(app);
    mkdir_p(user);

    /* --- colour parser -------------------------------------------------- */
    SsColor c;
    CHECK(content_parse_color("#ff8000", &c) && ((c >> 16) & 255) == 255 &&
              ((c >> 8) & 255) == 128 && (c & 255) == 0,
          "hex colour #ff8000");
    CHECK(content_parse_color("#f80", &c) && ((c >> 16) & 255) == 255 &&
              ((c >> 8) & 255) == 136 && (c & 255) == 0,
          "short hex colour #f80");
    CHECK(content_parse_color(" 12, 34, 56 ", &c) && ((c >> 16) & 255) == 12 &&
              ((c >> 8) & 255) == 34 && (c & 255) == 56,
          "r,g,b colour");
    CHECK(!content_parse_color("#12", &c), "bad hex rejected");
    CHECK(!content_parse_color("hello", &c), "text rejected");

    /* --- kind names ---------------------------------------------------- */
    CHECK(content_kind_from_name("star") == BT_STAR, "kind star");
    CHECK(content_kind_from_name("GIANT") == BT_GIANT, "kind is case insensitive");
    CHECK(content_kind_from_name("blackhole") == BT_BLACKHOLE, "kind blackhole");
    CHECK(content_kind_from_name("gas giant") == BT_GIANT, "kind ignores spaces");
    CHECK(content_kind_from_name("Black Hole") == BT_BLACKHOLE, "kind ignores case and spaces");
    CHECK(content_kind_from_name("planet") == BT_TERRESTRIAL, "kind alias planet");
    CHECK(content_kind_from_name("krypton") == -1, "unknown kind -> -1");
    CHECK(content_kind_from_name(NULL) == -1, "missing kind -> -1");

    /* --- content on disk ----------------------------------------------- */
    snprintf(p, sizeof(p), "%s/Parts", app);
    mkdir_p(p);
    write_file("build/tmp/content_test/app/Parts/Ocean.part",
               "# a test part\n"
               "name = Ocean World\n"
               "kind = terrestrial\n"
               "mass = 5.97e24\n"
               "radius = 6371\n"
               "orbit = 1.0\n"
               "ecc = 0.02\n"
               "color1 = #6ec0e8\n"
               "color2 = #123a66\n"
               "accent = #8fd8ff\n"
               "note = Temperate ocean world\n");
    write_file("build/tmp/content_test/app/Parts/rings.part",
               "kind=ice\n"
               "rings=1\n"
               "ring_in=1.5\n"
               "ring_out=2.1\n"
               "ring_color=210,230,240,170\n");
    snprintf(p, sizeof(p), "%s/Texture Pack/Deep Blue", app);
    mkdir_p(p);
    write_file("build/tmp/content_test/app/Texture Pack/Deep Blue/pack.ini",
               "name = Deep Blue\nnote = cold starlight\ntint = #5aa0ff\nstrength = 45\n");
    snprintf(p, sizeof(p), "%s/Mods/Custom_", app);
    mkdir_p(p);
    write_file("build/tmp/content_test/app/Mods/Custom_/sample.sims", "SSS1\nsim 0 1 0 1 0 1\n");
    snprintf(p, sizeof(p), "%s/Parts", user);
    mkdir_p(p);
    write_file("build/tmp/content_test/user/Parts/user.part", "name = User Part\nmass = 1e22\n");

    CHECK(content_init(app, user) == 1, "content_init succeeds");
    CHECK(content_dir(CF_PARTS) != NULL && strstr(content_dir(CF_PARTS), "user") != NULL,
          "content_dir prefers the writable user root");
    CHECK(content_app_dir(CF_PARTS) != NULL && strstr(content_app_dir(CF_PARTS), "app") != NULL,
          "content_app_dir is the app root");

    CHECK(content_part_count() == 3, "scanned 3 parts, got %d", content_part_count());
    CHECK(content_pack_count() == 1, "scanned 1 pack, got %d", content_pack_count());
    CHECK(content_mod_count() == 1, "scanned 1 mod, got %d", content_mod_count());

    const ContentPart *ocean = NULL, *rings = NULL, *userpart = NULL;
    for (int i = 0; i < content_part_count(); i++) {
        const ContentPart *part = content_part(i);
        if (strcmp(part->name, "Ocean World") == 0) ocean = part;
        if (strcmp(part->name, "rings") == 0) rings = part;
        if (strcmp(part->name, "User Part") == 0) userpart = part;
    }
    CHECK(ocean != NULL, "part keeps its name");
    if (ocean) {
        CHECK(content_kind_from_name(ocean->kind) == BT_TERRESTRIAL, "part kind parsed");
        CHECK(ocean->mass_kg > 5.9e24 && ocean->mass_kg < 6.0e24, "part mass %.3g", ocean->mass_kg);
        CHECK(ocean->radius_km == 6371, "part radius %.1f", ocean->radius_km);
        CHECK(((ocean->col1 >> 16) & 255) == 0x6e && ((ocean->col1 >> 8) & 255) == 0xc0 &&
                  (ocean->col1 & 255) == 0xe8,
              "part colour parsed");
        CHECK(ocean->orbit_au == 1.0 && ocean->ecc == 0.02, "part orbit and eccentricity");
        CHECK(strstr(ocean->note, "Temperate") != NULL, "part note parsed");
    }
    CHECK(rings != NULL, "part with no name falls back to the file name");
    if (rings) {
        CHECK(rings->ringed == 1 && rings->ring_in == 1.5 && rings->ring_out == 2.1,
              "ring fields parsed");
        CHECK(rings->mass_kg == 5.97e24, "part defaults fill missing mass");
    }
    CHECK(userpart != NULL, "user root parts are scanned too");

    const ContentPack *pack = content_pack(0);
    CHECK(pack != NULL, "pack present");
    if (pack) {
        CHECK(strcmp(pack->name, "Deep Blue") == 0, "pack name");
        CHECK(pack->strength == 45, "pack strength %d", pack->strength);
        CHECK((pack->tint & 255) == 0xff, "pack tint parsed");
        CHECK(strstr(pack->file, "Deep Blue") != NULL, "pack keeps its folder");
    }
    CHECK(content_mod_count() == 1 && strstr(content_mod(0)->file, "sample.sims") != NULL,
          "mod file recorded");

    /* --- log ----------------------------------------------------------- */
    content_log("test started, %d parts", content_part_count());
    char log[640];
    snprintf(log, sizeof(log), "%s/app.log", content_dir(CF_LOGS));
    CHECK(file_has(log, "test started, 3 parts"), "log line written to %s", log);
    CHECK(file_has(log, "20"), "log line starts with a timestamp");
    content_log("second line");
    CHECK(file_has(log, "second line"), "log appends a second line");

    /* --- rescan picks up new files ------------------------------------- */
    write_file("build/tmp/content_test/user/Parts/late.part", "name = Late\nkind = comet\n");
    content_scan();
    CHECK(content_part_count() == 4, "rescan finds the new part (%d)", content_part_count());
    int late = -1;
    for (int i = 0; i < content_part_count(); i++)
        if (strcmp(content_part(i)->name, "Late") == 0) late = i;
    CHECK(late >= 0 && content_kind_from_name(content_part(late)->kind) == BT_COMET,
          "new part classified as a comet");

    /* --- folders are created on demand --------------------------------- */
    char dir[640];
    snprintf(dir, sizeof(dir), "%s/Saving", user);
    struct stat st;
    CHECK(stat(dir, &st) == 0 && S_ISDIR(st.st_mode), "Saving folder created in the user root");
    snprintf(dir, sizeof(dir), "%s/Logs", user);
    CHECK(stat(dir, &st) == 0 && S_ISDIR(st.st_mode), "Logs folder created in the user root");

    content_shutdown();
    printf(fails == 0 ? "\nALL CONTENT TESTS PASSED\n" : "\n%d CONTENT TEST(S) FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}
