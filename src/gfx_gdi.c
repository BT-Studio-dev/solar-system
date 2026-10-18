/* gfx_gdi.c -- GDI+ implementation of the drawing API (Windows backend).
 *
 * GDI+ ships with Windows XP and later, so no extra runtime is required on
 * Windows 7.  Anti-aliased shapes, gradients and ClearType-off text match the
 * software renderer used by the screenshot tests closely enough.
 */
#include "gfx.h"

#ifdef SSS_WINDOWS

#include <windows.h>
#include <gdiplus/gdiplus.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* The mingw GDI+ headers expose the flat API macros only when the C++ classes
 * are not pulled in; we include the flat header explicitly. */
#include <gdiplus/gdiplusflat.h>
typedef void GpLinearGradientBrush;
typedef void GpGraphicsPath;

struct SsSurface {
    GpBitmap *bitmap;
    int w, h;
    HDC memory_dc;
    HBITMAP dib;
    HGDIOBJ old_dib;
    void *bits;
};

struct SsGfx {
    SsSurface *target;
    GpGraphics *g;
    GpGraphics *own_g;
};

static GpSolidFill *solid_brush(SsColor c) {
    GpSolidFill *b = NULL;
    GdipCreateSolidFill((ARGB)c, &b);
    return b;
}

static void brush_release(GpSolidFill *b) {
    if (b) GdipDeleteBrush((GpBrush *)b);
}

/* ------------------------------------------------------------- surfaces */

SsSurface *surf_create(int w, int h) {
    if (w < 1 || h < 1) return NULL;
    SsSurface *s = (SsSurface *)sss_alloc(sizeof(SsSurface));
    if (!s) return NULL;
    s->w = w;
    s->h = h;
    HDC screen = GetDC(NULL);
    s->memory_dc = CreateCompatibleDC(screen);
    ReleaseDC(NULL, screen);
    BITMAPINFO bi;
    ZeroMemory(&bi, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h; /* top-down */
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    s->dib = CreateDIBSection(s->memory_dc, &bi, DIB_RGB_COLORS, &s->bits, NULL, 0);
    if (!s->dib || !s->bits) {
        if (s->dib) DeleteObject(s->dib);
        if (s->memory_dc) DeleteDC(s->memory_dc);
        sss_free(s);
        return NULL;
    }
    s->old_dib = SelectObject(s->memory_dc, s->dib);
    /* GDI+ draws straight into the DIB pixels so presenting is a plain BitBlt. */
    if (GdipCreateBitmapFromScan0(w, h, w * 4, PixelFormat32bppPARGB, (BYTE *)s->bits,
                                  &s->bitmap) != Ok) {
        DeleteObject(s->dib);
        DeleteDC(s->memory_dc);
        sss_free(s);
        return NULL;
    }
    return s;
}

void surf_destroy(SsSurface *s) {
    if (!s) return;
    if (s->bitmap) GdipDisposeImage((GpImage *)s->bitmap);
    if (s->dib && s->memory_dc) {
        SelectObject(s->memory_dc, s->old_dib);
        DeleteObject(s->dib);
    }
    if (s->memory_dc) DeleteDC(s->memory_dc);
    sss_free(s);
}

void *surf_native_dc(SsSurface *s) { return s ? (void *)s->memory_dc : NULL; }

int surf_w(SsSurface *s) { return s ? s->w : 0; }
int surf_h(SsSurface *s) { return s ? s->h : 0; }
void *surf_pixels(SsSurface *s) { return s ? s->bits : NULL; }

SsSurface *surf_from_rgba(const uint8_t *rgba, int w, int h) {
    SsSurface *s = surf_create(w, h);
    if (!s || !s->bits) {
        surf_destroy(s);
        return NULL;
    }
    uint32_t *dst = (uint32_t *)s->bits;
    for (int i = 0; i < w * h; i++) {
        unsigned a = rgba[i * 4 + 3];
        unsigned r = rgba[i * 4 + 0] * a / 255;
        unsigned g = rgba[i * 4 + 1] * a / 255;
        unsigned b = rgba[i * 4 + 2] * a / 255;
        dst[i] = (a << 24) | (r << 16) | (g << 8) | b;
    }
    return s;
}

int surf_write_ppm(SsSurface *s, const char *path) {
    (void)s; (void)path;
    return 0;
}

/* ----------------------------------------------------------------- gfx */

SsGfx *gfx_create(SsSurface *target) {
    SsGfx *g = (SsGfx *)sss_alloc(sizeof(SsGfx));
    if (!g) return NULL;
    g->target = target;
    if (GdipGetImageGraphicsContext((GpImage *)target->bitmap, &g->own_g) != Ok) {
        sss_free(g);
        return NULL;
    }
    g->g = g->own_g;
    GdipSetSmoothingMode(g->g, SmoothingModeAntiAlias);
    GdipSetPixelOffsetMode(g->g, PixelOffsetModeHalf);
    GdipSetCompositingQuality(g->g, CompositingQualityHighQuality);
    GdipSetTextRenderingHint(g->g, TextRenderingHintAntiAliasGridFit);
    GdipSetInterpolationMode(g->g, InterpolationModeHighQualityBicubic);
    return g;
}

/* Draw through a memory DC as well so GDI text/shape paths stay fast; the
 * GDI+ graphics object targets the same DIB. */
void gfx_destroy(SsGfx *g) {
    if (!g) return;
    if (g->own_g) GdipDeleteGraphics(g->own_g);
    sss_free(g);
}

void gfx_clear(SsGfx *g, SsColor c) {
    if (!g || !g->g) return;
    GdipGraphicsClear(g->g, (ARGB)c);
}

static GpBrush *make_brush(SsPaint p, SsRect r) {
    if (p.kind == 1) {
        GpLinearGradientBrush *lg = NULL;
        GpPointF a, b;
        a.X = p.x0; a.Y = p.y0;
        b.X = p.x1; b.Y = p.y1;
        if (fabsf(p.x1 - p.x0) < 0.01f && fabsf(p.y1 - p.y0) < 0.01f) {
            a.X = r.x; a.Y = r.y;
            b.X = r.x; b.Y = r.y + r.h;
            if (fabsf(r.h) < 0.01f) b.Y = r.y + 1;
        }
        if (GdipCreateLineBrush(&a, &b, (ARGB)p.c0, (ARGB)p.c1, WrapModeTile, &lg) == Ok) {
            return (GpBrush *)lg;
        }
        return NULL;
    }
    if (p.kind == 2) {
        /* Radial gradient: a path gradient whose path is the ellipse being
         * filled, with the focus moved to the paint's centre.  This is how the
         * planets get their shaded, lit-from-one-side look. */
        GpPath *path = NULL;
        if (GdipCreatePath(FillModeAlternate, &path) != Ok || !path) return NULL;
        GdipAddPathEllipse(path, r.x, r.y, r.w, r.h);
        GpPathGradient *pg = NULL;
        GpStatus st = GdipCreatePathGradientFromPath(path, &pg);
        GdipDeletePath(path);
        if (st != Ok || !pg) return (GpBrush *)solid_brush(p.c0);
        GpPointF focus;
        focus.X = p.x0;
        focus.Y = p.y0;
        GdipSetPathGradientCenterPoint(pg, &focus);
        GdipSetPathGradientCenterColor(pg, (ARGB)p.c0);
        ARGB edge = (ARGB)p.c1;
        INT count = 1;
        GdipSetPathGradientSurroundColorsWithCount(pg, &edge, &count);
        return (GpBrush *)pg;
    }
    return (GpBrush *)solid_brush(p.c0);
}

void gfx_push_clip(SsGfx *g, SsRect r) {
    if (!g || !g->g) return;
    GdipSetClipRect(g->g, r.x, r.y, r.w, r.h, CombineModeIntersect);
}

void gfx_pop_clip(SsGfx *g) {
    if (!g || !g->g) return;
    GdipResetClip(g->g);
}

void gfx_fill_rect(SsGfx *g, SsRect r, SsPaint p) {
    if (!g || !g->g) return;
    GpBrush *b = make_brush(p, r);
    if (!b) return;
    GdipFillRectangle(g->g, b, r.x, r.y, r.w, r.h);
    GdipDeleteBrush(b);
}

void gfx_fill_round(SsGfx *g, SsRect r, float radius, SsPaint p) {
    if (!g || !g->g) return;
    GpBrush *b = make_brush(p, r);
    if (!b) return;
    if (radius > r.w * 0.5f) radius = r.w * 0.5f;
    if (radius > r.h * 0.5f) radius = r.h * 0.5f;
    if (radius <= 0.5f) {
        GdipFillRectangle(g->g, b, r.x, r.y, r.w, r.h);
    } else {
        float d = radius * 2;
        GpGraphicsPath *path = NULL;
        if (GdipCreatePath(FillModeAlternate, &path) == Ok) {
            GdipAddPathArc(path, r.x, r.y, d, d, 180, 90);
            GdipAddPathArc(path, r.x + r.w - d, r.y, d, d, 270, 90);
            GdipAddPathArc(path, r.x + r.w - d, r.y + r.h - d, d, d, 0, 90);
            GdipAddPathArc(path, r.x, r.y + r.h - d, d, d, 90, 90);
            GdipClosePathFigure(path);
            GdipFillPath(g->g, b, path);
            GdipDeletePath(path);
        }
    }
    GdipDeleteBrush(b);
}

void gfx_stroke_round(SsGfx *g, SsRect r, float radius, float width, SsPaint p) {
    if (!g || !g->g) return;
    GpSolidFill *b = solid_brush(p.c0);
    if (!b) return;
    if (radius > r.w * 0.5f) radius = r.w * 0.5f;
    if (radius > r.h * 0.5f) radius = r.h * 0.5f;
    GpPen *pen = NULL;
    if (GdipCreatePen1((ARGB)p.c0, width, UnitPixel, &pen) == Ok) {
        GpGraphicsPath *path = NULL;
        if (radius > 0.5f && GdipCreatePath(FillModeAlternate, &path) == Ok) {
            float x0 = r.x + width * 0.5f, y0 = r.y + width * 0.5f;
            float w0 = r.w - width, h0 = r.h - width;
            float dd = radius * 2;
            GdipAddPathArc(path, x0, y0, dd, dd, 180, 90);
            GdipAddPathArc(path, x0 + w0 - dd, y0, dd, dd, 270, 90);
            GdipAddPathArc(path, x0 + w0 - dd, y0 + h0 - dd, dd, dd, 0, 90);
            GdipAddPathArc(path, x0, y0 + h0 - dd, dd, dd, 90, 90);
            GdipClosePathFigure(path);
            GdipDrawPath(g->g, pen, path);
            GdipDeletePath(path);
        } else {
            GdipDrawRectangle(g->g, pen, r.x + width * 0.5f, r.y + width * 0.5f, r.w - width,
                              r.h - width);
        }
        GdipDeletePen(pen);
    }
    brush_release(b);
}

void gfx_stroke_rect(SsGfx *g, SsRect r, float width, SsPaint p) {
    gfx_stroke_round(g, r, 0.0f, width, p);
}

void gfx_fill_ellipse(SsGfx *g, SsRect r, SsPaint p) {
    if (!g || !g->g) return;
    GpBrush *b = make_brush(p, r);
    if (!b) return;
    GdipFillEllipse(g->g, b, r.x, r.y, r.w, r.h);
    GdipDeleteBrush(b);
}

void gfx_fill_circle(SsGfx *g, float cx, float cy, float radius, SsPaint p) {
    if (radius <= 0) return;
    gfx_fill_ellipse(g, rect_make(cx - radius, cy - radius, radius * 2, radius * 2), p);
}

void gfx_stroke_circle(SsGfx *g, float cx, float cy, float radius, float width, SsPaint p) {
    gfx_stroke_ellipse(g, rect_make(cx - radius, cy - radius, radius * 2, radius * 2), width, p);
}

void gfx_stroke_ellipse(SsGfx *g, SsRect r, float width, SsPaint p) {
    if (!g || !g->g) return;
    GpPen *pen = NULL;
    if (GdipCreatePen1((ARGB)p.c0, width, UnitPixel, &pen) != Ok) return;
    GdipDrawEllipse(g->g, pen, r.x + width * 0.5f, r.y + width * 0.5f, r.w - width, r.h - width);
    GdipDeletePen(pen);
}

void gfx_arc(SsGfx *g, SsRect r, float start_deg, float sweep_deg, float width, SsPaint p) {
    if (!g || !g->g) return;
    GpPen *pen = NULL;
    if (GdipCreatePen1((ARGB)p.c0, width, UnitPixel, &pen) != Ok) return;
    GpPen *cap_pen = pen;
    GdipSetPenStartCap(cap_pen, LineCapRound);
    GdipSetPenEndCap(cap_pen, LineCapRound);
    GdipDrawArc(g->g, pen, r.x + width * 0.5f, r.y + width * 0.5f, r.w - width, r.h - width,
                start_deg, sweep_deg);
    GdipDeletePen(pen);
}

void gfx_line(SsGfx *g, float x0, float y0, float x1, float y1, float width, SsPaint p) {
    if (!g || !g->g) return;
    GpPen *pen = NULL;
    if (GdipCreatePen1((ARGB)p.c0, width, UnitPixel, &pen) != Ok) return;
    GdipSetPenStartCap(pen, LineCapRound);
    GdipSetPenEndCap(pen, LineCapRound);
    GdipDrawLine(g->g, pen, x0, y0, x1, y1);
    GdipDeletePen(pen);
}

void gfx_polyline(SsGfx *g, const float *xy, int n, float width, SsPaint p, int closed) {
    if (!g || !g->g || n < 2) return;
    GpPen *pen = NULL;
    if (GdipCreatePen1((ARGB)p.c0, width, UnitPixel, &pen) != Ok) return;
    GdipSetPenLineJoin(pen, LineJoinRound);
    GdipSetPenStartCap(pen, LineCapRound);
    GdipSetPenEndCap(pen, LineCapRound);
    GpPointF pts[256];
    int count = n > 256 ? 256 : n;
    for (int i = 0; i < count; i++) {
        pts[i].X = xy[i * 2];
        pts[i].Y = xy[i * 2 + 1];
    }
    if (closed) {
        GdipDrawPolygon(g->g, pen, pts, count);
    } else {
        GdipDrawLines(g->g, pen, pts, count);
    }
    GdipDeletePen(pen);
}

void gfx_fill_poly(SsGfx *g, const float *xy, int n, SsPaint p) {
    if (!g || !g->g || n < 3) return;
    GpPointF pts[256];
    int count = n > 256 ? 256 : n;
    for (int i = 0; i < count; i++) {
        pts[i].X = xy[i * 2];
        pts[i].Y = xy[i * 2 + 1];
    }
    GpSolidFill *b = solid_brush(p.kind == 0 ? p.c0 : p.c1);
    if (!b) return;
    GdipFillPolygon(g->g, b, pts, count, FillModeWinding);
    brush_release(b);
}

void gfx_fill_triangle(SsGfx *g, float x0, float y0, float x1, float y1, float x2, float y2, SsPaint p) {
    float pts[6] = {x0, y0, x1, y1, x2, y2};
    gfx_fill_poly(g, pts, 3, p);
}

void gfx_bezier(SsGfx *g, float x0, float y0, float cx1, float cy1, float cx2, float cy2,
                float x1, float y1, float width, SsPaint p) {
    if (!g || !g->g) return;
    GpPen *pen = NULL;
    if (GdipCreatePen1((ARGB)p.c0, width, UnitPixel, &pen) != Ok) return;
    GdipDrawBezier(g->g, pen, x0, y0, cx1, cy1, cx2, cy2, x1, y1);
    GdipDeletePen(pen);
}

/* ------------------------------------------------------------------ text */

typedef struct {
    const wchar_t *name;
    int px;
    int style; /* 0 regular, 1 bold, 2 light */
} FontSpec;

static FontSpec font_spec(FontId id, float dpi) {
    FontSpec f;
    float scale = dpi;
    switch (id) {
        case FONT_UI: f.name = L"Segoe UI"; f.px = (int)(12 * scale + 0.5f); f.style = 0; break;
        case FONT_UI_SMALL: f.name = L"Segoe UI"; f.px = (int)(11 * scale + 0.5f); f.style = 0; break;
        case FONT_UI_BOLD: f.name = L"Segoe UI"; f.px = (int)(12 * scale + 0.5f); f.style = 1; break;
        case FONT_UI_TINY: f.name = L"Segoe UI"; f.px = (int)(10 * scale + 0.5f); f.style = 0; break;
        case FONT_H1: f.name = L"Segoe UI"; f.px = (int)(20 * scale + 0.5f); f.style = 2; break;
        case FONT_H2: f.name = L"Segoe UI"; f.px = (int)(15 * scale + 0.5f); f.style = 2; break;
        case FONT_HERO: f.name = L"Segoe UI"; f.px = (int)(42 * scale + 0.5f); f.style = 2; break;
        case FONT_MONO: f.name = L"Consolas"; f.px = (int)(11 * scale + 0.5f); f.style = 0; break;
        case FONT_MONO_BIG: f.name = L"Consolas"; f.px = (int)(18 * scale + 0.5f); f.style = 0; break;
        default: f.name = L"Segoe UI"; f.px = (int)(12 * scale + 0.5f); f.style = 0; break;
    }
    if (f.px < 8) f.px = 8;
    return f;
}

static float g_text_scale = 1.0f;
void gfx_set_text_scale(float s) { g_text_scale = s; }

static GpFont *make_font(FontId id) {
    FontSpec spec = font_spec(id, g_text_scale);
    GpFontFamily *family = NULL;
    if (GdipCreateFontFamilyFromName(spec.name, NULL, &family) != Ok || !family) {
        if (GdipCreateFontFamilyFromName(L"Tahoma", NULL, &family) != Ok) return NULL;
    }
    GpFont *font = NULL;
    int style = FontStyleRegular;
    if (spec.style == 1) style = FontStyleBold;
    if (GdipCreateFont(family, (REAL)spec.px, style, UnitPixel, &font) != Ok) font = NULL;
    GdipDeleteFontFamily(family);
    return font;
}

typedef struct {
    FontId id;
    GpFont *font;
} FontSlot;

static FontSlot g_fonts[9];

static GpFont *font_for(FontId id) {
    int i = (int)id;
    if (i < 0 || i >= 9) i = 0;
    if (!g_fonts[i].font) {
        g_fonts[i].id = (FontId)i;
        g_fonts[i].font = make_font((FontId)i);
    }
    return g_fonts[i].font;
}

void gfx_flush_fonts(void) {
    for (int i = 0; i < 9; i++) {
        if (g_fonts[i].font) {
            GdipDeleteFont(g_fonts[i].font);
            g_fonts[i].font = NULL;
        }
    }
}

static GpStringFormat *make_format(Align h, VAlign v) {
    GpStringFormat *fmt = NULL;
    if (GdipCreateStringFormat(StringFormatFlagsNoWrap | StringFormatFlagsNoClip |
                                   StringFormatFlagsMeasureTrailingSpaces,
                               0, &fmt) != Ok)
        return NULL;
    GdipSetStringFormatAlign(fmt, h == SS_ALIGN_LEFT ? StringAlignmentNear
                                                  : (h == SS_ALIGN_CENTER ? StringAlignmentCenter
                                                                       : StringAlignmentFar));
    GdipSetStringFormatLineAlign(fmt, v == SS_VALIGN_TOP ? StringAlignmentNear
                                                      : (v == SS_VALIGN_MIDDLE ? StringAlignmentCenter
                                                                            : StringAlignmentFar));
    return fmt;
}

static void draw_text_impl(SsGfx *g, float x, float y, const char *text, int len, FontId font,
                           SsColor color, Align halign, VAlign valign) {
    if (!g || !g->g || !text) return;
    GpFont *f = font_for(font);
    if (!f) return;
    GpStringFormat *fmt = make_format(halign, valign);
    GpSolidFill *brush = solid_brush(color);
    if (!brush) {
        if (fmt) GdipDeleteStringFormat(fmt);
        return;
    }
    wchar_t wide[1024];
    int n = utf8_to_utf16(text, len, wide, (int)(sizeof(wide) / sizeof(wide[0])));
    GpRectF box;
    box.X = x;
    box.Y = y;
    box.Width = 4000.0f;
    box.Height = 200.0f;
    if (halign == SS_ALIGN_CENTER) { box.X = x - 2000.0f; }
    else if (halign == SS_ALIGN_RIGHT) { box.X = x - 4000.0f; }
    if (valign == SS_VALIGN_MIDDLE) { box.Y = y - 100.0f; box.Height = 200.0f; }
    else if (valign == SS_VALIGN_BOTTOM) { box.Y = y - 200.0f; }
    GdipDrawString(g->g, wide, n, f, &box, fmt, (GpBrush *)brush);
    GdipDeleteStringFormat(fmt);
    brush_release(brush);
}

void gfx_text(SsGfx *g, float x, float y, const char *text, FontId font, SsColor color,
              Align halign, VAlign valign) {
    draw_text_impl(g, x, y, text, text ? (int)strlen(text) : 0, font, color, halign, valign);
}

void gfx_text_n(SsGfx *g, float x, float y, const char *text, int len, FontId font, SsColor color,
                Align halign, VAlign valign) {
    draw_text_impl(g, x, y, text, len, font, color, halign, valign);
}

float gfx_text_wn(SsGfx *g, const char *text, int len, FontId font) {
    if (!g || !text) return 0;
    GpFont *f = font_for(font);
    if (!f) return 0;
    wchar_t wide[1024];
    int n = utf8_to_utf16(text, len, wide, (int)(sizeof(wide) / sizeof(wide[0])));
    GpStringFormat *fmt = make_format(SS_ALIGN_LEFT, SS_VALIGN_TOP);
    GpRectF box = {0, 0, 4000, 200};
    GpRectF bounds;
    float w = 0;
    if (GdipMeasureString(g->g, wide, n, f, &box, fmt, &bounds, NULL, NULL) == Ok) w = bounds.Width;
    if (fmt) GdipDeleteStringFormat(fmt);
    return w;
}

float gfx_text_w(SsGfx *g, const char *text, FontId font) {
    return gfx_text_wn(g, text, text ? (int)strlen(text) : 0, font);
}

float gfx_font_h(FontId font) {
    GpFont *f = font_for(font);
    if (!f) return 14;
    REAL px = 14;
    GdipGetFontSize(f, &px);
    return px * 1.32f;
}

float gfx_font_ascent(FontId font) {
    GpFont *f = font_for(font);
    if (!f) return 11;
    REAL px = 14;
    GdipGetFontSize(f, &px);
    return px * 1.0f;
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
    const char *ell = "\xE2\x80\xA6";
    float ellw = gfx_text_w(g, ell, font);
    int len = (int)strlen(text);
    int lo = 0, hi = len;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        while (mid > 0 && mid < len && ((unsigned char)text[mid] & 0xC0) == 0x80) mid--;
        if (gfx_text_wn(g, text, mid, font) + ellw <= box.w) lo = mid;
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
    if (!g || !g->g || !src) return;
    if (src_rect.w <= 0) src_rect = rect_make(0, 0, (float)src->w, (float)src->h);
    GpImageAttributes *attr = NULL;
    if (GdipCreateImageAttributes(&attr) != Ok) return;
    ColorMatrix matrix = {{1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, alpha, 0,
                            0, 0, 0, 0, 1}};
    GdipSetImageAttributesColorMatrix(attr, ColorAdjustTypeBitmap, TRUE, &matrix, NULL,
                                      ColorMatrixFlagsDefault);
    GdipDrawImageRectRectI(g->g, (GpImage *)src->bitmap, (int)dst.x, (int)dst.y, (int)dst.w,
                           (int)dst.h, (int)src_rect.x, (int)src_rect.y, (int)src_rect.w,
                           (int)src_rect.h, UnitPixel, attr, NULL, NULL);
    GdipDisposeImageAttributes(attr);
}

void gfx_draw_surface(SsGfx *g, SsSurface *src, SsRect dst, float alpha) {
    gfx_draw_surface_ex(g, src, rect_make(0, 0, (float)surf_w(src), (float)surf_h(src)), dst, alpha);
}

void gfx_draw_surface_rot(SsGfx *g, SsSurface *src, SsRect dst, float angle_deg, float alpha) {
    if (!g || !g->g || !src) return;
    GpMatrix *m = NULL;
    GpMatrix *save = NULL;
    GdipCreateMatrix(&m);
    GdipCreateMatrix(&save);
    float cx = (float)dst.x + dst.w * 0.5f, cy = (float)dst.y + dst.h * 0.5f;
    GdipSetMatrixElements(m, 1, 0, 0, 1, 0, 0);
    GdipTranslateMatrix(m, cx, cy, MatrixOrderAppend);
    GdipRotateMatrix(m, angle_deg, MatrixOrderAppend);
    GdipTranslateMatrix(m, -dst.w * 0.5f, -dst.h * 0.5f, MatrixOrderAppend);
    GdipSetWorldTransform(g->g, m);
    gfx_draw_surface_ex(g, src, rect_make(0, 0, (float)src->w, (float)src->h),
                        rect_make(0, 0, dst.w, dst.h), alpha);
    GdipResetWorldTransform(g->g);
    GdipDeleteMatrix(save);
    GdipDeleteMatrix(m);
}

#endif /* SSS_WINDOWS */
