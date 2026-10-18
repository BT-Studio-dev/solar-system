/* gfx.h -- minimal immediate-mode drawing API.
 *
 * Two backends implement this interface:
 *   gfx_gdi.c   -- GDI+ (the real Windows application)
 *   gfx_soft.c  -- software rasterizer (headless screenshot/regression harness)
 *
 * Everything is specified in destination pixels after the app has applied its
 * DPI scale.  Coordinates are floats; backends anti-alias.
 */
#ifndef SSS_GFX_H
#define SSS_GFX_H

#include "core.h"

typedef struct SsSurface SsSurface;
typedef struct SsGfx SsGfx;

typedef enum {
    FONT_UI,        /* Segoe UI 12 px  */
    FONT_UI_SMALL,  /* Segoe UI 11 px  */
    FONT_UI_BOLD,   /* Segoe UI Semibold 12 px */
    FONT_UI_TINY,   /* Segoe UI 10 px  */
    FONT_H1,        /* Segoe UI Semibold 20 px */
    FONT_H2,        /* Segoe UI Semibold 15 px */
    FONT_HERO,      /* Segoe UI Light 42 px */
    FONT_MONO,      /* Consolas 11 px */
    FONT_MONO_BIG   /* Consolas 18 px */
} FontId;

typedef enum { SS_ALIGN_LEFT = 0, SS_ALIGN_CENTER = 1, SS_ALIGN_RIGHT = 2 } Align;
typedef enum { SS_VALIGN_TOP = 0, SS_VALIGN_MIDDLE = 1, SS_VALIGN_BOTTOM = 2 } VAlign;

/* SsPaint: solid or two-stop gradient. */
typedef struct {
    int kind; /* 0 solid, 1 linear, 2 radial */
    SsColor c0, c1;
    float x0, y0, x1, y1;
} SsPaint;

static inline SsPaint paint_solid(SsColor c) {
    SsPaint p; p.kind = 0; p.c0 = p.c1 = c; p.x0 = p.y0 = p.x1 = p.y1 = 0;
    return p;
}
static inline SsPaint paint_lin(float x0, float y0, float x1, float y1, SsColor c0, SsColor c1) {
    SsPaint p; p.kind = 1; p.c0 = c0; p.c1 = c1; p.x0 = x0; p.y0 = y0; p.x1 = x1; p.y1 = y1;
    return p;
}
static inline SsPaint paint_linv(float x, float y0, float y1, SsColor c0, SsColor c1) {
    return paint_lin(x, y0, x, y1, c0, c1);
}
static inline SsPaint paint_rad(float cx, float cy, float r, SsColor center, SsColor edge) {
    SsPaint p; p.kind = 2; p.c0 = center; p.c1 = edge; p.x0 = cx; p.y0 = cy; p.x1 = r; p.y1 = 0;
    return p;
}

#define PS(c) paint_solid(c)

/* ------------------------------------------------------------- surfaces */
SsSurface *surf_create(int w, int h);
void surf_destroy(SsSurface *s);
int surf_w(SsSurface *s);
int surf_h(SsSurface *s);
void *surf_pixels(SsSurface *s); /* ARGB, tightly packed, for tests */
#ifdef SSS_WINDOWS
void *surf_native_dc(SsSurface *s); /* HDC of the surface (Win32 backend) */
#endif
SsSurface *surf_from_rgba(const uint8_t *rgba, int w, int h); /* premultiplies */

/* --------------------------------------------------------------- gfx */
SsGfx *gfx_create(SsSurface *target);
void gfx_destroy(SsGfx *g);
void gfx_clear(SsGfx *g, SsColor c);

void gfx_push_clip(SsGfx *g, SsRect r);
void gfx_pop_clip(SsGfx *g);

void gfx_fill_rect(SsGfx *g, SsRect r, SsPaint p);
void gfx_fill_round(SsGfx *g, SsRect r, float radius, SsPaint p);
void gfx_stroke_round(SsGfx *g, SsRect r, float radius, float width, SsPaint p);
void gfx_stroke_rect(SsGfx *g, SsRect r, float width, SsPaint p);
void gfx_fill_ellipse(SsGfx *g, SsRect r, SsPaint p);
void gfx_fill_circle(SsGfx *g, float cx, float cy, float radius, SsPaint p);
void gfx_stroke_circle(SsGfx *g, float cx, float cy, float radius, float width, SsPaint p);
void gfx_stroke_ellipse(SsGfx *g, SsRect r, float width, SsPaint p);
void gfx_arc(SsGfx *g, SsRect r, float start_deg, float sweep_deg, float width, SsPaint p);
void gfx_line(SsGfx *g, float x0, float y0, float x1, float y1, float width, SsPaint p);
void gfx_polyline(SsGfx *g, const float *xy, int n, float width, SsPaint p, int closed);
void gfx_fill_poly(SsGfx *g, const float *xy, int n, SsPaint p);
void gfx_fill_triangle(SsGfx *g, float x0, float y0, float x1, float y1, float x2, float y2, SsPaint p);
void gfx_bezier(SsGfx *g, float x0, float y0, float cx1, float cy1, float cx2, float cy2,
                float x1, float y1, float width, SsPaint p);

void gfx_text(SsGfx *g, float x, float y, const char *text, FontId font, SsColor color,
              Align halign, VAlign valign);
void gfx_text_n(SsGfx *g, float x, float y, const char *text, int len, FontId font, SsColor color,
                Align halign, VAlign valign);
float gfx_text_w(SsGfx *g, const char *text, FontId font);
float gfx_text_wn(SsGfx *g, const char *text, int len, FontId font);
float gfx_font_h(FontId font);
float gfx_font_ascent(FontId font);

void gfx_draw_surface(SsGfx *g, SsSurface *src, SsRect dst, float alpha);
void gfx_draw_surface_ex(SsGfx *g, SsSurface *src, SsRect src_rect, SsRect dst, float alpha);
void gfx_draw_surface_rot(SsGfx *g, SsSurface *src, SsRect dst, float angle_deg, float alpha);

/* Windows backend: text scale follows the window DPI. */
void gfx_set_text_scale(float s);
void gfx_flush_fonts(void);

/* Convenience: text elided to a width, returns width used. */
float gfx_text_elided(SsGfx *g, SsRect box, const char *text, FontId font, SsColor color,
                      Align halign, VAlign valign);

/* Soft backend only: write the surface as a binary PPM (harness/debug). */
int surf_write_ppm(SsSurface *s, const char *path);

#endif /* SSS_GFX_H */
