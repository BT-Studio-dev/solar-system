/* gfx_soft.c -- anti-aliased software renderer.
 *
 * Faithful enough to preview the exact same UI code as the Windows build.
 * Used by tests/render_ui.c to produce screenshots on a headless machine.
 * Text comes from the baked glyph atlas (src/font_baked.h).
 */
#include "gfx.h"
#include "font_baked.h"

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_CLIP 12
#define FONT_COUNT 9

struct SsSurface {
    int w, h;
    uint32_t *px; /* 0xAARRGGBB, opaque */
};

struct SsGfx {
    SsSurface *target;
    SsRect clip[MAX_CLIP];
    int clip_count;
};

/* ------------------------------------------------------------------ utils */

static float fminf_(float a, float b) { return a < b ? a : b; }
static float fmaxf_(float a, float b) { return a > b ? a : b; }
static float fclampf_(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static int imini_(int a, int b) { return a < b ? a : b; }
static int imaxi_(int a, int b) { return a > b ? a : b; }

static uint32_t pack_argb(int a, int r, int g, int b) {
    return ((uint32_t)(a & 0xFF) << 24) | ((uint32_t)(r & 0xFF) << 16) |
           ((uint32_t)(g & 0xFF) << 8) | (uint32_t)(b & 0xFF);
}

static void blend_px(SsSurface *s, int x, int y, SsColor c, float cov) {
    if (cov <= 0.0f || x < 0 || y < 0 || x >= s->w || y >= s->h) return;
    float a = (float)COL_A(c) / 255.0f * (cov > 1.0f ? 1.0f : cov);
    if (a <= 0.0f) return;
    uint32_t *p = &s->px[y * s->w + x];
    uint32_t d = *p;
    int dr = (int)((d >> 16) & 0xFF), dg = (int)((d >> 8) & 0xFF), db = (int)(d & 0xFF);
    int r = (int)((float)COL_R(c) * a + (float)dr * (1.0f - a));
    int g = (int)((float)COL_G(c) * a + (float)dg * (1.0f - a));
    int b = (int)((float)COL_B(c) * a + (float)db * (1.0f - a));
    *p = pack_argb(255, r, g, b);
}

static SsColor paint_at(SsPaint p, float x, float y) {
    if (p.kind == 0) return p.c0;
    if (p.kind == 1) {
        float dx = p.x1 - p.x0, dy = p.y1 - p.y0;
        float len2 = dx * dx + dy * dy;
        float t = len2 > 1e-6f ? ((x - p.x0) * dx + (y - p.y0) * dy) / len2 : 0.0f;
        return col_blend(p.c0, p.c1, fclampf_(t, 0.0f, 1.0f));
    }
    float dx = x - p.x0, dy = y - p.y0;
    float t = fclampf_(sqrtf(dx * dx + dy * dy) / (p.x1 > 1e-6f ? p.x1 : 1.0f), 0.0f, 1.0f);
    return col_blend(p.c0, p.c1, t);
}

static int in_clip(SsGfx *g, int x, int y) {
    for (int i = 0; i < g->clip_count; i++) {
        SsRect r = g->clip[i];
        if ((float)x < r.x || (float)y < r.y || (float)x >= r.x + r.w || (float)y >= r.y + r.h) return 0;
    }
    return 1;
}

void gfx_set_text_scale(float s) { (void)s; }
void gfx_flush_fonts(void) {}

/* ------------------------------------------------------------- surfaces */

SsSurface *surf_create(int w, int h) {
    if (w < 1 || h < 1) return NULL;
    SsSurface *s = (SsSurface *)sss_alloc(sizeof(SsSurface));
    if (!s) return NULL;
    s->w = w;
    s->h = h;
    s->px = (uint32_t *)sss_alloc(sizeof(uint32_t) * (size_t)w * (size_t)h);
    if (!s->px) {
        sss_free(s);
        return NULL;
    }
    return s;
}

void surf_destroy(SsSurface *s) {
    if (!s) return;
    sss_free(s->px);
    sss_free(s);
}

int surf_w(SsSurface *s) { return s ? s->w : 0; }
int surf_h(SsSurface *s) { return s ? s->h : 0; }
void *surf_pixels(SsSurface *s) { return s ? s->px : NULL; }

SsSurface *surf_from_rgba(const uint8_t *rgba, int w, int h) {
    SsSurface *s = surf_create(w, h);
    if (!s) return NULL;
    for (int i = 0; i < w * h; i++) {
        int a = rgba[i * 4 + 3];
        int r = rgba[i * 4 + 0] * a / 255;
        int g = rgba[i * 4 + 1] * a / 255;
        int b = rgba[i * 4 + 2] * a / 255;
        s->px[i] = pack_argb(a, r, g, b);
    }
    return s;
}

int surf_write_ppm(SsSurface *s, const char *path) {
    if (!s || !path) return 0;
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    fprintf(f, "P6\n%d %d\n255\n", s->w, s->h);
    unsigned char *row = (unsigned char *)malloc((size_t)s->w * 3);
    if (!row) {
        fclose(f);
        return 0;
    }
    for (int y = 0; y < s->h; y++) {
        for (int x = 0; x < s->w; x++) {
            uint32_t c = s->px[y * s->w + x];
            row[x * 3 + 0] = (unsigned char)((c >> 16) & 0xFF);
            row[x * 3 + 1] = (unsigned char)((c >> 8) & 0xFF);
            row[x * 3 + 2] = (unsigned char)(c & 0xFF);
        }
        fwrite(row, 1, (size_t)s->w * 3, f);
    }
    free(row);
    fclose(f);
    return 1;
}

/* ----------------------------------------------------------------- gfx */

SsGfx *gfx_create(SsSurface *target) {
    SsGfx *g = (SsGfx *)sss_alloc(sizeof(SsGfx));
    if (!g) return NULL;
    g->target = target;
    g->clip_count = 0;
    return g;
}

void gfx_destroy(SsGfx *g) { sss_free(g); }

void gfx_clear(SsGfx *g, SsColor c) {
    if (!g || !g->target) return;
    SsSurface *s = g->target;
    int n = s->w * s->h;
    uint32_t v = c;
    for (int i = 0; i < n; i++) s->px[i] = v;
}

void gfx_push_clip(SsGfx *g, SsRect r) {
    if (!g) return;
    if (g->clip_count > 0) {
        SsRect o;
        if (rect_intersect(g->clip[g->clip_count - 1], r, &o)) r = o;
        else r.w = r.h = 0;
    }
    if (g->clip_count < MAX_CLIP) g->clip[g->clip_count++] = r;
}

void gfx_pop_clip(SsGfx *g) {
    if (g && g->clip_count > 0) g->clip_count--;
}

void gfx_fill_rect(SsGfx *g, SsRect r, SsPaint p) {
    if (!g || !g->target) return;
    int x0 = imaxi_(0, ifloor(r.x)), y0 = imaxi_(0, ifloor(r.y));
    int x1 = imini_(g->target->w, (int)ceilf(r.x + r.w)), y1 = imini_(g->target->h, (int)ceilf(r.y + r.h));
    for (int y = y0; y < y1; y++)
        for (int x = x0; x < x1; x++)
            if (in_clip(g, x, y)) blend_px(g->target, x, y, paint_at(p, x + 0.5f, y + 0.5f), 1.0f);
}

static float sdf_round(float px, float py, SsRect r, float rad) {
    float hw = r.w * 0.5f, hh = r.h * 0.5f;
    if (rad > hw) rad = hw;
    if (rad > hh) rad = hh;
    float cx = r.x + hw, cy = r.y + hh;
    float qx = fabsf(px - cx) - (hw - rad);
    float qy = fabsf(py - cy) - (hh - rad);
    float ax = fmaxf_(qx, 0), ay = fmaxf_(qy, 0);
    return sqrtf(ax * ax + ay * ay) + fminf_(fmaxf_(qx, qy), 0.0f) - rad;
}

void gfx_fill_round(SsGfx *g, SsRect r, float radius, SsPaint p) {
    if (!g || !g->target) return;
    int x0 = imaxi_(0, ifloor(r.x - 1)), y0 = imaxi_(0, ifloor(r.y - 1));
    int x1 = imini_(g->target->w, (int)ceilf(r.x + r.w + 1));
    int y1 = imini_(g->target->h, (int)ceilf(r.y + r.h + 1));
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            if (!in_clip(g, x, y)) continue;
            float d = sdf_round(x + 0.5f, y + 0.5f, r, radius);
            float cov = fclampf_(0.5f - d, 0.0f, 1.0f);
            if (cov > 0) blend_px(g->target, x, y, paint_at(p, x + 0.5f, y + 0.5f), cov);
        }
    }
}

void gfx_stroke_round(SsGfx *g, SsRect r, float radius, float width, SsPaint p) {
    if (!g || !g->target) return;
    float hw = width * 0.5f;
    int x0 = imaxi_(0, ifloor(r.x - width - 1)), y0 = imaxi_(0, ifloor(r.y - width - 1));
    int x1 = imini_(g->target->w, (int)ceilf(r.x + r.w + width + 1));
    int y1 = imini_(g->target->h, (int)ceilf(r.y + r.h + width + 1));
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            if (!in_clip(g, x, y)) continue;
            float d = fabsf(sdf_round(x + 0.5f, y + 0.5f, r, radius)) - hw;
            float cov = fclampf_(0.5f - d, 0.0f, 1.0f);
            if (cov > 0) blend_px(g->target, x, y, paint_at(p, x + 0.5f, y + 0.5f), cov);
        }
    }
}

void gfx_stroke_rect(SsGfx *g, SsRect r, float width, SsPaint p) {
    gfx_stroke_round(g, r, 0.0f, width, p);
}

static float ellipse_dist(float px, float py, SsRect r) {
    float a = fmaxf_(r.w * 0.5f, 0.01f), b = fmaxf_(r.h * 0.5f, 0.01f);
    float cx = r.x + a, cy = r.y + b;
    float dx = px - cx, dy = py - cy;
    float f = (dx * dx) / (a * a) + (dy * dy) / (b * b) - 1.0f;
    float gx = 2.0f * dx / (a * a), gy = 2.0f * dy / (b * b);
    float grad = sqrtf(gx * gx + gy * gy);
    if (grad < 1e-5f) grad = 1e-5f;
    return f / grad;
}

void gfx_fill_ellipse(SsGfx *g, SsRect r, SsPaint p) {
    if (!g || !g->target) return;
    int x0 = imaxi_(0, ifloor(r.x - 1)), y0 = imaxi_(0, ifloor(r.y - 1));
    int x1 = imini_(g->target->w, (int)ceilf(r.x + r.w + 1));
    int y1 = imini_(g->target->h, (int)ceilf(r.y + r.h + 1));
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            if (!in_clip(g, x, y)) continue;
            float d = ellipse_dist(x + 0.5f, y + 0.5f, r);
            float cov = fclampf_(0.5f - d, 0.0f, 1.0f);
            if (cov > 0) blend_px(g->target, x, y, paint_at(p, x + 0.5f, y + 0.5f), cov);
        }
    }
}

void gfx_fill_circle(SsGfx *g, float cx, float cy, float radius, SsPaint p) {
    gfx_fill_ellipse(g, rect_make(cx - radius, cy - radius, radius * 2, radius * 2), p);
}

void gfx_stroke_circle(SsGfx *g, float cx, float cy, float radius, float width, SsPaint p) {
    gfx_stroke_ellipse(g, rect_make(cx - radius, cy - radius, radius * 2, radius * 2), width, p);
}

void gfx_stroke_ellipse(SsGfx *g, SsRect r, float width, SsPaint p) {
    if (!g || !g->target) return;
    float hw = width * 0.5f;
    int x0 = imaxi_(0, ifloor(r.x - width - 1)), y0 = imaxi_(0, ifloor(r.y - width - 1));
    int x1 = imini_(g->target->w, (int)ceilf(r.x + r.w + width + 1));
    int y1 = imini_(g->target->h, (int)ceilf(r.y + r.h + width + 1));
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            if (!in_clip(g, x, y)) continue;
            float d = fabsf(ellipse_dist(x + 0.5f, y + 0.5f, r)) - hw;
            float cov = fclampf_(0.5f - d, 0.0f, 1.0f);
            if (cov > 0) blend_px(g->target, x, y, paint_at(p, x + 0.5f, y + 0.5f), cov);
        }
    }
}

void gfx_arc(SsGfx *g, SsRect r, float start_deg, float sweep_deg, float width, SsPaint p) {
    if (!g || !g->target) return;
    if (fabsf(sweep_deg) < 0.01f) return;
    if (sweep_deg < 0) {
        start_deg += sweep_deg;
        sweep_deg = -sweep_deg;
    }
    float hw = width * 0.5f;
    float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f;
    int x0 = imaxi_(0, ifloor(r.x - width - 1)), y0 = imaxi_(0, ifloor(r.y - width - 1));
    int x1 = imini_(g->target->w, (int)ceilf(r.x + r.w + width + 1));
    int y1 = imini_(g->target->h, (int)ceilf(r.y + r.h + width + 1));
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            if (!in_clip(g, x, y)) continue;
            float px = x + 0.5f, py = y + 0.5f;
            float ang = atan2f(py - cy, px - cx) * RAD2DEG;
            while (ang < 0) ang += 360.0f;
            float a0 = start_deg;
            while (a0 < 0) a0 += 360.0f;
            while (a0 >= 360.0f) a0 -= 360.0f;
            float rel = ang - a0;
            while (rel < 0) rel += 360.0f;
            if (rel > sweep_deg) continue;
            float d = fabsf(ellipse_dist(px, py, r)) - hw;
            float cov = fclampf_(0.5f - d, 0.0f, 1.0f);
            if (cov > 0) blend_px(g->target, x, y, paint_at(p, px, py), cov);
        }
    }
}

static void capsule(SsGfx *g, float x0, float y0, float x1, float y1, float width, SsPaint p) {
    float hw = width * 0.5f;
    int bx0 = imaxi_(0, ifloor(fminf_(x0, x1) - width - 1));
    int by0 = imaxi_(0, ifloor(fminf_(y0, y1) - width - 1));
    int bx1 = imini_(g->target->w, (int)ceilf(fmaxf_(x0, x1) + width + 1));
    int by1 = imini_(g->target->h, (int)ceilf(fmaxf_(y0, y1) + width + 1));
    float dx = x1 - x0, dy = y1 - y0;
    float len2 = dx * dx + dy * dy;
    for (int y = by0; y < by1; y++) {
        for (int x = bx0; x < bx1; x++) {
            if (!in_clip(g, x, y)) continue;
            float px = x + 0.5f, py = y + 0.5f;
            float t = len2 > 1e-6f ? ((px - x0) * dx + (py - y0) * dy) / len2 : 0.0f;
            t = fclampf_(t, 0.0f, 1.0f);
            float qx = px - (x0 + dx * t), qy = py - (y0 + dy * t);
            float d = sqrtf(qx * qx + qy * qy) - hw;
            float cov = fclampf_(0.5f - d, 0.0f, 1.0f);
            if (cov > 0) blend_px(g->target, x, y, paint_at(p, px, py), cov);
        }
    }
}

void gfx_line(SsGfx *g, float x0, float y0, float x1, float y1, float width, SsPaint p) {
    if (!g || !g->target) return;
    capsule(g, x0, y0, x1, y1, width, p);
    if (width > 2.2f) {
        SsPaint sp = p;
        float r = width * 0.5f;
        gfx_fill_circle(g, x0, y0, r, sp);
        gfx_fill_circle(g, x1, y1, r, sp);
    }
}

void gfx_polyline(SsGfx *g, const float *xy, int n, float width, SsPaint p, int closed) {
    if (!g || n < 2) return;
    for (int i = 0; i + 1 < n; i++) gfx_line(g, xy[i * 2], xy[i * 2 + 1], xy[i * 2 + 2], xy[i * 2 + 3], width, p);
    if (closed) gfx_line(g, xy[(n - 1) * 2], xy[(n - 1) * 2 + 1], xy[0], xy[1], width, p);
}

/* Scanline polygon fill with 4x vertical supersampling and fractional spans. */
void gfx_fill_poly(SsGfx *g, const float *xy, int n, SsPaint p) {
    if (!g || !g->target || n < 3) return;
    float minx = xy[0], maxx = xy[0], miny = xy[1], maxy = xy[1];
    for (int i = 1; i < n; i++) {
        minx = fminf_(minx, xy[i * 2]); maxx = fmaxf_(maxx, xy[i * 2]);
        miny = fminf_(miny, xy[i * 2 + 1]); maxy = fmaxf_(maxy, xy[i * 2 + 1]);
    }
    int y0 = imaxi_(0, ifloor(miny)), y1 = imini_(g->target->h - 1, (int)ceilf(maxy));
    int rx0 = imaxi_(0, ifloor(minx)), rx1 = imini_(g->target->w - 1, (int)ceilf(maxx));
    if (y1 < y0 || rx1 < rx0) return;
    int span = rx1 - rx0 + 2;
    float *cov = (float *)sss_alloc(sizeof(float) * (size_t)span);
    if (!cov) return;
    const int SUB = 4;
    for (int y = y0; y <= y1; y++) {
        memset(cov, 0, sizeof(float) * (size_t)span);
        for (int s = 0; s < SUB; s++) {
            float sy = y + (s + 0.5f) / SUB;
            float xs[32];
            int nx = 0;
            for (int i = 0; i < n; i++) {
                int j = (i + 1) % n;
                float ay = xy[i * 2 + 1], by = xy[j * 2 + 1];
                float ax = xy[i * 2], bx = xy[j * 2];
                if ((ay <= sy && by > sy) || (by <= sy && ay > sy)) {
                    float t = (sy - ay) / (by - ay);
                    if (nx < (int)ARRAY_LEN(xs)) xs[nx++] = ax + (bx - ax) * t;
                }
            }
            for (int i = 1; i < nx; i++) {
                float v = xs[i];
                int k = i - 1;
                while (k >= 0 && xs[k] > v) { xs[k + 1] = xs[k]; k--; }
                xs[k + 1] = v;
            }
            for (int i = 0; i + 1 < nx; i += 2) {
                float xa = xs[i], xb = xs[i + 1];
                if (xb <= xa) continue;
                int ia = imaxi_(rx0, ifloor(xa)), ib = imini_(rx1, (int)ceilf(xb) - 1);
                for (int x = ia; x <= ib; x++) {
                    float l = fmaxf_(xa, (float)x), r = fminf_(xb, (float)x + 1.0f);
                    if (r > l) cov[x - rx0] += (r - l) / SUB;
                }
            }
        }
        for (int x = 0; x <= rx1 - rx0; x++) {
            float c = cov[x];
            if (c <= 0.001f) continue;
            int px = x + rx0;
            if (!in_clip(g, px, y)) continue;
            blend_px(g->target, px, y, paint_at(p, px + 0.5f, y + 0.5f), c > 1 ? 1.0f : c);
        }
    }
    sss_free(cov);
}

void gfx_fill_triangle(SsGfx *g, float x0, float y0, float x1, float y1, float x2, float y2, SsPaint p) {
    float pts[6] = {x0, y0, x1, y1, x2, y2};
    gfx_fill_poly(g, pts, 3, p);
}

void gfx_bezier(SsGfx *g, float x0, float y0, float cx1, float cy1, float cx2, float cy2,
                float x1, float y1, float width, SsPaint p) {
    float pts[2 * 25];
    int n = 24;
    for (int i = 0; i <= n; i++) {
        float t = (float)i / n, u = 1 - t;
        float a = u * u * u, b = 3 * u * u * t, c = 3 * u * t * t, d = t * t * t;
        pts[i * 2] = a * x0 + b * cx1 + c * cx2 + d * x1;
        pts[i * 2 + 1] = a * y0 + b * cy1 + c * cy2 + d * y1;
    }
    gfx_polyline(g, pts, n + 1, width, p, 0);
}

/* ------------------------------------------------------------------ text */

static const BakedFont *font_for(FontId id) {
    int i = (int)id;
    if (i < 0 || i >= FONT_COUNT) i = 0;
    return &baked_fonts[i];
}

static const BakedGlyph *find_glyph(const BakedFont *f, uint32_t cp) {
    int lo = 0, hi = f->glyph_count - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (f->glyphs[mid].cp == cp) return &f->glyphs[mid];
        if (f->glyphs[mid].cp < cp) lo = mid + 1;
        else hi = mid - 1;
    }
    return NULL;
}

static uint32_t next_cp(const unsigned char **pp) {
    const unsigned char *p = *pp;
    uint32_t cp = *p++;
    if (cp >= 0xF0) {
        cp = ((cp & 0x07) << 18) | ((p[0] & 0x3F) << 12) | ((p[1] & 0x3F) << 6) | (p[2] & 0x3F);
        p += 3;
    } else if (cp >= 0xE0) {
        cp = ((cp & 0x0F) << 12) | ((p[0] & 0x3F) << 6) | (p[1] & 0x3F);
        p += 2;
    } else if (cp >= 0xC0) {
        cp = ((cp & 0x1F) << 6) | (p[0] & 0x3F);
        p += 1;
    } else if (cp >= 0x80) {
        cp = '?';
    }
    *pp = p;
    return cp;
}

float gfx_text_wn(SsGfx *g, const char *text, int len, FontId font) {
    (void)g;
    if (!text) return 0;
    if (len < 0) len = (int)strlen(text);
    const BakedFont *f = font_for(font);
    const unsigned char *p = (const unsigned char *)text;
    const unsigned char *end = p + len;
    float w = 0;
    while (p < end) {
        uint32_t cp = next_cp(&p);
        const BakedGlyph *gl = find_glyph(f, cp);
        if (gl) w += gl->advance64 / 64.0f;
        else w += f->px * 0.55f;
    }
    return w;
}

float gfx_text_w(SsGfx *g, const char *text, FontId font) {
    return gfx_text_wn(g, text, text ? (int)strlen(text) : 0, font);
}

float gfx_font_h(FontId font) { return (float)font_for(font)->line_h; }
float gfx_font_ascent(FontId font) { return (float)font_for(font)->ascent; }

void gfx_text_n(SsGfx *g, float x, float y, const char *text, int len, FontId font, SsColor color,
                Align halign, VAlign valign) {
    if (!g || !g->target || !text) return;
    if (len < 0) len = (int)strlen(text);
    const BakedFont *f = font_for(font);
    float total = gfx_text_wn(g, text, len, font);
    float start_x = x;
    if (halign == SS_ALIGN_CENTER) start_x -= total * 0.5f;
    else if (halign == SS_ALIGN_RIGHT) start_x -= total;
    float baseline;
    if (valign == SS_VALIGN_TOP) baseline = y + f->ascent;
    else if (valign == SS_VALIGN_MIDDLE) baseline = y + (f->ascent - f->descent) * 0.5f;
    else baseline = y;
    float pen = start_x;
    const unsigned char *p = (const unsigned char *)text;
    const unsigned char *end = p + len;
    int a = COL_A(color);
    if (a == 0) return;
    while (p < end) {
        uint32_t cp = next_cp(&p);
        const BakedGlyph *gl = find_glyph(f, cp);
        if (!gl) {
            float w = f->px * 0.55f;
            SsRect box = rect_make(pen + 1, baseline - f->px * 0.7f, w - 2, f->px * 0.7f);
            gfx_stroke_rect(g, box, 1.0f, paint_solid(col_with_alpha(color, a / 3)));
            pen += w;
            continue;
        }
        if (gl->w > 0 && gl->h > 0) {
            int gx0 = (int)floorf(pen + gl->bearing_x);
            int gy0 = (int)floorf(baseline + gl->bearing_y);
            for (int yy = 0; yy < gl->h; yy++) {
                int sy = gy0 + yy;
                if (sy < 0 || sy >= g->target->h) continue;
                for (int xx = 0; xx < gl->w; xx++) {
                    int sx = gx0 + xx;
                    if (sx < 0 || sx >= g->target->w) continue;
                    if (!in_clip(g, sx, sy)) continue;
                    int ax = gl->x + xx, ay = gl->y + yy;
                    if (ax >= BAKED_ATLAS_W || ay >= BAKED_ATLAS_H) continue;
                    unsigned m = baked_atlas[ay * BAKED_ATLAS_W + ax];
                    if (!m) continue;
                    float cov = (float)m / 255.0f;
                    blend_px(g->target, sx, sy, color, cov);
                }
            }
        }
        pen += gl->advance64 / 64.0f;
    }
}

void gfx_text(SsGfx *g, float x, float y, const char *text, FontId font, SsColor color,
              Align halign, VAlign valign) {
    gfx_text_n(g, x, y, text, text ? (int)strlen(text) : 0, font, color, halign, valign);
}

float gfx_text_elided(SsGfx *g, SsRect box, const char *text, FontId font, SsColor color,
                      Align halign, VAlign valign) {
    if (!text) return 0;
    float full = gfx_text_w(g, text, font);
    if (full <= box.w) {
        gfx_text(g, box.x + (halign == SS_ALIGN_CENTER ? box.w * 0.5f : (halign == SS_ALIGN_RIGHT ? box.w : 0.0f)),
                 box.y + (valign == SS_VALIGN_TOP ? 0.0f : (valign == SS_VALIGN_MIDDLE ? box.h * 0.5f : box.h)),
                 text, font, color, halign, valign);
        return full;
    }
    /* binary search the longest prefix that fits with an ellipsis */
    int len = (int)strlen(text);
    const char *ell = "\xE2\x80\xA6";
    float ellw = gfx_text_w(g, ell, font);
    int lo = 0, hi = len;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        /* do not split UTF-8 sequences */
        while (mid > 0 && mid < len && ((unsigned char)text[mid] & 0xC0) == 0x80) mid--;
        float w = gfx_text_wn(g, text, mid, font) + ellw;
        if (w <= box.w) lo = mid;
        else hi = mid - 1;
    }
    while (lo > 0 && lo < len && ((unsigned char)text[lo] & 0xC0) == 0x80) lo--;
    char buf[512];
    int n = lo < (int)sizeof(buf) - 4 ? lo : (int)sizeof(buf) - 4;
    memcpy(buf, text, (size_t)n);
    memcpy(buf + n, ell, 4);
    float x = box.x + (halign == SS_ALIGN_CENTER ? box.w * 0.5f : (halign == SS_ALIGN_RIGHT ? box.w : 0.0f));
    float y = box.y + (valign == SS_VALIGN_TOP ? 0.0f : (valign == SS_VALIGN_MIDDLE ? box.h * 0.5f : box.h));
    gfx_text_n(g, x, y, buf, n + 3, font, color, halign, valign);
    return gfx_text_wn(g, buf, n + 3, font);
}

/* --------------------------------------------------------------- surfaces */

void gfx_draw_surface_ex(SsGfx *g, SsSurface *src, SsRect src_rect, SsRect dst, float alpha) {
    if (!g || !g->target || !src || dst.w <= 0 || dst.h <= 0) return;
    int x0 = imaxi_(0, ifloor(dst.x)), y0 = imaxi_(0, ifloor(dst.y));
    int x1 = imini_(g->target->w, (int)ceilf(dst.x + dst.w));
    int y1 = imini_(g->target->h, (int)ceilf(dst.y + dst.h));
    if (src_rect.w <= 0) src_rect = rect_make(0, 0, (float)src->w, (float)src->h);
    for (int y = y0; y < y1; y++) {
        float v = ((float)y + 0.5f - dst.y) / dst.h;
        float sy = src_rect.y + v * src_rect.h;
        int iy = (int)sy;
        if (iy < 0) iy = 0;
        if (iy >= src->h) iy = src->h - 1;
        for (int x = x0; x < x1; x++) {
            if (!in_clip(g, x, y)) continue;
            float u = ((float)x + 0.5f - dst.x) / dst.w;
            float sx = src_rect.x + u * src_rect.w;
            int ix = (int)sx;
            if (ix < 0) ix = 0;
            if (ix >= src->w) ix = src->w - 1;
            uint32_t c = src->px[iy * src->w + ix];
            int a = (int)(COL_A(c) * alpha);
            if (a <= 0) continue;
            uint32_t *d = &g->target->px[y * g->target->w + x];
            float fa = (float)a / 255.0f;
            int dr = (int)((float)COL_R(c) * fa + (float)((*d >> 16) & 0xFF) * (1 - fa));
            int dg = (int)((float)COL_G(c) * fa + (float)((*d >> 8) & 0xFF) * (1 - fa));
            int db = (int)((float)COL_B(c) * fa + (float)(*d & 0xFF) * (1 - fa));
            *d = pack_argb(255, dr, dg, db);
        }
    }
}

void gfx_draw_surface(SsGfx *g, SsSurface *src, SsRect dst, float alpha) {
    SsRect full = rect_make(0, 0, (float)surf_w(src), (float)surf_h(src));
    gfx_draw_surface_ex(g, src, full, dst, alpha);
}

void gfx_draw_surface_rot(SsGfx *g, SsSurface *src, SsRect dst, float angle_deg, float alpha) {
    if (!g || !g->target || !src) return;
    float cx = dst.x + dst.w * 0.5f, cy = dst.y + dst.h * 0.5f;
    float rad = angle_deg * (float)DEG2RAD;
    float cs = cosf(rad), sn = sinf(rad);
    int x0 = imaxi_(0, ifloor(dst.x - dst.w * 0.5f)), y0 = imaxi_(0, ifloor(dst.y - dst.h * 0.5f));
    int x1 = imini_(g->target->w, (int)ceilf(dst.x + dst.w * 1.5f));
    int y1 = imini_(g->target->h, (int)ceilf(dst.y + dst.h * 1.5f));
    float hw = dst.w * 0.5f, hh = dst.h * 0.5f;
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            if (!in_clip(g, x, y)) continue;
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            float lx = dx * cs + dy * sn;
            float ly = -dx * sn + dy * cs;
            float u = (lx + hw) / dst.w, v = (ly + hh) / dst.h;
            if (u < 0 || u >= 1 || v < 0 || v >= 1) continue;
            int sx = (int)(u * src->w), sy = (int)(v * src->h);
            if (sx < 0) sx = 0;
            if (sy < 0) sy = 0;
            if (sx >= src->w) sx = src->w - 1;
            if (sy >= src->h) sy = src->h - 1;
            uint32_t c = src->px[sy * src->w + sx];
            float fa = (float)COL_A(c) / 255.0f * alpha;
            if (fa <= 0) continue;
            uint32_t *d = &g->target->px[y * g->target->w + x];
            int dr = (int)((float)COL_R(c) * fa + (float)((*d >> 16) & 0xFF) * (1 - fa));
            int dg = (int)((float)COL_G(c) * fa + (float)((*d >> 8) & 0xFF) * (1 - fa));
            int db = (int)((float)COL_B(c) * fa + (float)(*d & 0xFF) * (1 - fa));
            *d = pack_argb(255, dr, dg, db);
        }
    }
}
