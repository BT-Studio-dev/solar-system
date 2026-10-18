/* payload.c -- parser for the installer payload (portable, no Win32 calls).
 *
 * Kept free of platform headers so the same code can be exercised by the
 * Linux test harness (tests/test_payload.c) against tools/make_payload.py.
 */
#include "setup.h"

#include <string.h>

unsigned int sss_crc32(const unsigned char *data, unsigned int len) {
    static unsigned int table[256];
    static int ready;
    if (!ready) {
        for (unsigned int i = 0; i < 256; i++) {
            unsigned int c = i;
            for (int k = 0; k < 8; k++) c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        ready = 1;
    }
    unsigned int c = 0xFFFFFFFFu;
    for (unsigned int i = 0; i < len; i++) c = table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

static unsigned int rd16(const unsigned char *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}

static unsigned int rd32(const unsigned char *p) {
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) | ((unsigned int)p[2] << 16) |
           ((unsigned int)p[3] << 24);
}

static void copy_str(char *dst, int cap, const char *src, int len) {
    if (cap <= 0) return;
    if (len > cap - 1) len = cap - 1;
    for (int i = 0; i < len; i++) dst[i] = src[i];
    dst[len] = 0;
}

static void meta_parse(Payload *p, const char *text, int len) {
    int line = 0;
    while (line < len) {
        int end = line;
        while (end < len && text[end] != '\n' && text[end] != '\r') end++;
        int eq = line;
        while (eq < end && text[eq] != '=') eq++;
        if (eq < end) {
            const char *key = text + line;
            int key_len = eq - line;
            const char *val = text + eq + 1;
            int val_len = end - eq - 1;
            if (key_len == 4 && memcmp(key, "Name", 4) == 0)
                copy_str(p->product, (int)sizeof(p->product), val, val_len);
            else if (key_len == 7 && memcmp(key, "Version", 7) == 0)
                copy_str(p->version, (int)sizeof(p->version), val, val_len);
            else if (key_len == 9 && memcmp(key, "Publisher", 9) == 0)
                copy_str(p->publisher, (int)sizeof(p->publisher), val, val_len);
            else if (key_len == 10 && memcmp(key, "Executable", 10) == 0)
                copy_str(p->exe, (int)sizeof(p->exe), val, val_len);
        }
        line = end;
        while (line < len && (text[line] == '\r' || text[line] == '\n')) line++;
    }
}

int payload_open(Payload *p, const unsigned char *data, unsigned int size) {
    if (!p || !data || size < 20) return 0;
    if (memcmp(data, "SSSPAY1\0", 8) != 0) return 0;
    if (rd32(data + 8) != 1) return 0;

    unsigned int count = rd32(data + 12);
    if (count == 0 || count > SETUP_MAX_ENTRIES) return 0;

    memset(p, 0, sizeof(*p));
    p->data = data;
    p->size = size;

    unsigned int off = 20;
    for (unsigned int i = 0; i < count; i++) {
        if (off + 12 > size) return 0;
        unsigned int name_len = rd16(data + off);
        unsigned int flags = rd16(data + off + 2);
        unsigned int dsize = rd32(data + off + 4);
        unsigned int crc = rd32(data + off + 8);
        off += 12;
        if (flags != 0) return 0;
        if (name_len == 0 || name_len >= sizeof(p->entries[0].name)) return 0;
        if (name_len > size - off) return 0;
        PayloadEntry *e = &p->entries[p->count];
        copy_str(e->name, (int)sizeof(e->name), (const char *)(data + off), (int)name_len);
        if (e->name[0] == '/' || strstr(e->name, "..")) return 0;
        off += name_len;
        if (dsize > size - off) return 0;
        e->size = dsize;
        e->crc = crc;
        e->data = data + off;
        off += dsize;
        p->count++;
    }
    p->used = off;

    /* setup.ini carries product metadata; it is not installed as a file. */
    for (int i = 0; i < p->count; i++) {
        if (strcmp(p->entries[i].name, "setup.ini") == 0) {
            meta_parse(p, (const char *)p->entries[i].data, (int)p->entries[i].size);
            break;
        }
    }
    if (!p->product[0]) copy_str(p->product, (int)sizeof(p->product), "Solar System Sandbox", 20);
    if (!p->version[0]) copy_str(p->version, (int)sizeof(p->version), "1.0.0", 5);
    if (!p->publisher[0]) copy_str(p->publisher, (int)sizeof(p->publisher), "Solar Sandbox Project", 20);
    if (!p->exe[0]) copy_str(p->exe, (int)sizeof(p->exe), "SolarSandbox.exe", 16);
    return 1;
}

const char *payload_meta(const Payload *p, const char *key) {
    if (!p) return "";
    if (strcmp(key, "version") == 0) return p->version;
    if (strcmp(key, "product") == 0) return p->product;
    if (strcmp(key, "publisher") == 0) return p->publisher;
    if (strcmp(key, "exe") == 0) return p->exe;
    return "";
}

const wchar_t *payload_entry_name(const PayloadEntry *e, wchar_t *buf, int cap) {
    if (!buf || cap <= 0) return L"";
    int n = utf8_to_utf16(e->name, (int)strlen(e->name), buf, cap - 1);
    if (n < 0) n = 0;
    buf[n] = 0;
    return buf;
}
