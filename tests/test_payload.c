/* test_payload.c -- host-side checks for the installer payload.
 *
 * Builds and runs on Linux (the parser is deliberately platform neutral):
 *
 *   cc -std=c99 -O1 -Isrc tests/test_payload.c src/installer/payload.c src/core.c \
 *      -lm -o /tmp/test_payload
 *   /tmp/test_payload build/payload.bin build/payload
 *
 * It verifies the blob produced by tools/make_payload.py, walks every entry,
 * checks the CRC and writes the files out so byte equality can be compared
 * against the directory that was packed.
 */
#include "installer/setup.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void check(int cond, const char *what) {
    if (cond) {
        printf("  ok    %s\n", what);
    } else {
        printf("  FAIL  %s\n", what);
        failures++;
    }
}

static unsigned char *read_file(const char *path, unsigned int *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) {
        fclose(f);
        return NULL;
    }
    unsigned char *buffer = (unsigned char *)malloc((size_t)len ? (size_t)len : 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buffer, 1, (size_t)len, f);
    fclose(f);
    if ((long)got != len) {
        free(buffer);
        return NULL;
    }
    *size = (unsigned int)len;
    return buffer;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr, "usage: %s payload.bin packed-dir\n", argv[0]);
        return 2;
    }
    unsigned int size = 0;
    unsigned char *blob = read_file(argv[1], &size);
    if (!blob) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 2;
    }

    printf("payload parser\n");
    Payload payload;
    check(payload_open(&payload, blob, size) == 1, "payload header and table parse");
    check(payload.count >= 2, "payload has entries");
    check(strcmp(payload_meta(&payload, "product"), "Solar System Sandbox") == 0, "product name");
    check(strcmp(payload_meta(&payload, "version"), "1.0.0") == 0, "product version");

    int crc_ok = 1;
    int has_ini = 0;
    int has_exe = 0;
    int has_parts = 0;
    int has_pack = 0;
    int has_mod = 0;
    int has_saving = 0;
    for (int i = 0; i < payload.count; i++) {
        const PayloadEntry *e = &payload.entries[i];
        if (sss_crc32(e->data, e->size) != e->crc) {
            printf("  bad crc: %s\n", e->name);
            crc_ok = 0;
        }
        if (strcmp(e->name, "setup.ini") == 0) has_ini = 1;
        if (strstr(e->name, "SolarSandbox.exe")) has_exe = 1;
        if (strstr(e->name, "Parts/") && strstr(e->name, ".part")) has_parts = 1;
        if (strstr(e->name, "Texture Pack/") && strstr(e->name, "pack.ini")) has_pack = 1;
        if (strstr(e->name, "Mods/Custom_/") && strstr(e->name, ".sims")) has_mod = 1;
        if (strcmp(e->name, "Saving/README.txt") == 0) has_saving = 1;
    }
    check(crc_ok, "every entry passes its CRC-32");
    check(has_ini, "setup.ini metadata entry present");
    check(has_exe, "application executable present");
    check(has_parts, "sample Parts/*.part templates packed");
    check(has_pack, "sample Texture Pack/<name>/pack.ini packed");
    check(has_mod, "sample Mods/Custom_/*.sims scenario packed");
    check(has_saving, "Saving folder marker packed");

    /* Every packed file must appear in the payload with identical bytes. */
    int all_files_found = 1;
    for (int i = 0; i < payload.count; i++) {
        const PayloadEntry *e = &payload.entries[i];
        if (strcmp(e->name, "setup.ini") == 0) continue;
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", argv[2], e->name);
        unsigned int file_size = 0;
        unsigned char *file = read_file(path, &file_size);
        if (!file) {
            printf("  missing on disk: %s\n", path);
            all_files_found = 0;
            continue;
        }
        if (file_size != e->size || memcmp(file, e->data, file_size) != 0) {
            printf("  byte mismatch: %s\n", e->name);
            all_files_found = 0;
        }
        free(file);
    }
    check(all_files_found, "payload bytes match the packed directory");

    /* Negative cases: a truncated blob and a corrupted magic must be rejected. */
    check(payload_open(&payload, blob, 12) == 0, "truncated header rejected");
    unsigned char *broken = (unsigned char *)malloc(size);
    memcpy(broken, blob, size);
    broken[0] = 'X';
    check(payload_open(&payload, broken, size) == 0, "bad magic rejected");
    memcpy(broken, blob, size);
    broken[size - 1] ^= 0xFF; /* corrupt the last payload byte */
    Payload second;
    int parsed = payload_open(&second, broken, size);
    int crc_failed = 1;
    if (parsed) {
        crc_failed = 0;
        for (int i = 0; i < second.count; i++)
            if (sss_crc32(second.entries[i].data, second.entries[i].size) != second.entries[i].crc)
                crc_failed = 1;
    }
    check(parsed && crc_failed, "corrupted file data fails the CRC check");
    free(broken);
    free(blob);

    printf(failures ? "\nFAILED (%d)\n" : "\nALL PAYLOAD TESTS PASSED\n", failures);
    return failures ? 1 : 0;
}
