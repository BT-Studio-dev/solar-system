/* ui.c -- window chrome, inspector, transport controls and input handling. */
#include "app.h"
#include "theme.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------- ids */

enum {
    W_NONE = 0,
    /* plain buttons: 1000-1099 */
    W_PLAY = 1000,
    W_SPEED,
    W_NOW,
    W_PRESET,
    W_SAVE,
    W_LOAD,
    W_THEME,
    W_HELP,
    W_TAB_INSPECT,
    W_TAB_CREATE,
    W_TAB_LIBRARY,
    W_OPEN_CONTENT,
    W_RESCAN,
    W_FOCUS,
    W_TRACK,
    W_DYNAMIC,
    W_DELETE,
    W_ADD,
    W_ORBITS,
    W_LABELS,
    W_TRAILS,
    W_MOONS,
    W_SCALE,
    W_ZIN,
    W_ZOUT,
    W_RESETVIEW,
    W_NBODY,
    W_MASS,
    W_RADIUS,
    W_ORBIT,
    W_MIN,
    W_MAX,
    W_CLOSE,
    W_HELP_CLOSE,
    /* preset dropdown rows: 2000-2099 */
    W_PRESET_0 = 2000,
    /* body-type chips in the composer: 3000-3099 */
    W_KIND_0 = 3000,
    /* satellite rows: 4000-4299 */
    W_BODY_ROW_0 = 4000,
    /* scenario rows in the create tab: 5000-5099 */
    W_SCENARIO_0 = 5000,
    /* quick actions: 6000-6099 */
    W_QUICK_0 = 6000,
    /* theme rows: 7000-7099 */
    W_THEME_0 = 7000,
    /* Library: parts 8000-8099, texture packs 8100-8199, mods 8200-8299 */
    W_PART_0 = 8000,
    W_PACK_0 = 8100,
    W_MOD_0 = 8200,
    /* "open this folder" buttons: 8400+ContentFolder */
    W_FOLDER_0 = 8400,
    /* simulation panel: 8500-8599, body picker rows: 8600-8799 */
    W_SIM = 8500,
    W_SIM_CLOSE,
    W_DATE_Y_DN, W_DATE_Y_UP,
    W_DATE_M_DN, W_DATE_M_UP,
    W_DATE_D_DN, W_DATE_D_UP,
    W_DATE_SET,
    W_DATE_NOW,
    W_OPT_0 = 8520,
    W_TRACK_PICK = 8540,
    W_SEP_A,
    W_SEP_B,
    W_PICK_0 = 8600
};

/* ------------------------------------------------------------------ util */

static void hit_init(App *app) { app->ui.hit_count = 0; }

static void hit_add(App *app, uint32_t id, SsRect r) {
    UI *ui = &app->ui;
    if (ui->hit_count >= (int)ARRAY_LEN(ui->hit_id)) return;
    ui->hit_id[ui->hit_count] = id;
    ui->hit_rect[ui->hit_count] = r;
    ui->hit_count++;
}

static uint32_t hit_find(App *app, float x, float y) {
    UI *ui = &app->ui;
    for (int i = ui->hit_count - 1; i >= 0; i--) {
        if (rect_contains(ui->hit_rect[i], x, y)) return ui->hit_id[i];
    }
    return 0;
}

static SsRect hit_rect_of(App *app, uint32_t id) {
    UI *ui = &app->ui;
    for (int i = ui->hit_count - 1; i >= 0; i--)
        if (ui->hit_id[i] == id) return ui->hit_rect[i];
    return rect_make(0, 0, 0, 0);
}

static int is_hot(App *app, uint32_t id, SsRect r) {
    if (rect_contains(r, app->ui.mouse_x, app->ui.mouse_y)) {
        app->ui.hot_id = id;
        return 1;
    }
    return 0;
}

/* ---------------------------------------------------------------- icons */

static void ico_play(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_fill_triangle(g, x - s * 0.32f, y - s * 0.48f, x - s * 0.32f, y + s * 0.48f,
                      x + s * 0.52f, y, PS(c));
}
static void ico_pause(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_fill_round(g, rect_make(x - s * 0.42f, y - s * 0.44f, s * 0.3f, s * 0.88f), 1, PS(c));
    gfx_fill_round(g, rect_make(x + s * 0.12f, y - s * 0.44f, s * 0.3f, s * 0.88f), 1, PS(c));
}
static void ico_plus(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_line(g, x - s * 0.42f, y, x + s * 0.42f, y, 1.4f, PS(c));
    gfx_line(g, x, y - s * 0.42f, x, y + s * 0.42f, 1.4f, PS(c));
}
static void ico_minus(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_line(g, x - s * 0.42f, y, x + s * 0.42f, y, 1.4f, PS(c));
}
static void ico_chevron(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_line(g, x - s * 0.3f, y - s * 0.16f, x, y + s * 0.18f, 1.4f, PS(c));
    gfx_line(g, x, y + s * 0.18f, x + s * 0.3f, y - s * 0.16f, 1.4f, PS(c));
}
static void ico_orbit(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_stroke_ellipse(g, rect_make(x - s * 0.62f, y - s * 0.30f, s * 1.24f, s * 0.60f), 1.1f, PS(c));
    gfx_fill_circle(g, x, y, s * 0.16f, PS(c));
    gfx_fill_circle(g, x + s * 0.56f, y, s * 0.1f, PS(c));
}
static void ico_labels(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_text(g, x, y + s * 0.34f, "Aa", FONT_UI_TINY, c, SS_ALIGN_CENTER, SS_VALIGN_BOTTOM);
}
static void ico_trail(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_arc(g, rect_make(x - s * 0.5f, y - s * 0.5f, s, s), 200, 120, 1.3f, PS(c));
    gfx_fill_circle(g, x + s * 0.34f, y + s * 0.24f, s * 0.14f, PS(c));
}
static void ico_moons(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_fill_circle(g, x - s * 0.1f, y - s * 0.06f, s * 0.26f, PS(c));
    gfx_stroke_circle(g, x + s * 0.06f, y + s * 0.06f, s * 0.5f, 1.0f, PS(col_with_alpha(c, 170)));
    gfx_fill_circle(g, x + s * 0.5f, y + s * 0.34f, s * 0.09f, PS(c));
}
static void ico_ruler(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_line(g, x - s * 0.55f, y + s * 0.25f, x + s * 0.55f, y + s * 0.25f, 1.2f, PS(c));
    for (int i = 0; i < 3; i++)
        gfx_line(g, x - s * 0.55f + i * s * 0.55f, y + s * 0.25f, x - s * 0.55f + i * s * 0.55f,
                 y - s * 0.18f - (i == 1 ? s * 0.16f : 0), 1.1f, PS(c));
}
static void ico_save(SsGfx *g, float x, float y, float s, SsColor c) {
    SsRect r = rect_make(x - s * 0.4f, y - s * 0.4f, s * 0.8f, s * 0.8f);
    gfx_stroke_round(g, r, 1.5f, 1.1f, PS(c));
    gfx_fill_rect(g, rect_make(x - s * 0.18f, y - s * 0.4f, s * 0.36f, s * 0.3f), PS(c));
    gfx_stroke_rect(g, rect_make(x - s * 0.24f, y + s * 0.04f, s * 0.48f, s * 0.36f), 1.0f, PS(c));
}
static void ico_load(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_stroke_round(g, rect_make(x - s * 0.5f, y - s * 0.25f, s * 1.0f, s * 0.66f), 1.5f, 1.1f, PS(c));
    float pts[8] = {x - s * 0.5f, y - s * 0.25f, x - s * 0.38f, y - s * 0.48f,
                    x - s * 0.05f, y - s * 0.48f, x + s * 0.04f, y - s * 0.25f};
    gfx_polyline(g, pts, 4, 1.1f, PS(c), 0);
}
static void ico_target(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_stroke_circle(g, x, y, s * 0.42f, 1.1f, PS(c));
    gfx_fill_circle(g, x, y, s * 0.12f, PS(c));
    gfx_line(g, x - s * 0.62f, y, x - s * 0.34f, y, 1.0f, PS(c));
    gfx_line(g, x + s * 0.34f, y, x + s * 0.62f, y, 1.0f, PS(c));
    gfx_line(g, x, y - s * 0.62f, x, y - s * 0.34f, 1.0f, PS(c));
    gfx_line(g, x, y + s * 0.34f, x, y + s * 0.62f, 1.0f, PS(c));
}
static void ico_delete(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_stroke_round(g, rect_make(x - s * 0.28f, y - s * 0.28f, s * 0.56f, s * 0.7f), 2, 1.1f, PS(c));
    gfx_line(g, x - s * 0.42f, y - s * 0.34f, x + s * 0.42f, y - s * 0.34f, 1.2f, PS(c));
    gfx_line(g, x - s * 0.14f, y - s * 0.46f, x + s * 0.14f, y - s * 0.46f, 1.2f, PS(c));
}
static void ico_help(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_stroke_circle(g, x, y, s * 0.46f, 1.1f, PS(c));
    gfx_text(g, x, y + 1, "?", FONT_UI_BOLD, c, SS_ALIGN_CENTER, SS_VALIGN_MIDDLE);
}
/* Four swatches in a 2x2 grid: the theme picker. */
static void ico_theme(SsGfx *g, float x, float y, float s, SsColor c) {
    float q = s * 0.40f, gap = s * 0.20f;
    float x0 = x - q - gap * 0.5f, y0 = y - q - gap * 0.5f;
    float rad = q * 0.34f;
    gfx_fill_round(g, rect_make(x0, y0, q, q), rad, PS(c));
    gfx_fill_round(g, rect_make(x0 + q + gap, y0, q, q), rad, PS(col_with_alpha(c, 185)));
    gfx_fill_round(g, rect_make(x0, y0 + q + gap, q, q), rad, PS(col_with_alpha(c, 135)));
    gfx_fill_round(g, rect_make(x0 + q + gap, y0 + q + gap, q, q), rad, PS(col_with_alpha(c, 90)));
    gfx_stroke_round(g, rect_make(x0 - 1, y0 - 1, q * 2 + gap + 2, q * 2 + gap + 2), rad, 1.0f,
                     PS(col_with_alpha(c, 60)));
}

static void ico_gravity(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_fill_circle(g, x, y + s * 0.2f, s * 0.16f, PS(c));
    gfx_arc(g, rect_make(x - s * 0.52f, y - s * 0.42f, s * 1.04f, s * 1.04f), 200, 140, 1.1f, PS(c));
    gfx_arc(g, rect_make(x - s * 0.78f, y - s * 0.68f, s * 1.56f, s * 1.56f), 205, 130, 1.0f,
            PS(col_with_alpha(c, 120)));
}
static void ico_planet(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_fill_circle(g, x, y, s * 0.34f, PS(c));
    gfx_stroke_ellipse(g, rect_make(x - s * 0.72f, y - s * 0.24f, s * 1.44f, s * 0.48f), 1.1f,
                       PS(col_with_alpha(c, 190)));
}
static void ico_star(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_fill_circle(g, x, y, s * 0.26f, PS(c));
    for (int i = 0; i < 4; i++) {
        float dx = (i == 0 ? 1.0f : (i == 1 ? -1.0f : 0.0f));
        float dy = (i == 2 ? 1.0f : (i == 3 ? -1.0f : 0.0f));
        gfx_line(g, x + dx * s * 0.38f, y + dy * s * 0.38f, x + dx * s * 0.68f, y + dy * s * 0.68f,
                 1.0f, PS(col_with_alpha(c, 170)));
    }
}
static void ico_asteroid(SsGfx *g, float x, float y, float s, SsColor c) {
    float pts[10] = {x - s * 0.12f, y - s * 0.42f, x + s * 0.28f, y - s * 0.28f, x + s * 0.4f, y + s * 0.12f,
                     x + s * 0.02f, y + s * 0.36f, x - s * 0.36f, y + s * 0.1f};
    gfx_fill_poly(g, pts, 5, PS(c));
    gfx_fill_circle(g, x - s * 0.1f, y - s * 0.04f, s * 0.08f, PS(RGBA(0, 0, 0, 90)));
    gfx_fill_circle(g, x + s * 0.1f, y + s * 0.12f, s * 0.06f, PS(RGBA(0, 0, 0, 80)));
}
static void ico_comet(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_fill_circle(g, x + s * 0.34f, y - s * 0.28f, s * 0.16f, PS(c));
    float pts[8] = {x + s * 0.34f, y - s * 0.28f, x - s * 0.2f, y + s * 0.12f,
                    x - s * 0.5f, y + s * 0.4f, x + s * 0.02f, y + s * 0.1f};
    gfx_fill_poly(g, pts, 4, PS(col_with_alpha(c, 90)));
}
static void ico_blackhole(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_fill_circle(g, x, y, s * 0.34f, PS(RGB_A(6, 5, 10)));
    gfx_stroke_circle(g, x, y, s * 0.42f, 1.2f, PS(c));
    gfx_stroke_ellipse(g, rect_make(x - s * 0.78f, y - s * 0.18f, s * 1.56f, s * 0.36f), 1.0f,
                       PS(col_with_alpha(c, 130)));
}
static void ico_folder(SsGfx *g, float x, float y, float s, SsColor c) {
    float w = s * 1.15f, h = s * 0.82f;
    gfx_fill_round(g, rect_make(x - w * 0.5f, y - h * 0.5f + s * 0.10f, w * 0.52f, s * 0.30f),
                   1.2f, PS(c));
    gfx_fill_round(g, rect_make(x - w * 0.5f, y - h * 0.5f + s * 0.20f, w, h), 1.6f, PS(c));
}

static void ico_sliders(SsGfx *g, float x, float y, float s, SsColor c) {
    for (int i = 0; i < 3; i++) {
        float yy = y - s * 0.42f + i * s * 0.42f;
        gfx_line(g, x - s * 0.52f, yy, x + s * 0.52f, yy, 1.2f, PS(col_with_alpha(c, 190)));
        float kx = x + (i == 0 ? -s * 0.18f : (i == 1 ? s * 0.22f : -s * 0.02f));
        gfx_fill_circle(g, kx, yy, s * 0.17f, PS(c));
    }
}

static void ico_reset(SsGfx *g, float x, float y, float s, SsColor c) {
    gfx_arc(g, rect_make(x - s * 0.4f, y - s * 0.4f, s * 0.8f, s * 0.8f), 40, 280, 1.2f, PS(c));
    float px = x + s * 0.3f, py = y - s * 0.34f;
    gfx_fill_triangle(g, px - s * 0.16f, py - s * 0.1f, px + s * 0.14f, py - s * 0.16f,
                      px + s * 0.02f, py + s * 0.16f, PS(c));
}

typedef void (*IconFn)(SsGfx *, float, float, float, SsColor);

/* -------------------------------------------------------------- widgets */

static void text_tracked(SsGfx *g, float x, float y, const char *text, FontId f, SsColor c,
                         float tracking, Align align, VAlign v) {
    float total = 0;
    for (const char *p = text; *p; p++) {
        char ch[2] = {*p, 0};
        total += gfx_text_w(g, ch, f) + tracking;
    }
    total -= tracking;
    if (align == SS_ALIGN_CENTER) x -= total * 0.5f;
    else if (align == SS_ALIGN_RIGHT) x -= total;
    float cx = x;
    float line = gfx_font_h(f);
    float ty = y;
    if (v == SS_VALIGN_TOP) ty = y + line * 0.5f;
    else if (v == SS_VALIGN_BOTTOM) ty = y - line * 0.5f;
    if (v == SS_VALIGN_TOP) {
        for (const char *p = text; *p; p++) {
            char ch[2] = {*p, 0};
            gfx_text(g, cx, y, ch, f, c, SS_ALIGN_LEFT, SS_VALIGN_TOP);
            cx += gfx_text_w(g, ch, f) + tracking;
        }
    } else {
        for (const char *p = text; *p; p++) {
            char ch[2] = {*p, 0};
            gfx_text(g, cx, ty, ch, f, c, SS_ALIGN_LEFT, SS_VALIGN_MIDDLE);
            cx += gfx_text_w(g, ch, f) + tracking;
        }
    }
}

static void section_header(SsGfx *g, float x, float y, const char *text) {
    text_tracked(g, x, y, text, FONT_UI_TINY, C_HEADER, 0.7f, SS_ALIGN_LEFT, SS_VALIGN_TOP);
}

static float wrap_text(SsGfx *g, SsRect box, const char *text, FontId f, SsColor c, float line_h) {
    float x = box.x, y = box.y;
    float line_start = x;
    char word[64];
    int wi = 0;
    float maxw = box.w;
    const char *p = text;
    while (1) {
        char ch = *p;
        if (ch == ' ' || ch == 0 || ch == '\n') {
            word[wi] = 0;
            if (wi > 0) {
                float ww = gfx_text_w(g, word, f);
                if (x > line_start && x + ww > line_start + maxw) {
                    x = line_start;
                    y += line_h;
                }
                gfx_text(g, x, y, word, f, c, SS_ALIGN_LEFT, SS_VALIGN_TOP);
                x += ww + gfx_text_w(g, " ", f);
            }
            wi = 0;
            if (ch == 0) break;
            if (ch == '\n') {
                x = line_start;
                y += line_h;
            }
        } else if (wi < (int)sizeof(word) - 1) {
            word[wi++] = ch;
        }
        p++;
    }
    return y + line_h - box.y;
}

typedef struct {
    int style; /* 0 secondary, 1 primary, 2 ghost, 3 danger */
    int active;
} BtnStyle;

static void draw_button_base(SsGfx *g, SsRect r, float radius, int style, int hot, int down) {
    SsPaint p;
    if (style == 1) {
        SsColor top = hot ? col_blend(C_ACCENT, RGB_A(255, 255, 255), 0.18f) : C_ACCENT;
        SsColor bottom = hot ? C_ACCENT_DEEP : col_blend(C_ACCENT_DEEP, RGB_A(0, 0, 0), 0.12f);
        p = paint_linv(0, r.y, r.y + r.h, top, bottom);
    } else if (style == 3) {
        SsColor top = hot ? col_blend(C_RED, RGB_A(255, 255, 255), 0.14f)
                          : col_blend(C_RED, RGB_A(0, 0, 0), 0.05f);
        SsColor bottom = hot ? col_blend(C_RED, RGB_A(0, 0, 0), 0.35f)
                             : col_blend(C_RED, RGB_A(0, 0, 0), 0.28f);
        p = paint_linv(0, r.y, r.y + r.h, top, bottom);
    } else if (style == 2) {
        p = paint_solid(down ? sss_theme()->ghost_down
                             : (hot ? sss_theme()->ghost_hot : sss_theme()->ghost));
    } else {
        p = paint_linv(0, r.y, r.y + r.h,
                       down ? C_BTN_TOP_DOWN : (hot ? C_BTN_TOP_HOVER : C_BTN_TOP),
                       down ? C_BTN_BOTTOM_DOWN : (hot ? C_BTN_BOTTOM_HOVER : C_BTN_BOTTOM));
    }
    gfx_fill_round(g, r, radius, p);
    if (style == 1) {
        SsPaint top = paint_linv(0, r.y + 1, r.y + r.h * 0.55f,
                                 col_with_alpha(RGB_A(255, 255, 255), 64),
                                 col_with_alpha(RGB_A(255, 255, 255), 0));
        gfx_fill_round(g, rect_make(r.x + 1, r.y + 1, r.w - 2, r.h * 0.52f), radius - 1, top);
        gfx_stroke_round(g, r, radius, 1.0f, PS(col_with_alpha(C_ACCENT_HI, hot ? 200 : 140)));
    } else if (style == 3) {
        gfx_stroke_round(g, r, radius, 1.0f,
                         PS(hot ? col_with_alpha(C_RED, 190) : col_with_alpha(C_RED, 90)));
    } else if (style != 2) {
        gfx_stroke_round(g, r, radius, 1.0f, PS(down ? C_BTN_BORDER_HOVER : (hot ? C_BTN_BORDER_HOVER : C_BTN_BORDER)));
    }
    if (down) gfx_fill_round(g, r, radius, PS(RGBA(0, 0, 0, 30)));
}

static int button_icon(App *app, SsGfx *g, uint32_t id, SsRect r, IconFn ico, const char *tooltip,
                       int style) {
    int hot = is_hot(app, id, r);
    int down = app->ui.active_id == id;
    float radius = (style == 2) ? 4.0f : RADIUS_BUTTON;
    draw_button_base(g, r, radius, style, hot, down);
    SsColor c = style == 1 ? RGB_A(255, 255, 255) : (style == 2 ? (hot ? C_TEXT : C_TEXT_DIM) : C_TEXT);
    if (ico) ico(g, r.x + r.w * 0.5f, r.y + r.h * 0.5f, r.h * 0.42f, c);
    hit_add(app, id, r);
    return down;
}

static int button_text(App *app, SsGfx *g, uint32_t id, SsRect r, const char *label, int style,
                       IconFn ico) {
    int hot = is_hot(app, id, r);
    int down = app->ui.active_id == id;
    draw_button_base(g, r, RADIUS_BUTTON, style, hot, down);
    SsColor c = (style == 1) ? sss_theme()->on_accent
                             : (style == 3 ? RGB_A(255, 230, 226) : C_TEXT);
    float tw = gfx_text_w(g, label, FONT_UI_BOLD);
    float icon_w = ico ? 15.0f * app->dpi : 0.0f;
    float total = tw + icon_w;
    float x = r.x + (r.w - total) * 0.5f;
    if (ico) {
        ico(g, x + 6 * app->dpi, r.y + r.h * 0.5f, 7.5f * app->dpi, c);
        x += icon_w;
    }
    gfx_text(g, x, r.y + r.h * 0.5f, label, FONT_UI_BOLD, c, SS_ALIGN_LEFT, SS_VALIGN_MIDDLE);
    hit_add(app, id, r);
    return down;
}

static void chip(App *app, SsGfx *g, uint32_t id, SsRect r, const char *label, IconFn ico, int on) {
    int hot = is_hot(app, id, r);
    SsPaint p = on ? paint_linv(0, r.y, r.y + r.h, C_ACCENT, C_ACCENT_DEEP)
                 : paint_linv(0, r.y, r.y + r.h, hot ? C_BTN_TOP_HOVER : C_BTN_TOP,
                              hot ? C_BTN_BOTTOM_HOVER : C_BTN_BOTTOM);
    gfx_fill_round(g, r, r.h * 0.5f, p);
    gfx_stroke_round(g, r, r.h * 0.5f, 1.0f,
                     PS(on ? col_with_alpha(C_ACCENT_HI, 200) : C_BTN_BORDER));
    SsColor c = on ? sss_theme()->on_accent : (hot ? C_TEXT : C_TEXT_MUTED);
    float tw = gfx_text_w(g, label, FONT_UI_SMALL);
    float iw = ico ? 16.0f * app->dpi : 0.0f;
    float x = r.x + (r.w - (tw + iw)) * 0.5f;
    if (ico) {
        ico(g, x + 6 * app->dpi, r.y + r.h * 0.5f, r.h * 0.30f, c);
        x += iw;
    }
    gfx_text(g, x, r.y + r.h * 0.5f, label, FONT_UI_SMALL, c, SS_ALIGN_LEFT, SS_VALIGN_MIDDLE);
    hit_add(app, id, r);
}

static void slider(App *app, SsGfx *g, uint32_t id, SsRect track, float t, SsColor accent) {
    int hot = is_hot(app, id, rect_make(track.x - 6, track.y - 10, track.w + 12, track.h + 20));
    (void)hot;
    SsRect line = rect_make(track.x, track.y + track.h * 0.5f - 2, track.w, 4);
    gfx_fill_round(g, line, 2, PS(sss_theme()->track));
    float fillw = line.w * CLAMP(t, 0, 1);
    if (fillw > 2)
        gfx_fill_round(g, rect_make(line.x, line.y, fillw, line.h), 2,
                       paint_linv(0, line.y, line.y + line.h, col_with_alpha(accent, 220), accent));
    float kx = line.x + fillw;
    gfx_fill_circle(g, kx, line.y + line.h * 0.5f, 7.0f * app->dpi,
                    PS(col_with_alpha(RGB_A(0, 0, 0), 90)));
    gfx_fill_circle(g, kx, line.y + line.h * 0.5f, 5.4f * app->dpi,
                    paint_linv(0, track.y, track.y + track.h, sss_theme()->knob,
                               col_blend(sss_theme()->knob, C_ACCENT, 0.35f)));
    gfx_stroke_circle(g, kx, line.y + line.h * 0.5f, 5.4f * app->dpi, 1.0f, PS(col_with_alpha(accent, 200)));
    hit_add(app, id, rect_make(track.x - 8, track.y - 6, track.w + 16, track.h + 12));
}


/* ------------------------------------------------------------- formatting */

static void fmt_date(double days, char *buf, int cap, int with_time) {
    int y, mo, d, hh, mm, ss;
    jd_to_date(days, &y, &mo, &d, &hh, &mm, &ss);
    static const char *MON[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN",
                                "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};
    if (with_time) snprintf(buf, cap, "%02d %s %d  %02d:%02d", d, MON[(mo - 1) % 12], y, hh, mm);
    else snprintf(buf, cap, "%02d %s %d", d, MON[(mo - 1) % 12], y);
}

static void fmt_speed(double dps, char *buf, int cap) {
    if (dps < 1.0) snprintf(buf, cap, "%.1f h/s", dps * 24.0);
    else if (dps < 45.0) snprintf(buf, cap, "%.2f day/s", dps);
    else if (dps < 700.0) snprintf(buf, cap, "%.0f day/s", dps);
    else snprintf(buf, cap, "%.2f yr/s", dps / 365.25);
}

static void fmt_mass(double kg, char *buf, int cap) {
    if (kg >= 1e30) snprintf(buf, cap, "%.2fe30 kg", kg / 1e30);
    else if (kg >= 1e27) snprintf(buf, cap, "%.2fe27 kg", kg / 1e27);
    else if (kg >= 1e24) snprintf(buf, cap, "%.2fe24 kg", kg / 1e24);
    else if (kg >= 1e21) snprintf(buf, cap, "%.2fe21 kg", kg / 1e21);
    else if (kg >= 1e18) snprintf(buf, cap, "%.2fe18 kg", kg / 1e18);
    else snprintf(buf, cap, "%.0f kg", kg);
}

static void fmt_radius(double km, char *buf, int cap) {
    if (km >= 100000.0) snprintf(buf, cap, "%.0f,000 km", km / 1000.0);
    else if (km >= 1000.0) snprintf(buf, cap, "%.0f km", km);
    else snprintf(buf, cap, "%.1f km", km);
}

/* ------------------------------------------------------------ title bar */

void ui_draw_caption(App *app, SsGfx *g, SsRect r) {
    gfx_fill_rect(g, r, paint_linv(0, r.y, r.y + r.h, C_TITLEBAR_TOP, C_TITLEBAR_BOTTOM));
    gfx_fill_rect(g, rect_make(r.x, r.y, r.w, 1), PS(col_with_alpha(C_TITLE_RGB, 30)));
    gfx_fill_rect(g, rect_make(r.x, r.y + r.h - 1, r.w, 1), PS(C_TITLEBAR_LINE));
    /* soft glass sheen */
    gfx_fill_rect(g, rect_make(r.x, r.y + 1, r.w, r.h * 0.42f),
                  paint_linv(0, r.y, r.y + r.h * 0.45f, sss_theme()->titlebar_sheen,
                             col_with_alpha(sss_theme()->titlebar_sheen, 0)));

    float y = r.y + r.h * 0.5f;
    ico_planet(g, r.x + 20 * app->dpi, y, 9.0f * app->dpi, C_ACCENT_HI);
    gfx_text(g, r.x + 36 * app->dpi, y, "Solar System Sandbox", FONT_UI_BOLD, C_TITLE_RGB,
             SS_ALIGN_LEFT, SS_VALIGN_MIDDLE);
    float tw = gfx_text_w(g, "Solar System Sandbox", FONT_UI_BOLD);
    char sub[96];
    snprintf(sub, sizeof(sub), "SIMULATOR  \xC2\xB7  %s", sss_theme()->name);
    for (char *c = sub; *c; c++)
        if (*c >= 'a' && *c <= 'z') *c = (char)(*c - 32);
    gfx_text(g, r.x + 36 * app->dpi + tw + 10 * app->dpi, y + 1, sub, FONT_UI_TINY,
             C_TITLE_SUB, SS_ALIGN_LEFT, SS_VALIGN_MIDDLE);

    /* window buttons */
    float bw = 48.0f * app->dpi;
    SsRect btn_max = rect_make(r.x + r.w - bw * 2, r.y, bw, r.h);
    SsRect btn_min = rect_make(r.x + r.w - bw * 3, r.y, bw, r.h);
    SsRect btn_close = rect_make(r.x + r.w - bw, r.y, bw, r.h);
    int hot_min = is_hot(app, W_MIN, btn_min);
    int hot_max = is_hot(app, W_MAX, btn_max);
    int hot_close = is_hot(app, W_CLOSE, btn_close);
    if (hot_min) gfx_fill_rect(g, btn_min, PS(col_with_alpha(C_TITLE_RGB, 18)));
    if (hot_max) gfx_fill_rect(g, btn_max, PS(col_with_alpha(C_TITLE_RGB, 18)));
    if (hot_close) {
        gfx_fill_rect(g, btn_close, paint_linv(0, btn_close.y, btn_close.y + btn_close.h,
                                              C_CLOSE_HOVER, C_CLOSE_DOWN));
    }
    SsColor wc = C_TITLE_RGB;
    float s = 5.0f * app->dpi;
    gfx_line(g, btn_min.x + bw * 0.5f - s, y, btn_min.x + bw * 0.5f + s, y, 1.0f, PS(wc));
    gfx_stroke_rect(g, rect_make(btn_max.x + bw * 0.5f - s, y - s, s * 2, s * 2), 1.0f, PS(wc));
    gfx_line(g, btn_close.x + bw * 0.5f - s, y - s, btn_close.x + bw * 0.5f + s, y + s, 1.3f, PS(wc));
    gfx_line(g, btn_close.x + bw * 0.5f + s, y - s, btn_close.x + bw * 0.5f - s, y + s, 1.3f, PS(wc));
    hit_add(app, W_MIN, btn_min);
    hit_add(app, W_MAX, btn_max);
    hit_add(app, W_CLOSE, btn_close);
}

/* ---------------------------------------------------------- transport bar */

static void draw_transport(App *app, SsGfx *g, SsRect r) {
    UI *ui = &app->ui;
    Sim *s = &app->sim;
    float d = app->dpi;
    gfx_fill_rect(g, r, paint_linv(0, r.y, r.y + r.h, C_TRANSPORT_TOP, C_TRANSPORT_BOTTOM));
    gfx_fill_rect(g, rect_make(r.x, r.y + r.h - 1, r.w, 1), PS(C_TRANSPORT_LINE));

    float cy = r.y + r.h * 0.5f;
    float x = r.x + 14 * d;

    /* play / pause */
    SsRect play = rect_make(x, cy - 16 * d, 34 * d, 32 * d);
    {
        int hot = is_hot(app, W_PLAY, play);
        gfx_fill_round(g, play, 6 * d, paint_linv(0, play.y, play.y + play.h,
                                                  hot ? col_blend(C_PLAY_TOP, RGB_A(255, 255, 255), 0.16f)
                                                      : C_PLAY_TOP,
                                                  hot ? col_blend(C_PLAY_BOTTOM, RGB_A(255, 255, 255), 0.10f)
                                                      : C_PLAY_BOTTOM));
        gfx_stroke_round(g, play, 6 * d, 1.0f, PS(C_PLAY_BORDER));
        SsColor glyph = sss_theme()->play_icon;
        if (s->paused) ico_play(g, play.x + play.w * 0.53f, cy, 13 * d, glyph);
        else ico_pause(g, play.x + play.w * 0.5f, cy, 13 * d, glyph);
        hit_add(app, W_PLAY, play);
    }
    x += 34 * d + 18 * d;

    /* speed */
    float slider_w = 150.0f * d;
    if (app->width < 1120) slider_w = 110.0f * d;
    section_header(g, x, r.y + 9 * d, "SIMULATION SPEED");
    SsRect track = rect_make(x, r.y + 26 * d, slider_w, 14 * d);
    float t = (float)(log(s->speed / 0.02) / log(3650.0 / 0.02));
    slider(app, g, W_SPEED, track, t, C_ACCENT);
    char buf[64];
    fmt_speed(s->speed, buf, sizeof(buf));
    gfx_text(g, x + slider_w + 12 * d, cy + 2 * d, buf, FONT_MONO, C_TEXT, SS_ALIGN_LEFT, SS_VALIGN_MIDDLE);
    x += slider_w + 12 * d + gfx_text_w(g, "000.00 day/s", FONT_MONO) + 20 * d;

    gfx_line(g, x, r.y + 12 * d, x, r.y + r.h - 12 * d, 1.0f, PS(C_DIVIDER));
    x += 18 * d;

    /* date */
    section_header(g, x, r.y + 9 * d, "EPOCH (UTC)");
    fmt_date(s->t, buf, sizeof(buf), 1);
    gfx_text(g, x, r.y + 26 * d, buf, FONT_MONO, C_TEXT, SS_ALIGN_LEFT, SS_VALIGN_TOP);
    float date_w = gfx_text_w(g, buf, FONT_MONO);
    SsRect now_btn = rect_make(x + date_w + 12 * d, r.y + 22 * d, 46 * d, 22 * d);
    if (app->width > 1080) {
        button_text(app, g, W_NOW, now_btn, "NOW", 2, NULL);
    } else {
        now_btn.w = 0;
    }
    x += date_w + (app->width > 1080 ? 46 * d + 24 * d : 26 * d);

    gfx_line(g, x, r.y + 12 * d, x, r.y + r.h - 12 * d, 1.0f, PS(C_DIVIDER));
    x += 18 * d;

    /* scenario dropdown */
    section_header(g, x, r.y + 9 * d, "SCENARIO");
    const char *pname = sim_preset_name(s->preset);
    float pw = 168.0f * d;
    SsRect preset = rect_make(x, r.y + 24 * d, pw, 24 * d);
    {
        int hot = is_hot(app, W_PRESET, preset);
        draw_button_base(g, preset, RADIUS_BUTTON, 0, hot, ui->preset_open);
        gfx_text(g, preset.x + 10 * d, preset.y + preset.h * 0.5f, pname, FONT_UI_SMALL, C_TEXT,
                 SS_ALIGN_LEFT, SS_VALIGN_MIDDLE);
        ico_chevron(g, preset.x + preset.w - 14 * d, preset.y + preset.h * 0.5f, 8 * d, C_TEXT_DIM);
        hit_add(app, W_PRESET, preset);
    }
    x += pw + 18 * d;

    /* right side */
    float rx = r.x + r.w - 14 * d;
    SsRect close_help = rect_make(rx - 28 * d, cy - 14 * d, 28 * d, 28 * d);
    button_icon(app, g, W_HELP, close_help, ico_help, "Help", 2);
    rx -= 34 * d;
    {
        SsRect theme_btn = rect_make(rx - 28 * d, cy - 14 * d, 28 * d, 28 * d);
        if (ui->theme_open)
            gfx_fill_round(g, theme_btn, 4 * d, PS(col_with_alpha(C_ACCENT, 90)));
        button_icon(app, g, W_THEME, theme_btn, ico_theme, "Theme (F3)", 2);
        rx -= 30 * d;
    }
    if (app->width > 1000) {
        SsRect load = rect_make(rx - 28 * d, cy - 14 * d, 28 * d, 28 * d);
        button_icon(app, g, W_LOAD, load, ico_load, "Load", 2);
        rx -= 30 * d;
        SsRect save = rect_make(rx - 28 * d, cy - 14 * d, 28 * d, 28 * d);
        button_icon(app, g, W_SAVE, save, ico_save, "Save", 2);
        rx -= 34 * d;
    }
    if (app->width > 880) {
        char gbuf[40];
        snprintf(gbuf, sizeof(gbuf), "Gravity %s", s->nbody ? "on" : "off");
        float gw = gfx_text_w(g, gbuf, FONT_UI_SMALL) + 40 * d;
        SsRect gchip = rect_make(rx - gw, cy - 13 * d, gw, 26 * d);
        chip(app, g, W_NBODY, gchip, gbuf, ico_gravity, s->nbody);
        rx -= gw + 12 * d;
    }
    (void)rx;
}

/* ------------------------------------------------------------- inspector */

static void stat_card(SsGfx *g, App *app, SsRect r, const char *label, const char *value,
                      const char *sub, SsColor accent) {
    (void)accent;
    gfx_fill_round(g, r, 6 * app->dpi, paint_linv(0, r.y, r.y + r.h, C_CARD_TOP, C_CARD_BOTTOM));
    gfx_stroke_round(g, r, 6 * app->dpi, 1.0f, PS(C_CARD_BORDER));
    text_tracked(g, r.x + 9 * app->dpi, r.y + 7 * app->dpi, label, FONT_UI_TINY,
                 C_TEXT_MUTED, 0.4f, SS_ALIGN_LEFT, SS_VALIGN_TOP);
    gfx_text(g, r.x + 9 * app->dpi, r.y + 20 * app->dpi, value, FONT_MONO, C_TEXT,
             SS_ALIGN_LEFT, SS_VALIGN_TOP);
    if (sub && sub[0])
        gfx_text(g, r.x + 9 * app->dpi, r.y + 35 * app->dpi, sub, FONT_UI_TINY, C_TEXT_FAINT,
                 SS_ALIGN_LEFT, SS_VALIGN_TOP);
}

static void draw_scenario_list(App *app, SsGfx *g, SsRect area);
static void draw_library_tab(App *app, SsGfx *g, SsRect area);

static void draw_inspector(App *app, SsGfx *g, SsRect r) {
    UI *ui = &app->ui;
    Sim *s = &app->sim;
    float d = app->dpi;

    gfx_fill_rect(g, r, paint_linv(0, r.y, r.y + r.h, C_PANEL_TOP, C_PANEL_BOTTOM));
    gfx_fill_rect(g, rect_make(r.x, r.y, 1, r.h), PS(C_DIVIDER));

    SsRect inner = rect_insets(r, (Insets){16 * d, 14 * d, 16 * d, 10 * d});

    /* tabs */
    SsRect tab_bar = rect_make(inner.x, inner.y, inner.w, 30 * d);
    gfx_fill_round(g, tab_bar, 6 * d, PS(col_with_alpha(C_BTN_BOTTOM, 70)));
    float tab_w = (tab_bar.w - 6 * d) / 3.0f;
    SsRect t1 = rect_make(tab_bar.x + 2 * d, tab_bar.y + 2 * d, tab_w, tab_bar.h - 4 * d);
    SsRect t2 = rect_make(t1.x + t1.w + 2 * d, t1.y, t1.w, t1.h);
    SsRect t3 = rect_make(t2.x + t2.w + 2 * d, t1.y, t1.w, t1.h);
    for (int i = 0; i < 3; i++) {
        SsRect t = i == 0 ? t1 : (i == 1 ? t2 : t3);
        int active = ui->tab == i;
        uint32_t id = i == 0 ? W_TAB_INSPECT : (i == 1 ? W_TAB_CREATE : W_TAB_LIBRARY);
        int hot = is_hot(app, id, t);
        if (active) {
            gfx_fill_round(g, t, 5 * d, paint_linv(0, t.y, t.y + t.h, C_ACCENT, C_ACCENT_DEEP));
            gfx_stroke_round(g, t, 5 * d, 1.0f, PS(col_with_alpha(C_ACCENT_HI, 120)));
        } else if (hot) {
            gfx_fill_round(g, t, 5 * d, PS(sss_theme()->ghost_hot));
        }
        static const char *TAB_LABEL[3] = {"INSPECT", "CREATE", "LIBRARY"};
        gfx_text(g, t.x + t.w * 0.5f, t.y + t.h * 0.5f, TAB_LABEL[i], FONT_UI_TINY,
                 active ? sss_theme()->on_accent : C_TEXT_DIM, SS_ALIGN_CENTER,
                 SS_VALIGN_MIDDLE);
        hit_add(app, id, t);
    }
    SsRect body_area = rect_make(inner.x, tab_bar.y + tab_bar.h + 14 * d, inner.w,
                               inner.y + inner.h - (tab_bar.y + tab_bar.h + 14 * d));

    if (ui->tab == 0) {
        int idx = sim_find(s, ui->selected_id);
        if (idx < 0) {
            /* pick the most interesting body as a fallback */
            for (int i = 0; i < s->count; i++)
                if (s->bodies[i].alive && s->bodies[i].kind == BT_TERRESTRIAL) { idx = i; break; }
            if (idx < 0)
                for (int i = 0; i < s->count; i++)
                    if (s->bodies[i].alive) { idx = i; break; }
        }
        if (idx < 0) {
            gfx_text(g, body_area.x, body_area.y + 20, "No body selected", FONT_UI, C_TEXT_DIM,
                     SS_ALIGN_LEFT, SS_VALIGN_TOP);
            return;
        }
        Body *b = &s->bodies[idx];
        float y = body_area.y;

        /* color dot + name */
        gfx_fill_circle(g, body_area.x + 7 * d, y + 13 * d, 5 * d, PS(b->accent));
        gfx_fill_circle(g, body_area.x + 7 * d, y + 13 * d, 8 * d, PS(col_with_alpha(b->accent, 40)));
        gfx_text(g, body_area.x + 22 * d, y, b->name, FONT_H1, C_TEXT, SS_ALIGN_LEFT, SS_VALIGN_TOP);
        gfx_text(g, body_area.x + 22 * d, y + 28 * d, sim_kind_name(b->kind), FONT_UI_SMALL,
                 C_TEXT_DIM, SS_ALIGN_LEFT, SS_VALIGN_TOP);
        y += 50 * d;

        char dist[40], sub[40], period[40], daybuf[40], radius[40], mass[44], mass_sub[40];
        double dist_au = body_distance_to_parent(b, s);
        if (b->kind == BT_STAR) {
            snprintf(dist, sizeof(dist), "-");
            snprintf(sub, sizeof(sub), "barycenter");
        } else if (dist_au * KM_PER_AU > 1e7)
            snprintf(dist, sizeof(dist), "%.3f AU", dist_au);
        else if (dist_au * KM_PER_AU > 100000)
            snprintf(dist, sizeof(dist), "%.2fM km", dist_au * KM_PER_AU / 1e6);
        else
            snprintf(dist, sizeof(dist), "%.0fk km", dist_au * KM_PER_AU / 1000.0);
        if (b->kind != BT_STAR)
            snprintf(sub, sizeof(sub), "%s", b->parent >= 0 ? "from parent" : "from the Sun");
        double per = body_orbit_period(b, s);
        if (per > 0) fmt_hm(per * 24.0, period, sizeof(period));
        else snprintf(period, sizeof(period), "unbound");
        if (b->day_hours > 0) fmt_hm(b->day_hours, daybuf, sizeof(daybuf));
        else snprintf(daybuf, sizeof(daybuf), "-");
        fmt_radius(b->radius_km, radius, sizeof(radius));
        fmt_mass(b->mass_kg, mass, sizeof(mass));
        double earths = b->mass_kg / 5.97237e24;
        if (earths >= 0.01) snprintf(mass_sub, sizeof(mass_sub), "%.2fx Earth", earths);
        else snprintf(mass_sub, sizeof(mass_sub), "tiny body");

        float cw = (body_area.w - 8 * d) * 0.5f;
        float ch = 52 * d;
        stat_card(g, app, rect_make(body_area.x, y, cw, ch), "DISTANCE", dist, sub, C_ACCENT);
        stat_card(g, app, rect_make(body_area.x + cw + 8 * d, y, cw, ch), "ORBITAL PERIOD", period,
                  "one revolution", C_AMBER);
        y += ch + 8 * d;
        stat_card(g, app, rect_make(body_area.x, y, cw, ch), "ROTATION", daybuf, "one local day",
                  C_GREEN);
        stat_card(g, app, rect_make(body_area.x + cw + 8 * d, y, cw, ch), "RADIUS", radius, mass,
                  C_PURPLE);
        y += ch + 14 * d;

        if (b->fact && b->fact[0]) {
            SsRect fact_box = rect_make(body_area.x, y, body_area.w, 46 * d);
            gfx_fill_round(g, fact_box, 5 * d, PS(RGBA(255, 255, 255, 6)));
            gfx_fill_round(g, rect_make(fact_box.x, fact_box.y, 2.5f * d, fact_box.h), 1.2f,
                           PS(col_with_alpha(b->accent, 200)));
            wrap_text(g, rect_inset(fact_box, 10 * d), b->fact, FONT_UI_SMALL, C_TEXT_DIM, 15 * d);
            y += fact_box.h + 14 * d;
        }

        /* actions */
        float bw = (body_area.w - 8 * d) * 0.5f;
        SsRect focus = rect_make(body_area.x, y, bw, 30 * d);
        SsRect trackb = rect_make(body_area.x + bw + 8 * d, y, bw, 30 * d);
        button_text(app, g, W_FOCUS, focus, "Focus", 1, ico_target);
        button_text(app, g, W_TRACK, trackb, ui->follow_id == b->id ? "Following" : "Follow", 0,
                    ico_orbit);
        y += 36 * d;
        SsRect dyn = rect_make(body_area.x, y, bw, 28 * d);
        int is_dyn = b->dynamic || (!b->on_rail && b->parent < 0);
        button_text(app, g, W_DYNAMIC, dyn, is_dyn ? "Return to orbit" : "Release to gravity", 0,
                    ico_gravity);
        SsRect del = rect_make(body_area.x + bw + 8 * d, y, bw, 28 * d);
        button_text(app, g, W_DELETE, del, "Remove", 3, ico_delete);
        y += 42 * d;

        /* moons list */
        if (b->parent < 0) {
            int others = 0;
            for (int i = 0; i < s->count; i++) {
                if (!s->bodies[i].alive || s->bodies[i].parent != idx) continue;
                others++;
            }
            if (others > 0 && y + 20 * d < body_area.y + body_area.h - 30 * d) {
                char mbuf[48];
                snprintf(mbuf, sizeof(mbuf), "%d %s", others, others == 1 ? "moon" : "moons");
                section_header(g, body_area.x, y, "SATELLITES");
                gfx_text(g, body_area.x + body_area.w, y - 1, mbuf, FONT_UI_SMALL, C_TEXT_DIM,
                         SS_ALIGN_RIGHT, SS_VALIGN_TOP);
                y += 20 * d;
                int shown = 0;
                for (int i = 0; i < s->count && shown < 4; i++) {
                    Body *m = &s->bodies[i];
                    if (!m->alive || m->parent != idx) continue;
                    SsRect row = rect_make(body_area.x, y, body_area.w, 24 * d);
                    int hot = is_hot(app, W_BODY_ROW_0 + i, row);
                    if (hot) gfx_fill_round(g, row, 4 * d, PS(RGBA(255, 255, 255, 12)));
                    gfx_fill_circle(g, row.x + 7 * d, row.y + row.h * 0.5f, 3.0f * d, PS(m->accent));
                    gfx_text(g, row.x + 18 * d, row.y + row.h * 0.5f, m->name, FONT_UI_SMALL,
                             C_TEXT_DIM, SS_ALIGN_LEFT, SS_VALIGN_MIDDLE);
                    char rb[32];
                    snprintf(rb, sizeof(rb), "%.0fk km", m->radius_km);
                    gfx_text(g, row.x + row.w, row.y + row.h * 0.5f, rb, FONT_MONO, C_TEXT_FAINT,
                             SS_ALIGN_RIGHT, SS_VALIGN_MIDDLE);
                    hit_add(app, W_BODY_ROW_0 + i, row);
                    y += 22 * d;
                    shown++;
                }
                y += 8 * d;
            }
        }
        /* quick actions fill any remaining space */
        float qy = body_area.y + body_area.h - 96 * d;
        if (qy > y + 12 * d) y = qy;
        if (y + 88 * d <= body_area.y + body_area.h) {
            gfx_line(g, body_area.x, y, body_area.x + body_area.w, y, 1.0f, PS(RGBA(255, 255, 255, 16)));
            y += 14 * d;
            section_header(g, body_area.x, y, "QUICK ACTIONS");
            y += 18 * d;
            static const char *QA[] = {"Spawn asteroid", "Spawn comet", "Clear trails", "Empty sky"};
            float qw = (body_area.w - 8 * d) * 0.5f;
            for (int i = 0; i < 4; i++) {
                SsRect q = rect_make(body_area.x + (i % 2) * (qw + 8 * d), y + (i / 2) * 30 * d, qw,
                                   26 * d);
                button_text(app, g, W_QUICK_0 + i, q, QA[i], 0, NULL);
            }
        }
    } else if (ui->tab == 1) {
        /* create body */
        float y = body_area.y;
        section_header(g, body_area.x, y, "BODY TYPE");
        y += 18 * d;
        static const char *KIND_LABEL[] = {"Star", "Terrestrial", "Gas giant", "Ice giant",
                                           "Dwarf", "Asteroid", "Comet", "Black hole"};
        IconFn KIND_ICON[] = {ico_star, ico_planet, ico_planet, ico_planet, ico_planet,
                              ico_asteroid, ico_comet, ico_blackhole};
        float chip_w = (body_area.w - 6 * d) * 0.5f;
        for (int i = 0; i < 8; i++) {
            int row = i / 2, col = i % 2;
            SsRect rc = rect_make(body_area.x + col * (chip_w + 6 * d), y + row * (28 * d),
                                chip_w, 24 * d);
            chip(app, g, W_KIND_0 + i, rc, KIND_LABEL[i], KIND_ICON[i], ui->add_kind == i);
        }
        y += 4 * (28 * d) + 6 * d;

        char vbuf[64], sub[64];
        section_header(g, body_area.x, y, "MASS");
        double mass = pow(10.0, ui->add_mass_log);
        fmt_mass(mass, vbuf, sizeof(vbuf));
        snprintf(sub, sizeof(sub), "%.2fx Earth", mass / 5.97237e24);
        gfx_text(g, body_area.x + body_area.w, y - 1, sub, FONT_UI_SMALL, C_TEXT_DIM,
                 SS_ALIGN_RIGHT, SS_VALIGN_TOP);
        y += 16 * d;
        float t = (ui->add_mass_log - 18.0f) / (31.0f - 18.0f);
        slider(app, g, W_MASS, rect_make(body_area.x, y, body_area.w, 16 * d), t, C_AMBER);
        gfx_text(g, body_area.x, y + 19 * d, vbuf, FONT_MONO, C_TEXT_DIM, SS_ALIGN_LEFT, SS_VALIGN_TOP);
        y += 40 * d;

        section_header(g, body_area.x, y, "RADIUS");
        double rad = pow(10.0, ui->add_radius_log);
        fmt_radius(rad, vbuf, sizeof(vbuf));
        gfx_text(g, body_area.x + body_area.w, y - 1, vbuf, FONT_UI_SMALL, C_TEXT_DIM,
                 SS_ALIGN_RIGHT, SS_VALIGN_TOP);
        y += 16 * d;
        t = (ui->add_radius_log + 1.0f) / 7.0f;
        slider(app, g, W_RADIUS, rect_make(body_area.x, y, body_area.w, 16 * d), t, C_GREEN);
        y += 30 * d;

        section_header(g, body_area.x, y, "ORBIT RADIUS");
        double au = pow(10.0, ui->add_orbit_log);
        snprintf(vbuf, sizeof(vbuf), "%.2f AU", au);
        double per = 0;
        int central = -1;
        double best = 0;
        for (int i = 0; i < s->count; i++) {
            if (!s->bodies[i].alive) continue;
            if (s->bodies[i].kind == BT_STAR && s->bodies[i].mass_kg > best) {
                best = s->bodies[i].mass_kg;
                central = i;
            }
        }
        if (central >= 0) per = TAU * sqrt(au * au * au / (G_AU_D2_PER_KG * best));
        gfx_text(g, body_area.x + body_area.w, y - 1, vbuf, FONT_UI_SMALL, C_TEXT_DIM,
                 SS_ALIGN_RIGHT, SS_VALIGN_TOP);
        y += 16 * d;
        t = (ui->add_orbit_log + 0.3f) / 1.9f;
        slider(app, g, W_ORBIT, rect_make(body_area.x, y, body_area.w, 16 * d), t, C_ACCENT);
        if (per > 0) {
            char pb[48];
            fmt_hm(per * 24.0, pb, sizeof(pb));
            gfx_text(g, body_area.x, y + 14 * d, "Orbital period", FONT_UI_TINY, C_TEXT_MUTED,
                     SS_ALIGN_LEFT, SS_VALIGN_TOP);
            gfx_text(g, body_area.x + body_area.w, y + 14 * d, pb, FONT_MONO, C_TEXT_DIM,
                     SS_ALIGN_RIGHT, SS_VALIGN_TOP);
        }
        y += 38 * d;

        SsRect add = rect_make(body_area.x, y, body_area.w, 36 * d);
        button_text(app, g, W_ADD, add, "Add to sandbox", 1, ico_plus);
        y += 46 * d;
        wrap_text(g, rect_make(body_area.x, y, body_area.w, 40 * d),
                  "The new body is integrated live under gravity while the planets stay on their "
                  "accurate ephemerides.", FONT_UI_TINY, C_TEXT_FAINT, 14 * d);
        y += 34 * d;
        if (y + 40 * d < body_area.y + body_area.h) {
            gfx_line(g, body_area.x, y, body_area.x + body_area.w, y, 1.0f, PS(RGBA(255, 255, 255, 16)));
            y += 12 * d;
            draw_scenario_list(app, g, rect_make(body_area.x, y, body_area.w,
                                                 body_area.y + body_area.h - y));
        }
    } else if (ui->tab == 2) {
        draw_library_tab(app, g, body_area);
    }
}


/* ---------------------------------------------------------------- library */

static void lib_section_head(App *app, SsGfx *g, SsRect row, SsRect clip_rect, const char *title,
                             int folder) {
    float d = app->dpi;
    section_header(g, row.x, row.y + 4 * d, title);
    if (folder >= 0) {
        SsRect b = rect_make(row.x + row.w - 22 * d, row.y, 22 * d, 19 * d);
        int hot = is_hot(app, (uint32_t)(W_FOLDER_0 + folder), b);
        gfx_fill_round(g, b, 4 * d, PS(hot ? RGBA(255, 255, 255, 30) : RGBA(255, 255, 255, 8)));
        ico_folder(g, b.x + b.w * 0.5f, b.y + b.h * 0.5f, 9 * d, hot ? C_TEXT : C_TEXT_DIM);
        if (b.y >= clip_rect.y && b.y + b.h <= clip_rect.y + clip_rect.h)
            hit_add(app, (uint32_t)(W_FOLDER_0 + folder), b);
    }
}

static void lib_empty(App *app, SsGfx *g, SsRect row, const char *hint) {
    gfx_text_elided(g, row, hint, FONT_UI_TINY, C_TEXT_FAINT, SS_ALIGN_LEFT, SS_VALIGN_MIDDLE);
    (void)app;
}

static void draw_library_tab(App *app, SsGfx *g, SsRect area) {
    UI *ui = &app->ui;
    float d = app->dpi;
    static const char *KIND_LABEL_LIB[] = {"star", "terrestrial", "gas giant", "ice giant",
                                           "dwarf", "moon", "asteroid", "comet", "black hole"};

    ui->library_area = area;
    char buf[128];
    snprintf(buf, sizeof(buf), "%d PARTS   %d PACKS   %d MODS", content_part_count(),
             content_pack_count(), content_mod_count());
    gfx_text(g, area.x, area.y + 2 * d, buf, FONT_UI_TINY, C_HEADER, SS_ALIGN_LEFT,
             SS_VALIGN_TOP);
    float bw = (area.w - 6 * d) * 0.5f;
    SsRect rescan = rect_make(area.x, area.y + 18 * d, bw, 22 * d);
    SsRect folders = rect_make(area.x + bw + 6 * d, rescan.y, bw, 22 * d);
    button_text(app, g, W_RESCAN, rescan, "RESCAN", 2, ico_reset);
    button_text(app, g, W_OPEN_CONTENT, folders, "FOLDERS", 2, ico_folder);

    SsRect content = rect_make(area.x, area.y + 46 * d, area.w,
                               area.y + area.h - (area.y + 46 * d));
    ui->library_area = content;
    if (content.h < 40 * d) return;

    if (ui->library_scroll < 0) ui->library_scroll = 0;
    gfx_push_clip(g, content);
    float y = content.y - ui->library_scroll;
    float start = y;

    /* -------- parts */
    lib_section_head(app, g, rect_make(content.x, y, content.w, 20 * d), content, "PARTS",
                     CF_PARTS);
    y += 22 * d;
    if (content_part_count() == 0) {
        lib_empty(app, g, rect_make(content.x, y, content.w, 18 * d),
                  "No parts yet - drop .part files in Parts");
        y += 20 * d;
    }
    for (int i = 0; i < content_part_count(); i++) {
        const ContentPart *part = content_part(i);
        if (!part) continue;
        SsRect row = rect_make(content.x, y, content.w, 33 * d);
        int hot = is_hot(app, (uint32_t)(W_PART_0 + i), row);
        if (hot) gfx_fill_round(g, row, 4 * d, PS(RGBA(255, 255, 255, 12)));
        gfx_fill_circle(g, row.x + 8 * d, row.y + 11 * d, 5 * d, PS(part->col1));
        gfx_fill_circle(g, row.x + 8 * d, row.y + 11 * d, 7 * d, PS(col_with_alpha(part->col1, 46)));
        gfx_text_elided(g, rect_make(row.x + 20 * d, row.y + 3 * d, row.w - 48 * d, 14 * d),
                        part->name, FONT_UI_SMALL, C_TEXT, SS_ALIGN_LEFT, SS_VALIGN_TOP);
        int kind = content_kind_from_name(part->kind);
        char sub[80];
        const char *kl = (kind >= 0 && kind < 9) ? KIND_LABEL_LIB[kind] : part->kind;
        if (part->mass_kg > 0) {
            char mass[40];
            fmt_mass(part->mass_kg, mass, sizeof(mass));
            snprintf(sub, sizeof(sub), "%s  %s", kl, mass);
        } else {
            snprintf(sub, sizeof(sub), "%s", kl);
        }
        gfx_text_elided(g, rect_make(row.x + 20 * d, row.y + 18 * d, row.w - 48 * d, 12 * d), sub,
                        FONT_UI_TINY, C_TEXT_DIM, SS_ALIGN_LEFT, SS_VALIGN_TOP);
        gfx_text(g, row.x + row.w - 4 * d, row.y + 18 * d, "USE", FONT_UI_TINY,
                 hot ? C_ACCENT_HI : C_TEXT_MUTED, SS_ALIGN_RIGHT, SS_VALIGN_TOP);
        if (row.y >= content.y && row.y + row.h <= content.y + content.h)
            hit_add(app, (uint32_t)(W_PART_0 + i), row);
        y += 34 * d;
    }

    /* -------- texture packs */
    y += 8 * d;
    lib_section_head(app, g, rect_make(content.x, y, content.w, 20 * d), content, "TEXTURE PACKS",
                     CF_TEXTURE_PACKS);
    y += 22 * d;
    {
        SsRect row = rect_make(content.x, y, content.w, 32 * d);
        int active = app->pack_index < 0;
        int hot = is_hot(app, (uint32_t)W_PACK_0, row);
        if (hot || active)
            gfx_fill_round(g, row, 4 * d, PS(active ? RGBA(70, 128, 214, 70) : RGBA(255, 255, 255, 10)));
        gfx_fill_round(g, rect_make(row.x + 5 * d, row.y + 8 * d, 12 * d, 12 * d), 2 * d,
                       PS(RGBA(120, 150, 200, 160)));
        gfx_text(g, row.x + 23 * d, row.y + 4 * d, "Theme colours", FONT_UI_SMALL,
                 active ? C_TEXT : C_TEXT_DIM, SS_ALIGN_LEFT, SS_VALIGN_TOP);
        gfx_text(g, row.x + 23 * d, row.y + 19 * d, "no recolour", FONT_UI_TINY, C_TEXT_DIM,
                 SS_ALIGN_LEFT, SS_VALIGN_TOP);
        if (row.y >= content.y && row.y + row.h <= content.y + content.h)
            hit_add(app, W_PACK_0, row);
        y += 33 * d;
    }
    for (int i = 0; i < content_pack_count(); i++) {
        const ContentPack *pack = content_pack(i);
        if (!pack) continue;
        SsRect row = rect_make(content.x, y, content.w, 32 * d);
        int active = app->pack_index == i;
        int hot = is_hot(app, (uint32_t)(W_PACK_0 + 1 + i), row);
        if (hot || active)
            gfx_fill_round(g, row, 4 * d, PS(active ? RGBA(70, 128, 214, 70) : RGBA(255, 255, 255, 10)));
        if (active)
            gfx_stroke_round(g, row, 4 * d, 1.0f, PS(col_with_alpha(C_ACCENT_HI, 150)));
        gfx_fill_round(g, rect_make(row.x + 5 * d, row.y + 8 * d, 12 * d, 12 * d), 2 * d,
                       PS(pack->tint));
        gfx_text_elided(g, rect_make(row.x + 23 * d, row.y + 4 * d, row.w - 46 * d, 14 * d),
                        pack->name, FONT_UI_SMALL, active ? C_TEXT : C_TEXT_DIM, SS_ALIGN_LEFT,
                        SS_VALIGN_TOP);
        char sub[128];
        snprintf(sub, sizeof(sub), "%s  %d%%", pack->note[0] ? pack->note : "recolour",
                 pack->strength);
        gfx_text_elided(g, rect_make(row.x + 23 * d, row.y + 19 * d, row.w - 46 * d, 11 * d), sub,
                        FONT_UI_TINY, C_TEXT_DIM, SS_ALIGN_LEFT, SS_VALIGN_TOP);
        if (active)
            gfx_text(g, row.x + row.w - 5 * d, row.y + 9 * d, "ON", FONT_UI_TINY, C_ACCENT_HI,
                     SS_ALIGN_RIGHT, SS_VALIGN_TOP);
        if (row.y >= content.y && row.y + row.h <= content.y + content.h)
            hit_add(app, (uint32_t)(W_PACK_0 + 1 + i), row);
        y += 33 * d;
    }

    /* -------- mods */
    y += 12 * d;
    lib_section_head(app, g, rect_make(content.x, y, content.w, 20 * d), content, "MODS",
                     CF_MODS);
    y += 22 * d;
    if (content_mod_count() == 0) {
        lib_empty(app, g, rect_make(content.x, y, content.w, 18 * d),
                  "No mods yet - drop .sims files in Mods");
        y += 20 * d;
    }
    for (int i = 0; i < content_mod_count(); i++) {
        const ContentMod *mod = content_mod(i);
        if (!mod) continue;
        SsRect row = rect_make(content.x, y, content.w, 28 * d);
        int hot = is_hot(app, (uint32_t)(W_MOD_0 + i), row);
        if (hot) gfx_fill_round(g, row, 4 * d, PS(RGBA(255, 255, 255, 12)));
        ico_orbit(g, row.x + 9 * d, row.y + 13 * d, 9 * d, hot ? C_ACCENT_HI : C_TEXT_MUTED);
        gfx_text_elided(g, rect_make(row.x + 23 * d, row.y + 3 * d, row.w - 30 * d, 13 * d),
                        mod->name, FONT_UI_SMALL, C_TEXT, SS_ALIGN_LEFT, SS_VALIGN_TOP);
        const char *leaf = strrchr(mod->file, '/');
        if (!leaf) leaf = strrchr(mod->file, '\\');
        gfx_text_elided(g, rect_make(row.x + 23 * d, row.y + 16 * d, row.w - 30 * d, 10 * d),
                        leaf ? leaf + 1 : mod->file, FONT_UI_TINY, C_TEXT_DIM, SS_ALIGN_LEFT,
                        SS_VALIGN_TOP);
        if (row.y >= content.y && row.y + row.h <= content.y + content.h)
            hit_add(app, (uint32_t)(W_MOD_0 + i), row);
        y += 29 * d;
    }

    /* footer hint */
    y += 10 * d;
    {
        char hint[220];
        snprintf(hint, sizeof(hint),
                 "Folders are scanned beside the app and in %%APPDATA%%\\Solar System Sandbox.");
        wrap_text(g, rect_make(content.x, y, content.w, 40 * d), hint, FONT_UI_TINY, C_TEXT_FAINT,
                  12 * d);
        y += 32 * d;
    }

    ui->library_content_h = y - start;
    gfx_pop_clip(g);

    float max_scroll = ui->library_content_h - content.h;
    if (max_scroll < 0) max_scroll = 0;
    if (ui->library_scroll > max_scroll) ui->library_scroll = max_scroll;
}

/* --------------------------------------------------------------- scenario */

static void draw_scenario_list(App *app, SsGfx *g, SsRect area) {
    UI *ui = &app->ui;
    float d = app->dpi;
    (void)ui;
    section_header(g, area.x, area.y, "SCENARIOS");
    float y = area.y + 18 * d;
    static const char *DESC[PRESET_COUNT] = {
        "All eight planets, dwarf planets, moons and comets",
        "Mercury to Mars with the Moon and Ceres",
        "The two gas giants with their major moons",
        "Uranus, Neptune, Pluto and the Kuiper belt",
        "Two suns, three circumbinary worlds, live gravity",
        "An empty sky with one star: build your own"
    };
    for (int i = 0; i < PRESET_COUNT && y + 30 * d < area.y + area.h; i++) {
        SsRect row = rect_make(area.x, y, area.w, 30 * d);
        int hot = is_hot(app, W_SCENARIO_0 + i, row);
        int active = app->sim.preset == i && app->ui.tab == 1;
        if (hot || active) gfx_fill_round(g, row, 4 * d, PS(active ? RGBA(70, 128, 214, 80)
                                                                    : RGBA(255, 255, 255, 12)));
        gfx_fill_circle(g, row.x + 7 * d, row.y + 10 * d, 2.5f * d, PS(active ? C_ACCENT_HI : C_TEXT_MUTED));
        gfx_text(g, row.x + 18 * d, row.y + 4 * d, sim_preset_name(i), FONT_UI_SMALL,
                 hot || active ? C_TEXT : C_TEXT_DIM, SS_ALIGN_LEFT, SS_VALIGN_TOP);
        gfx_text_elided(g, rect_make(row.x + 18 * d, row.y + 16 * d, row.w - 26 * d, 12 * d),
                        DESC[i], FONT_UI_TINY, C_TEXT_FAINT, SS_ALIGN_LEFT, SS_VALIGN_TOP);
        hit_add(app, W_SCENARIO_0 + i, row);
        y += 30 * d;
    }
    (void)ui;
}

/* ------------------------------------------------------------- status bar */

static void draw_status(App *app, SsGfx *g, SsRect r) {
    float d = app->dpi;
    gfx_fill_rect(g, r, PS(C_TRANSPORT_BOTTOM));
    gfx_fill_rect(g, rect_make(r.x, r.y, r.w, 1), PS(C_TRANSPORT_LINE));
    int total = 0;
    for (int i = 0; i < app->sim.count; i++)
        if (app->sim.bodies[i].alive) total++;
    char buf[128];
    snprintf(buf, sizeof(buf), "%d bodies   %s   %s", total,
             app->sim.nbody ? "gravity sandbox" : "ephemeris rails",
             app->sim.paused ? "paused" : "running");
    gfx_text(g, r.x + 14 * d, r.y + r.h * 0.5f, buf, FONT_UI_TINY, C_TEXT_MUTED, SS_ALIGN_LEFT,
             SS_VALIGN_MIDDLE);
    char fps[64];
    snprintf(fps, sizeof(fps), "%.0f fps   %d x %d", app->fps, app->width, app->height);
    gfx_text(g, r.x + r.w - 14 * d, r.y + r.h * 0.5f, fps, FONT_MONO, C_TEXT_FAINT, SS_ALIGN_RIGHT,
             SS_VALIGN_MIDDLE);
    const char *hint = "SPACE play   LMB pan   RMB orbit   wheel zoom   C controls   F1 help";
    if (app->width > 1150)
        gfx_text(g, r.x + r.w * 0.5f, r.y + r.h * 0.5f, hint, FONT_UI_TINY, C_TEXT_FAINT,
                 SS_ALIGN_CENTER, SS_VALIGN_MIDDLE);
}

/* ---------------------------------------------------------------- overlays */

static void draw_tool_pill(App *app, SsGfx *g, SsRect vp) {
    float d = app->dpi;
    UI *ui = &app->ui;
    struct { uint32_t id; const char *label; IconFn ico; int *flag; } tools[] = {
        {W_ORBITS, "Orbits", ico_orbit, &ui->show_orbits},
        {W_LABELS, "Labels", ico_labels, &ui->show_labels},
        {W_TRAILS, "Trails", ico_trail, &ui->show_trails},
        {W_MOONS, "Moons", ico_moons, &ui->show_moons},
        {W_SCALE, "True scale", ico_ruler, &ui->true_scale},
    };
    int n = (int)ARRAY_LEN(tools);
    float bw = 30 * d;
    float gap = 4 * d;
    float w = n * bw + (n - 1) * gap + 114 * d + 20 * d;
    SsRect pill = rect_make(vp.x + 16 * d, vp.y + vp.h - 16 * d - 38 * d, w, 38 * d);
    gfx_fill_round(g, pill, 8 * d, paint_linv(0, pill.y, pill.y + pill.h,
                                              sss_theme()->overlay_top,
                                              sss_theme()->overlay_bottom));
    gfx_stroke_round(g, pill, 8 * d, 1.0f, PS(sss_theme()->overlay_border));
    float x = pill.x + 8 * d;
    float cy = pill.y + pill.h * 0.5f;
    for (int i = 0; i < n; i++) {
        SsRect b = rect_make(x, cy - 13 * d, bw, 26 * d);
        int hot = is_hot(app, tools[i].id, b);
        int on = *tools[i].flag;
        if (on) gfx_fill_round(g, b, 5 * d, PS(col_with_alpha(C_ACCENT, 120)));
        else if (hot) gfx_fill_round(g, b, 5 * d, PS(sss_theme()->ghost_hot));
        SsColor c = on ? sss_theme()->on_accent : (hot ? C_TEXT : C_TEXT_MUTED);
        tools[i].ico(g, b.x + b.w * 0.5f, b.y + b.h * 0.5f, 8.0f * d, c);
        /* tooltip */
        if (hot) {
            float tw = gfx_text_w(g, tools[i].label, FONT_UI_TINY) + 14 * d;
            SsRect tip = rect_make(b.x + b.w * 0.5f - tw * 0.5f, b.y - 24 * d, tw, 19 * d);
            gfx_fill_round(g, tip, 4 * d, PS(sss_theme()->overlay_bottom));
            gfx_stroke_round(g, tip, 4 * d, 1.0f, PS(sss_theme()->overlay_border));
            gfx_text(g, tip.x + tip.w * 0.5f, tip.y + tip.h * 0.5f, tools[i].label, FONT_UI_TINY,
                     sss_theme()->overlay_text, SS_ALIGN_CENTER, SS_VALIGN_MIDDLE);
        }
        hit_add(app, tools[i].id, b);
        x += bw + gap;
    }
    gfx_line(g, x + 2 * d, pill.y + 9 * d, x + 2 * d, pill.y + pill.h - 9 * d, 1.0f,
             PS(C_DIVIDER));
    x += 12 * d;
    SsRect zin = rect_make(x, cy - 13 * d, 28 * d, 26 * d);
    button_icon(app, g, W_ZIN, zin, ico_plus, "Zoom in", 2);
    x += 30 * d;
    SsRect zout = rect_make(x, cy - 13 * d, 28 * d, 26 * d);
    button_icon(app, g, W_ZOUT, zout, ico_minus, "Zoom out", 2);
    x += 30 * d;
    SsRect reset = rect_make(x, cy - 13 * d, 28 * d, 26 * d);
    button_icon(app, g, W_RESETVIEW, reset, ico_reset, "Reset view", 2);
    x += 30 * d;
    SsRect ctl = rect_make(x, cy - 13 * d, 28 * d, 26 * d);
    if (ui->panel_open) gfx_fill_round(g, ctl, 4 * d, PS(col_with_alpha(C_ACCENT, 120)));
    button_icon(app, g, W_SIM, ctl, ico_sliders, "Simulation controls (C)", 2);
}

static void draw_preset_menu(App *app, SsGfx *g) {
    UI *ui = &app->ui;
    float d = app->dpi;
    if (!ui->preset_open) return;
    float w = 300 * d;
    float item_h = 36 * d;
    float h = 8 * d + PRESET_COUNT * item_h + 8 * d;
    SsRect send = rect_make(app->ui.transport.x + app->ui.transport.w - w - 220 * d,
                          app->ui.transport.y + app->ui.transport.h + 4 * d, w, h);
    /* anchor under the scenario button when possible */
    SsRect preg = hit_rect_of(app, W_PRESET);
    if (preg.w > 0) send.x = preg.x;
    if (send.x + send.w > app->width - 10 * d) send.x = app->width - 10 * d - send.w;
    if (send.x < 10 * d) send.x = 10 * d;
    /* drop shadow */
    gfx_fill_round(g, rect_offset(send, 0, 4 * d), 8 * d, PS(RGBA(0, 0, 0, 90)));
    gfx_fill_round(g, send, 8 * d, paint_linv(0, send.y, send.y + send.h, C_PANEL_TOP,
                                              C_PANEL_BOTTOM));
    gfx_stroke_round(g, send, 8 * d, 1.0f, PS(C_PANEL_BORDER));
    for (int i = 0; i < PRESET_COUNT; i++) {
        SsRect row = rect_make(send.x + 6 * d, send.y + 8 * d + i * item_h, send.w - 12 * d,
                             item_h - 2 * d);
        int hot = is_hot(app, W_PRESET_0 + i, row);
        if (hot) gfx_fill_round(g, row, 5 * d, PS(col_with_alpha(C_ACCENT, 120)));
        static const char *DESC[PRESET_COUNT] = {
            "The whole system, moons and all", "Mercury to Mars",
            "Gas giants and their moons", "Ice giants, Pluto and comets",
            "Binary star with circumbinary worlds", "One star, empty space"
        };
        gfx_text(g, row.x + 10 * d, row.y + 6 * d, sim_preset_name(i), FONT_UI_BOLD,
                 hot ? sss_theme()->on_accent : C_TEXT, SS_ALIGN_LEFT, SS_VALIGN_TOP);
        gfx_text(g, row.x + 10 * d, row.y + 20 * d, DESC[i], FONT_UI_TINY, C_TEXT_MUTED,
                 SS_ALIGN_LEFT, SS_VALIGN_TOP);
        hit_add(app, W_PRESET_0 + i, row);
    }
}

static void draw_theme_menu(App *app, SsGfx *g) {
    UI *ui = &app->ui;
    if (!ui->theme_open) return;
    float d = app->dpi;
    int n = sss_theme_count();
    float w = 356 * d;
    float head = 26 * d;
    float item_h = 50 * d;
    float h = head + 6 * d + n * item_h + 8 * d;
    SsRect btn = hit_rect_of(app, W_THEME);
    SsRect box = rect_make(btn.x + btn.w - w, btn.y + btn.h + 6 * d, w, h);
    if (box.x < 10 * d) box.x = 10 * d;
    if (box.x + box.w > app->width - 10 * d) box.x = app->width - 10 * d - box.w;
    if (box.y + box.h > app->height - 10 * d) box.y = app->height - 10 * d - box.h;

    gfx_fill_round(g, rect_offset(box, 0, 4 * d), 9 * d, PS(RGBA(0, 0, 0, 100)));
    gfx_fill_round(g, box, 9 * d, paint_linv(0, box.y, box.y + box.h, C_PANEL_TOP, C_PANEL_BOTTOM));
    gfx_stroke_round(g, box, 9 * d, 1.0f, PS(C_PANEL_BORDER));
    text_tracked(g, box.x + 14 * d, box.y + 9 * d, "THEME", FONT_UI_TINY, C_TEXT_MUTED, 0.6f,
                 SS_ALIGN_LEFT, SS_VALIGN_TOP);
    gfx_text(g, box.x + box.w - 14 * d, box.y + 9 * d, "F3 cycles", FONT_UI_TINY, C_TEXT_FAINT,
             SS_ALIGN_RIGHT, SS_VALIGN_TOP);

    for (int i = 0; i < n; i++) {
        const SsTheme *th = sss_theme_at(i);
        SsRect row = rect_make(box.x + 6 * d, box.y + head + 4 * d + i * item_h, box.w - 12 * d,
                               item_h - 3 * d);
        int hot = is_hot(app, W_THEME_0 + i, row);
        int active = i == sss_theme_index();
        if (hot) gfx_fill_round(g, row, 6 * d, PS(col_with_alpha(C_ACCENT, 80)));
        else if (active) gfx_fill_round(g, row, 6 * d, PS(col_with_alpha(C_ACCENT, 40)));

        /* three-stripe swatch: sky, accent, panel */
        SsRect sw = rect_make(row.x + 10 * d, row.y + (row.h - 30 * d) * 0.5f, 30 * d, 30 * d);
        gfx_fill_round(g, sw, 5 * d, PS(th->scene_bottom));
        gfx_fill_round(g, rect_make(sw.x + sw.w * 0.55f, sw.y + 4 * d, sw.w * 0.4f, sw.h - 8 * d),
                       3 * d, PS(th->accent));
        gfx_stroke_round(g, sw, 5 * d, 1.0f, PS(col_with_alpha(th->panel_border, 160)));

        gfx_text(g, row.x + 52 * d, row.y + 7 * d, th->name, FONT_UI_BOLD,
                 hot ? C_TEXT : (active ? C_ACCENT_HI : C_TEXT), SS_ALIGN_LEFT, SS_VALIGN_TOP);
        gfx_text(g, row.x + 52 * d, row.y + 24 * d, th->note, FONT_UI_TINY,
                 hot ? C_TEXT_DIM : C_TEXT_MUTED, SS_ALIGN_LEFT, SS_VALIGN_TOP);

        if (active) {
            float cx = row.x + row.w - 17 * d, cy = row.y + row.h * 0.5f;
            gfx_fill_circle(g, cx, cy, 7.5f * d, PS(C_ACCENT));
            gfx_line(g, cx - 3.4f * d, cy, cx - 0.8f * d, cy + 2.8f * d, 1.6f * d,
                     PS(sss_theme()->on_accent));
            gfx_line(g, cx - 0.8f * d, cy + 2.8f * d, cx + 3.8f * d, cy - 2.8f * d, 1.6f * d,
                     PS(sss_theme()->on_accent));
        }
        hit_add(app, W_THEME_0 + i, row);
    }
}

/* ------------------------------------------- simulation panel (SSG style) */

static void spin_button(App *app, SsGfx *g, uint32_t id, SsRect r, int up) {
    float d = app->dpi;
    int hot = is_hot(app, id, r);
    gfx_fill_round(g, r, 3.0f * d, PS(hot ? col_with_alpha(C_TEXT, 30) : col_with_alpha(C_TEXT, 10)));
    SsColor c = hot ? C_TEXT : C_TEXT_DIM;
    float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f, s = 3.2f * d;
    gfx_line(g, cx - s, cy, cx + s, cy, 1.3f, PS(c));
    if (up) gfx_line(g, cx, cy - s * 0.7f, cx, cy + s * 0.7f, 1.3f, PS(c));
    hit_add(app, id, r);
}

static void date_field(App *app, SsGfx *g, float x, float y, float w, const char *label, int value,
                       uint32_t id_dn, uint32_t id_up) {
    float d = app->dpi;
    text_tracked(g, x, y, label, FONT_UI_TINY, C_TEXT_MUTED, 0.5f, SS_ALIGN_LEFT, SS_VALIGN_TOP);
    char buf[8];
    snprintf(buf, sizeof(buf), "%04d", value);
    if (w < 70 * d) snprintf(buf, sizeof(buf), "%02d", value);
    SsRect box = rect_make(x, y + 11 * d, w, 24 * d);
    gfx_fill_round(g, box, 4 * d, PS(col_with_alpha(C_TEXT, 9)));
    gfx_stroke_round(g, box, 4 * d, 1.0f, PS(C_CARD_BORDER));
    gfx_text(g, box.x + box.w * 0.5f, box.y + box.h * 0.5f, buf, FONT_MONO, C_TEXT,
             SS_ALIGN_CENTER, SS_VALIGN_MIDDLE);
    float bw = (w - 4 * d) * 0.5f;
    spin_button(app, g, id_dn, rect_make(x, y + 38 * d, bw, 17 * d), 0);
    spin_button(app, g, id_up, rect_make(x + bw + 4 * d, y + 38 * d, bw, 17 * d), 1);
}

static void panel_dropdown(App *app, SsGfx *g, uint32_t id, SsRect r, const char *text, int open) {
    float d = app->dpi;
    int hot = is_hot(app, id, r);
    draw_button_base(g, r, RADIUS_BUTTON, 0, hot, open);
    SsRect tb = rect_make(r.x + 10 * d, r.y, r.w - 26 * d, r.h);
    gfx_text_elided(g, tb, text, FONT_UI_SMALL, hot ? C_TEXT : C_TEXT_MUTED,
                    SS_ALIGN_LEFT, SS_VALIGN_MIDDLE);
    ico_chevron(g, r.x + r.w - 12 * d, r.y + r.h * 0.5f, 7 * d, C_TEXT_DIM);
    hit_add(app, id, r);
}

static const char *body_name_of(const App *app, int id, const char *fallback) {
    int idx = sim_find(&app->sim, id);
    return idx >= 0 ? app->sim.bodies[idx].name : fallback;
}

static void bump_date(App *app, int field, int delta) {
    UI *ui = &app->ui;
    int y = ui->date_year, m = ui->date_month, d = ui->date_day;
    if (field == 0) {
        y = CLAMP(y + delta, 1800, 2100);
    } else if (field == 1) {
        m += delta;
        if (m < 1) { m = 12; y--; }
        if (m > 12) { m = 1; y++; }
        y = CLAMP(y, 1800, 2100);
    } else {
        int hh, mi, ss;
        jd_to_date(date_to_jd(y, m, d, 0, 0, 0) + delta, &y, &m, &d, &hh, &mi, &ss);
        if (y < 1800) { y = 1800; m = 1; d = 1; }
        if (y > 2100) { y = 2100; m = 12; d = 31; }
    }
    /* keep the day inside the month */
    static const int DIM[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int dim = DIM[(m - 1) % 12];
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) dim = 29;
    if (d < 1) d = 1;
    if (d > dim) d = dim;
    ui->date_year = y;
    ui->date_month = m;
    ui->date_day = d;
}

/* Clicks are hit-tested against the rectangles captured while drawing, so
 * clip them to the visible part of a scrolled panel too. */
static void hit_clip_since(App *app, int since, SsRect clip) {
    UI *ui = &app->ui;
    int n = since;
    for (int i = since; i < ui->hit_count; i++) {
        SsRect r;
        if (rect_intersect(ui->hit_rect[i], clip, &r)) {
            ui->hit_id[n] = ui->hit_id[i];
            ui->hit_rect[n] = r;
            n++;
        }
    }
    ui->hit_count = n;
}

static void draw_sim_panel(App *app, SsGfx *g) {
    UI *ui = &app->ui;
    float d = app->dpi;
    if (!ui->panel_open) return;
    SsRect vp = ui->viewport;
    float w = 292 * d, h = 416 * d;
    SsRect p = rect_make(vp.x + 16 * d, vp.y + 16 * d, w, h);
    if (p.y + p.h > vp.y + vp.h - 12 * d) p.h = vp.y + vp.h - 12 * d - p.y;
    if (p.h < 120 * d) p.h = 120 * d;
    ui->panel_rect = p;

    gfx_fill_round(g, rect_offset(p, 0, 5 * d), 10 * d, PS(RGBA(0, 0, 0, 100)));
    gfx_fill_round(g, p, 10 * d, paint_linv(0, p.y, p.y + p.h, C_PANEL_TOP, C_PANEL_BOTTOM));
    gfx_stroke_round(g, p, 10 * d, 1.0f, PS(C_PANEL_BORDER));

    float x = p.x + 16 * d;
    float cw = p.w - 32 * d;
    section_header(g, x, p.y + 14 * d, "SIMULATION");

    SsRect cb = rect_make(p.x + p.w - 32 * d, p.y + 9 * d, 22 * d, 22 * d);
    int chot = is_hot(app, W_SIM_CLOSE, cb);
    if (chot) gfx_fill_round(g, cb, 4 * d, PS(col_with_alpha(C_TEXT, 22)));
    gfx_line(g, cb.x + 7 * d, cb.y + 7 * d, cb.x + 15 * d, cb.y + 15 * d, 1.2f,
             PS(chot ? C_TEXT : C_TEXT_DIM));
    gfx_line(g, cb.x + 15 * d, cb.y + 7 * d, cb.x + 7 * d, cb.y + 15 * d, 1.2f,
             PS(chot ? C_TEXT : C_TEXT_DIM));
    hit_add(app, W_SIM_CLOSE, cb);

    /* The panel keeps its 416 px design height; short windows scroll it. */
    ui->panel_scroll_max = MAX(0.0f, 404 * d - p.h);
    if (ui->panel_scroll > ui->panel_scroll_max) ui->panel_scroll = ui->panel_scroll_max;
    if (ui->panel_scroll < 0) ui->panel_scroll = 0;
    float scroll = ui->panel_scroll;
    SsRect content_clip = rect_make(p.x, p.y + 30 * d, p.w, p.h - 30 * d);
    int hit0 = ui->hit_count;
    gfx_push_clip(g, content_clip);

    float y = p.y + 38 * d - scroll;
    section_header(g, x, y, "START DATE (UTC)");
    y += 16 * d;
    float yw = 88 * d, mw = 56 * d, dw = 56 * d, gap = 8 * d;
    date_field(app, g, x, y, yw, "YYYY", ui->date_year, W_DATE_Y_DN, W_DATE_Y_UP);
    date_field(app, g, x + yw + gap, y, mw, "MM", ui->date_month, W_DATE_M_DN, W_DATE_M_UP);
    date_field(app, g, x + yw + mw + gap * 2, y, dw, "DD", ui->date_day,
               W_DATE_D_DN, W_DATE_D_UP);
    y += 61 * d;

    SsRect setb = rect_make(x, y, cw - 76 * d - 8 * d, 26 * d);
    button_text(app, g, W_DATE_SET, setb, "SET START DATE", 1, NULL);
    SsRect nowb = rect_make(x + cw - 76 * d, y, 76 * d, 26 * d);
    button_text(app, g, W_DATE_NOW, nowb, "NOW", 0, NULL);
    y += 32 * d;

    section_header(g, x, y, "VIEW");
    y += 16 * d;
    struct { const char *label; int *flag; } opts[] = {
        {"Orbits", &ui->show_orbits}, {"Fade orbits", &ui->show_fade_orbits},
        {"Labels", &ui->show_labels}, {"Moon names", &ui->show_moon_names},
        {"Sun", &ui->show_sun}, {"Axes", &ui->show_axes},
        {"Simplified", &ui->simplified}, {"True scale", &ui->true_scale},
        {"Trails", &ui->show_trails}, {"Moons", &ui->show_moons},
    };
    int nopt = (int)ARRAY_LEN(opts);
    float cellw = (cw - 8 * d) * 0.5f;
    for (int i = 0; i < nopt; i++) {
        int col = i & 1, row = i >> 1;
        SsRect cell = rect_make(x + col * (cellw + 8 * d), y + row * 22 * d, cellw, 20 * d);
        int hot = is_hot(app, (uint32_t)(W_OPT_0 + i), cell);
        int on = *opts[i].flag;
        SsRect box = rect_make(cell.x, cell.y + (cell.h - 13 * d) * 0.5f, 13 * d, 13 * d);
        if (on) gfx_fill_round(g, box, 3.5f * d, PS(C_ACCENT));
        else gfx_stroke_round(g, box, 3.5f * d, 1.0f, PS(hot ? C_TEXT_DIM : C_CARD_BORDER));
        if (on) {
            gfx_line(g, box.x + 3 * d, box.y + 6.5f * d, box.x + 5.5f * d, box.y + 9.5f * d,
                     1.5f * d, PS(sss_theme()->on_accent));
            gfx_line(g, box.x + 5.5f * d, box.y + 9.5f * d, box.x + 10 * d, box.y + 3.5f * d,
                     1.5f * d, PS(sss_theme()->on_accent));
        }
        gfx_text(g, cell.x + 20 * d, cell.y + cell.h * 0.5f, opts[i].label, FONT_UI_SMALL,
                 on ? C_TEXT : (hot ? C_TEXT : C_TEXT_MUTED), SS_ALIGN_LEFT, SS_VALIGN_MIDDLE);
        hit_add(app, (uint32_t)(W_OPT_0 + i), cell);
    }
    y += 5 * 22 * d + 6 * d;

    section_header(g, x, y, "AUTO TRACK");
    y += 16 * d;
    const char *track = ui->follow_id ? body_name_of(app, ui->follow_id, "Free camera")
                                      : "Free camera";
    panel_dropdown(app, g, W_TRACK_PICK, rect_make(x, y, cw, 28 * d), track,
                   ui->picker_open && ui->picker_target == 1);
    y += 34 * d;

    section_header(g, x, y, "OBJECT SEPARATION");
    y += 16 * d;
    float half = (cw - 8 * d) * 0.5f;
    panel_dropdown(app, g, W_SEP_A, rect_make(x, y, half, 28 * d),
                   body_name_of(app, ui->sep_a_id, "Object A"),
                   ui->picker_open && ui->picker_target == 2);
    panel_dropdown(app, g, W_SEP_B, rect_make(x + half + 8 * d, y, half, 28 * d),
                   body_name_of(app, ui->sep_b_id, "Object B"),
                   ui->picker_open && ui->picker_target == 3);
    y += 32 * d;

    double au = app_separation_au(app, ui->sep_a_id, ui->sep_b_id);
    if (au >= 0.0) {
        char big[64], sub[80];
        snprintf(big, sizeof(big), "%.4f AU", au);
        double km = au * 149597870.7;
        if (km >= 1e6) snprintf(sub, sizeof(sub), "%.3f million km", km / 1e6);
        else snprintf(sub, sizeof(sub), "%.0f thousand km", km / 1e3);
        gfx_text(g, x, y, big, FONT_MONO, C_ACCENT_HI, SS_ALIGN_LEFT, SS_VALIGN_TOP);
        gfx_text(g, p.x + p.w - 16 * d, y, sub, FONT_UI_TINY, C_TEXT_FAINT,
                 SS_ALIGN_RIGHT, SS_VALIGN_TOP);
    } else {
        gfx_text(g, x, y, "Pick two objects to measure", FONT_UI_TINY, C_TEXT_FAINT,
                 SS_ALIGN_LEFT, SS_VALIGN_TOP);
    }

    gfx_pop_clip(g);
    hit_clip_since(app, hit0, content_clip);

    if (ui->panel_scroll_max > 0.5f) {
        float track_y = p.y + 36 * d;
        float track_h = p.h - 44 * d;
        if (track_h < 24 * d) track_h = 24 * d;
        gfx_fill_round(g, rect_make(p.x + p.w - 7 * d, track_y, 3 * d, track_h), 1.5f * d,
                       PS(col_with_alpha(C_TEXT, 26)));
        float visible = p.h - 30 * d;
        float total = 404 * d - 30 * d;
        float frac = visible / total;
        if (frac < 0.18f) frac = 0.18f;
        if (frac > 1.0f) frac = 1.0f;
        float thumb_h = track_h * frac;
        float t = ui->panel_scroll / ui->panel_scroll_max;
        float thumb_y = track_y + (track_h - thumb_h) * t;
        gfx_fill_round(g, rect_make(p.x + p.w - 8 * d, thumb_y, 5 * d, thumb_h), 2.5f * d,
                       PS(col_with_alpha(C_TEXT, 90)));
    }
}

static int alive_body_at(const App *app, int k) {
    int n = 0;
    for (int i = 0; i < app->sim.count; i++) {
        if (!app->sim.bodies[i].alive) continue;
        if (n == k) return i;
        n++;
    }
    return -1;
}

static void draw_picker(App *app, SsGfx *g) {
    UI *ui = &app->ui;
    float d = app->dpi;
    if (!ui->picker_open) return;
    uint32_t anchor_id = ui->picker_target == 1 ? W_TRACK_PICK
                       : (ui->picker_target == 2 ? W_SEP_A : W_SEP_B);
    SsRect anchor = hit_rect_of(app, anchor_id);
    if (anchor.w <= 0) {
        ui->picker_open = 0;
        return;
    }
    int alive = 0;
    for (int i = 0; i < app->sim.count; i++) if (app->sim.bodies[i].alive) alive++;
    int rows = alive + 1;
    float row_h = 25 * d;
    float w = 252 * d;
    float h = 36 * d + rows * row_h + 6 * d;
    if (h > 330 * d) h = 330 * d;
    SsRect box = rect_make(anchor.x, anchor.y + anchor.h + 5 * d, w, h);
    if (box.x + box.w > app->width - 10 * d) box.x = app->width - 10 * d - box.w;
    if (box.x < 10 * d) box.x = 10 * d;
    if (box.y + box.h > app->height - 36 * d) {
        box.y = anchor.y - box.h - 5 * d;
        if (box.y < 10 * d) box.y = 10 * d;
    }
    ui->picker_rect = box;
    ui->picker_content_h = rows * row_h;
    SsRect list = rect_make(box.x + 6 * d, box.y + 34 * d, box.w - 12 * d, box.h - 40 * d);
    float max_scroll = ui->picker_content_h - list.h;
    if (max_scroll < 0) max_scroll = 0;
    if (ui->picker_scroll > max_scroll) ui->picker_scroll = max_scroll;
    if (ui->picker_scroll < 0) ui->picker_scroll = 0;

    gfx_fill_round(g, rect_offset(box, 0, 4 * d), 8 * d, PS(RGBA(0, 0, 0, 100)));
    gfx_fill_round(g, box, 8 * d, paint_linv(0, box.y, box.y + box.h, C_PANEL_TOP,
                                             C_PANEL_BOTTOM));
    gfx_stroke_round(g, box, 8 * d, 1.0f, PS(C_PANEL_BORDER));
    const char *title = ui->picker_target == 1 ? "AUTO TRACK"
                      : (ui->picker_target == 2 ? "OBJECT A" : "OBJECT B");
    section_header(g, box.x + 12 * d, box.y + 10 * d, title);
    gfx_text(g, box.x + box.w - 12 * d, box.y + 10 * d, "click to pick", FONT_UI_TINY,
             C_TEXT_FAINT, SS_ALIGN_RIGHT, SS_VALIGN_TOP);

    gfx_push_clip(g, list);
    float yy = list.y - ui->picker_scroll;
    int first = (int)(ui->picker_scroll / row_h);
    int last = first + (int)(list.h / row_h) + 2;
    for (int k = first; k <= last && k < rows; k++) {
        SsRect row = rect_make(list.x, yy + k * row_h, list.w, row_h - 1 * d);
        if (row.y + row.h <= list.y || row.y >= list.y + list.h) continue;
        int active = 0;
        int marker = 0;
        const char *name;
        SsColor dot = C_TEXT_DIM;
        if (k == 0) {
            name = ui->picker_target == 1 ? "Free camera" : "None";
            active = ui->picker_target == 1 ? ui->follow_id == 0
                                            : (ui->picker_target == 2 ? ui->sep_a_id == 0
                                                                      : ui->sep_b_id == 0);
        } else {
            int idx = alive_body_at(app, k - 1);
            if (idx < 0) continue;
            Body *b = &app->sim.bodies[idx];
            name = b->name;
            dot = b->accent;
            marker = 1;
            active = ui->picker_target == 1 ? ui->follow_id == b->id
                   : (ui->picker_target == 2 ? ui->sep_a_id == b->id : ui->sep_b_id == b->id);
        }
        int hot = is_hot(app, (uint32_t)(W_PICK_0 + k), row);
        if (hot) gfx_fill_round(g, row, 4 * d, PS(col_with_alpha(C_ACCENT, 90)));
        else if (active) gfx_fill_round(g, row, 4 * d, PS(col_with_alpha(C_ACCENT, 46)));
        if (marker) gfx_fill_circle(g, row.x + 11 * d, row.y + row.h * 0.5f, 4 * d, PS(dot));
        else {
            gfx_stroke_circle(g, row.x + 11 * d, row.y + row.h * 0.5f, 3.6f * d, 1.0f,
                              PS(C_TEXT_FAINT));
        }
        gfx_text(g, row.x + 21 * d, row.y + row.h * 0.5f, name, FONT_UI_SMALL,
                 (hot || active) ? C_TEXT : C_TEXT_DIM, SS_ALIGN_LEFT, SS_VALIGN_MIDDLE);
        hit_add(app, (uint32_t)(W_PICK_0 + k), row);
    }
    gfx_pop_clip(g);
}

static void draw_help(App *app, SsGfx *g) {
    UI *ui = &app->ui;
    if (!ui->help_open) return;
    float d = app->dpi;
    gfx_fill_rect(g, rect_make(0, 0, (float)app->width, (float)app->height),
                  PS(col_alpha(C_WINDOW_TOP, 0.72f)));
    float w = 540 * d, h = 398 * d;
    SsRect box = rect_center(rect_make(0, 0, (float)app->width, (float)app->height), w, h);
    gfx_fill_round(g, rect_offset(box, 0, 6 * d), 10 * d, PS(RGBA(0, 0, 0, 110)));
    gfx_fill_round(g, box, 10 * d, paint_linv(0, box.y, box.y + box.h, C_PANEL_TOP,
                                              C_PANEL_BOTTOM));
    gfx_stroke_round(g, box, 10 * d, 1.0f, PS(C_PANEL_BORDER));
    gfx_text(g, box.x + 22 * d, box.y + 18 * d, "Solar System Sandbox", FONT_H1, C_TEXT,
             SS_ALIGN_LEFT, SS_VALIGN_TOP);
    gfx_text(g, box.x + 22 * d, box.y + 46 * d, "A real ephemeris, plus a gravity sandbox.",
             FONT_UI_SMALL, C_TEXT_DIM, SS_ALIGN_LEFT, SS_VALIGN_TOP);
    static const char *KEYS[][2] = {
        {"Space", "play or pause"},
        {"1 - 5", "speed presets"},
        {"N", "toggle N-body gravity"},
        {"O / L", "orbits / labels"},
        {"T", "trails"},
        {"C", "simulation controls"},
        {"F", "focus selected body"},
        {"F3", "cycle the colour theme"},
        {"F4", "open the content library"},
        {"R", "reset the view"},
        {"Delete", "remove the selected body"},
        {"F12", "save a screenshot"},
        {"F1 / Esc", "close this help"},
        {"Wheel", "zoom toward the cursor"},
        {"Drag", "pan; right-drag orbits"},
    };
    float y = box.y + 80 * d;
    for (int i = 0; i < (int)ARRAY_LEN(KEYS); i++) {
        float col = (i < 8) ? 0 : 1;
        if (col == 1) continue;
        gfx_text(g, box.x + 24 * d, y, KEYS[i][0], FONT_MONO, C_ACCENT_HI, SS_ALIGN_LEFT, SS_VALIGN_TOP);
        gfx_text(g, box.x + 130 * d, y, KEYS[i][1], FONT_UI_SMALL, C_TEXT_DIM, SS_ALIGN_LEFT, SS_VALIGN_TOP);
        y += 25 * d;
    }
    y = box.y + 80 * d;
    for (int i = 8; i < (int)ARRAY_LEN(KEYS); i++) {
        gfx_text(g, box.x + 282 * d, y, KEYS[i][0], FONT_MONO, C_ACCENT_HI, SS_ALIGN_LEFT, SS_VALIGN_TOP);
        gfx_text(g, box.x + 366 * d, y, KEYS[i][1], FONT_UI_SMALL, C_TEXT_DIM, SS_ALIGN_LEFT, SS_VALIGN_TOP);
        y += 25 * d;
    }
    gfx_text(g, box.x + 22 * d, box.y + box.h - 30 * d,
             "Simulation data: JPL approximate ephemerides (1800-2050). Build: " SSS_VERSION_STRING,
             FONT_UI_TINY, C_TEXT_FAINT, SS_ALIGN_LEFT, SS_VALIGN_TOP);
    SsRect close = rect_make(box.x + box.w - 30 * d, box.y + 14 * d, 20 * d, 20 * d);
    button_icon(app, g, W_HELP_CLOSE, close, NULL, NULL, 2);
    gfx_line(g, close.x + 6 * d, close.y + 6 * d, close.x + 14 * d, close.y + 14 * d, 1.2f, PS(C_TEXT_DIM));
    gfx_line(g, close.x + 14 * d, close.y + 6 * d, close.x + 6 * d, close.y + 14 * d, 1.2f, PS(C_TEXT_DIM));
}

static void draw_toast(App *app, SsGfx *g) {
    UI *ui = &app->ui;
    if (!ui->toast[0] || app->now > ui->toast_until) return;
    float d = app->dpi;
    float remain = (float)(ui->toast_until - app->now);
    float a = remain < 0.4f ? remain / 0.4f : 1.0f;
    if (a < 0.0f) a = 0.0f;
    if (a > 1.0f) a = 1.0f;
    SsRect vp = ui->viewport;
    float tw = gfx_text_w(g, ui->toast, FONT_UI_SMALL) + 34 * d;
    SsRect chipr = rect_make(vp.x + vp.w * 0.5f - tw * 0.5f, vp.y + vp.h - 96 * d, tw, 30 * d);
    gfx_fill_round(g, chipr, 15 * d, PS(col_alpha(sss_theme()->overlay_top, a)));
    gfx_stroke_round(g, chipr, 15 * d, 1.0f,
                     PS(col_alpha(ui->toast_kind == 1 ? C_RED : C_ACCENT, a * 0.75f)));
    gfx_fill_circle(g, chipr.x + 15 * d, chipr.y + chipr.h * 0.5f, 3 * d,
                    PS(col_alpha(ui->toast_kind == 1 ? C_RED : C_GREEN, a)));
    gfx_text(g, chipr.x + 26 * d, chipr.y + chipr.h * 0.5f, ui->toast, FONT_UI_SMALL,
             col_alpha(sss_theme()->overlay_text, a), SS_ALIGN_LEFT, SS_VALIGN_MIDDLE);
}

void app_draw(App *app, SsGfx *g) { ui_draw(app, g); }

void app_event(App *app, const UiEvent *ev) { ui_event(app, ev); }

/* ------------------------------------------------------------------ draw */

void ui_layout(App *app) {
    app->ui.titlebar = rect_make(0, 0, (float)app->width, TITLEBAR_H * app->dpi);
    app->ui.transport = rect_make(0, app->ui.titlebar.y + app->ui.titlebar.h, (float)app->width,
                                  TRANSPORT_H * app->dpi);
    float top = app->ui.transport.y + app->ui.transport.h;
    float bottom = (float)app->height - STATUS_H * app->dpi;
    app->ui.status = rect_make(0, bottom, (float)app->width, STATUS_H * app->dpi);
    app->ui.inspector = rect_make((float)app->width - PANEL_W * app->dpi, top, PANEL_W * app->dpi,
                                  bottom - top);
    app->ui.viewport = rect_make(0, top, (float)app->width - PANEL_W * app->dpi, bottom - top);
}

void ui_draw(App *app, SsGfx *g) {
    UI *ui = &app->ui;
    ui_layout(app);
    hit_init(app);
    ui->hot_id = 0;

    /* hovered body for labels (not while a panel covers the scene) */
    int over_panel = (ui->panel_open && rect_contains(ui->panel_rect, ui->mouse_x, ui->mouse_y)) ||
                     (ui->picker_open && rect_contains(ui->picker_rect, ui->mouse_x,
                                                       ui->mouse_y));
    if (!over_panel && rect_contains(ui->viewport, ui->mouse_x, ui->mouse_y)) {
        int id = scene_pick(app, ui->mouse_x, ui->mouse_y);
        ui->hover_id = id;
    } else {
        ui->hover_id = 0;
    }

    gfx_clear(g, C_WINDOW_BOTTOM);
    gfx_fill_rect(g, rect_make(0, 0, (float)app->width, (float)app->height),
                  paint_linv(0, 0, (float)app->height, C_WINDOW_TOP, C_WINDOW_BOTTOM));

    scene_draw(app, g, ui->viewport);
    draw_transport(app, g, ui->transport);
    draw_inspector(app, g, ui->inspector);
    draw_status(app, g, ui->status);
    draw_tool_pill(app, g, ui->viewport);
    draw_sim_panel(app, g);
    ui_draw_caption(app, g, ui->titlebar);
    draw_preset_menu(app, g);
    draw_theme_menu(app, g);
    draw_picker(app, g);
    draw_help(app, g);
    draw_toast(app, g);
}

/* ---------------------------------------------------------------- input */

static void zoom_about(App *app, float px, float py, double factor) {
    UI *ui = &app->ui;
    double zoom0 = ui->zoom;
    double zoom1 = clamp_zoom(app, zoom0 * factor);
    if (zoom1 == zoom0) return;
    /* keep the world point under the cursor fixed */
    double cx = ui->viewport.x + ui->viewport.w * 0.5;
    double cy = ui->viewport.y + ui->viewport.h * 0.5;
    double du = (px - cx) / zoom0, dv = -(py - cy) / (zoom0 * sin(ui->pitch));
    double dwx = du * cos(ui->yaw) + dv * sin(ui->yaw);
    double dwy = -du * sin(ui->yaw) + dv * cos(ui->yaw);
    ui->center.x += dwx * (1.0 - zoom0 / zoom1);
    ui->center.y += dwy * (1.0 - zoom0 / zoom1);
    ui->zoom = zoom1;
}

void ui_event(App *app, const UiEvent *ev) {
    UI *ui = &app->ui;
    switch (ev->type) {
        case UI_EV_MOVE: {
            float dx = ev->x - ui->mouse_x;
            float dy = ev->y - ui->mouse_y;
            ui->mouse_x = ev->x;
            ui->mouse_y = ev->y;
            if (ui->drag_mode == 1) {
                Vec3 wd;
                /* pan: convert to world */
                double u = -dx / ui->zoom;
                double v = dy / (ui->zoom * sin(ui->pitch));
                wd.x = u * cos(ui->yaw) + v * sin(ui->yaw);
                wd.y = -u * sin(ui->yaw) + v * cos(ui->yaw);
                wd.z = 0;
                ui->center.x += wd.x;
                ui->center.y += wd.y;
                ui->follow_id = 0;
            } else if (ui->drag_mode == 2) {
                ui->yaw -= dx * 0.006f;
                ui->pitch = CLAMP(ui->pitch + dy * 0.005f, 0.12f, 1.50f);
                scene_build_camera(app);
            } else if (ui->active_id == W_SPEED) {
                SsRect r = hit_rect_of(app, W_SPEED);
                if (r.w > 0) {
                    float t = CLAMP((ui->mouse_x - (r.x + 8)) / (r.w - 16), 0, 1);
                    app->sim.speed = 0.02 * pow(3650.0 / 0.02, t);
                }
            } else if (ui->active_id == W_MASS) {
                SsRect r = hit_rect_of(app, W_MASS);
                if (r.w > 0) {
                    float t = CLAMP((ui->mouse_x - (r.x + 8)) / (r.w - 16), 0, 1);
                    ui->add_mass_log = 18.0f + t * 13.0f;
                }
            } else if (ui->active_id == W_RADIUS) {
                SsRect r = hit_rect_of(app, W_RADIUS);
                if (r.w > 0) {
                    float t = CLAMP((ui->mouse_x - (r.x + 8)) / (r.w - 16), 0, 1);
                    ui->add_radius_log = -1.0f + t * 7.0f;
                }
            } else if (ui->active_id == W_ORBIT) {
                SsRect r = hit_rect_of(app, W_ORBIT);
                if (r.w > 0) {
                    float t = CLAMP((ui->mouse_x - (r.x + 8)) / (r.w - 16), 0, 1);
                    ui->add_orbit_log = -0.3f + t * 1.9f;
                }
            }
            break;
        }
        case UI_EV_DOWN: {
            ui->mouse_x = ev->x;
            ui->mouse_y = ev->y;
            ui->mouse_down = 1;
            uint32_t id = hit_find(app, ev->x, ev->y);
            if (id) {
                ui->active_id = id;
                break;
            }
            if (ui->picker_open && !rect_contains(ui->picker_rect, ev->x, ev->y)) {
                ui->picker_open = 0;
            }
            if (rect_contains(ui->inspector, ev->x, ev->y) ||
                (ui->panel_open && rect_contains(ui->panel_rect, ev->x, ev->y))) {
                ui->preset_open = 0;
                ui->theme_open = 0;
            }
            int over_panel = (ui->panel_open && rect_contains(ui->panel_rect, ev->x, ev->y)) ||
                             (ui->picker_open && rect_contains(ui->picker_rect, ev->x, ev->y));
            if (!over_panel && rect_contains(ui->viewport, ev->x, ev->y)) {
                if (ev->button == UI_MB_RIGHT || ev->button == UI_MB_MIDDLE) {
                    ui->drag_mode = 2;
                } else {
                    ui->drag_mode = 1;
                    ui->drag_x = ev->x;
                    ui->drag_y = ev->y;
                }
            }
            break;
        }
        case UI_EV_UP: {
            ui->mouse_x = ev->x;
            ui->mouse_y = ev->y;
            ui->mouse_down = 0;
            uint32_t id = ui->active_id;
            uint32_t over = hit_find(app, ev->x, ev->y);
            ui->active_id = 0;
            if (ui->drag_mode) {
                ui->drag_mode = 0;
                if (id == 0) break;
            }
            if (id && over == id) {
                /* activate */
                if (id == W_PLAY) app->sim.paused = !app->sim.paused;
                else if (id == W_NOW) app_set_time_now(app);
                else if (id == W_SIM) {
                    ui->panel_open = !ui->panel_open;
                    ui->preset_open = 0;
                    ui->theme_open = 0;
                    ui->picker_open = 0;
                    ui->panel_scroll = 0;
                    if (ui->panel_open) app_sync_date_fields(app);
                } else if (id == W_SIM_CLOSE) {
                    ui->panel_open = 0;
                    ui->picker_open = 0;
                } else if (id == W_DATE_SET) {
                    app_set_start_date(app, ui->date_year, ui->date_month, ui->date_day);
                } else if (id == W_DATE_NOW) {
                    app_set_time_now(app);
                    ui->picker_open = 0;
                } else if (id == W_DATE_Y_DN) bump_date(app, 0, -1);
                else if (id == W_DATE_Y_UP) bump_date(app, 0, 1);
                else if (id == W_DATE_M_DN) bump_date(app, 1, -1);
                else if (id == W_DATE_M_UP) bump_date(app, 1, 1);
                else if (id == W_DATE_D_DN) bump_date(app, 2, -1);
                else if (id == W_DATE_D_UP) bump_date(app, 2, 1);
                else if (id >= W_OPT_0 && id < W_OPT_0 + 10) {
                    int *flags[10] = {&ui->show_orbits, &ui->show_fade_orbits,
                                      &ui->show_labels, &ui->show_moon_names,
                                      &ui->show_sun, &ui->show_axes,
                                      &ui->simplified, &ui->true_scale,
                                      &ui->show_trails, &ui->show_moons};
                    int *f = flags[id - W_OPT_0];
                    *f = !*f;
                } else if (id == W_TRACK_PICK || id == W_SEP_A || id == W_SEP_B) {
                    int target = id == W_TRACK_PICK ? 1 : (id == W_SEP_A ? 2 : 3);
                    if (ui->picker_open && ui->picker_target == target) {
                        ui->picker_open = 0;
                    } else {
                        ui->picker_open = 1;
                        ui->picker_target = target;
                        ui->picker_scroll = 0;
                        ui->preset_open = 0;
                        ui->theme_open = 0;
                    }
                } else if (id >= W_PICK_0 && id < W_PICK_0 + 1 + MAX_BODIES) {
                    int k = (int)(id - W_PICK_0);
                    int bid = 0;
                    if (k > 0) {
                        int idx = alive_body_at(app, k - 1);
                        if (idx >= 0) bid = app->sim.bodies[idx].id;
                    }
                    if (ui->picker_target == 1) app_set_track(app, bid);
                    else if (ui->picker_target == 2) ui->sep_a_id = bid;
                    else if (ui->picker_target == 3) ui->sep_b_id = bid;
                    ui->picker_open = 0;
                }
                else if (id == W_PRESET) {
                    ui->preset_open = !ui->preset_open;
                    ui->theme_open = 0;
                }
                else if (id >= W_PRESET_0 && id < W_PRESET_0 + PRESET_COUNT) {
                    app_apply_preset(app, (int)(id - W_PRESET_0));
                    ui->preset_open = 0;
                } else if (id == W_THEME) {
                    ui->theme_open = !ui->theme_open;
                    ui->preset_open = 0;
                } else if (id >= W_THEME_0 && id < W_THEME_0 + (uint32_t)sss_theme_count()) {
                    app_set_theme(app, (int)(id - W_THEME_0));
                } else if (id == W_SAVE) { if (app->on_save) app->on_save(app); }
                else if (id == W_LOAD) { if (app->on_load) app->on_load(app); }
                else if (id == W_HELP) ui->help_open = !ui->help_open;
                else if (id == W_HELP_CLOSE) ui->help_open = 0;
                else if (id == W_TAB_INSPECT) { ui->tab = 0; ui->library_scroll = 0; }
                else if (id == W_TAB_CREATE) { ui->tab = 1; ui->library_scroll = 0; }
                else if (id == W_TAB_LIBRARY) { ui->tab = 2; ui->library_scroll = 0; }
                else if (id == W_RESCAN) app_rescan_content(app);
                else if (id == W_OPEN_CONTENT) {
                    if (app->open_path) app->open_path(app, content_user_root());
                } else if (id >= W_FOLDER_0 && id < W_FOLDER_0 + CF_COUNT) {
                    const char *dir = content_dir((ContentFolder)(id - W_FOLDER_0));
                    if (dir && app->open_path) app->open_path(app, dir);
                } else if (id >= W_PART_0 && id < W_PART_0 + 100) {
                    app_use_part(app, (int)(id - W_PART_0));
                } else if (id >= W_PACK_0 && id < W_PACK_0 + 100) {
                    app_apply_pack(app, (int)(id - W_PACK_0) - 1);
                } else if (id >= W_MOD_0 && id < W_MOD_0 + 100) {
                    app_open_mod(app, (int)(id - W_MOD_0));
                } else if (id == W_FOCUS) app_focus_selected(app);
                else if (id == W_TRACK) {
                    int idx = sim_find(&app->sim, ui->selected_id);
                    if (idx >= 0)
                        app_set_track(app, ui->follow_id == ui->selected_id ? 0
                                                                           : ui->selected_id);
                } else if (id == W_DYNAMIC) {
                    int idx = sim_find(&app->sim, ui->selected_id);
                    if (idx >= 0) {
                        Body *b = &app->sim.bodies[idx];
                        if (b->kind != BT_MOON) {
                            int make_dyn = !(b->dynamic || (!b->on_rail && b->parent < 0));
                            sim_set_dynamic(&app->sim, idx, make_dyn);
                            if (make_dyn) app->sim.nbody = 1;
                            app_toast(app, make_dyn ? "Body released into the sandbox"
                                                    : "Body returned to its orbit", 0);
                        }
                    }
                } else if (id == W_DELETE) app_remove_selected(app);
                else if (id == W_ADD) app_add_custom(app);
                else if (id >= W_KIND_0 && id < W_KIND_0 + 8) {
                    ui->add_kind = (int)(id - W_KIND_0);
                    /* picking a type drops the palette of a Library part */
                    ui->part_colors = 0;
                    ui->part_name[0] = 0;
                }
                else if (id >= W_SCENARIO_0 && id < W_SCENARIO_0 + PRESET_COUNT) {
                    app_apply_preset(app, (int)(id - W_SCENARIO_0));
                    ui->tab = 0;
                } else if (id >= W_BODY_ROW_0 && id < W_BODY_ROW_0 + MAX_BODIES) {
                    int idx = (int)(id - W_BODY_ROW_0);
                    if (idx >= 0 && idx < app->sim.count) app_select(app, app->sim.bodies[idx].id);
                } else if (id == W_ORBITS) ui->show_orbits = !ui->show_orbits;
                else if (id == W_LABELS) ui->show_labels = !ui->show_labels;
                else if (id == W_TRAILS) ui->show_trails = !ui->show_trails;
                else if (id == W_MOONS) ui->show_moons = !ui->show_moons;
                else if (id == W_SCALE) {
                    ui->true_scale = !ui->true_scale;
                    app_toast(app, ui->true_scale ? "True scale: sizes and orbits to scale"
                                                  : "Enhanced scale: bodies enlarged for visibility", 0);
                } else if (id == W_ZIN) zoom_about(app, ui->viewport.x + ui->viewport.w * 0.5f,
                                                   ui->viewport.y + ui->viewport.h * 0.5f, 1.35);
                else if (id == W_ZOUT) zoom_about(app, ui->viewport.x + ui->viewport.w * 0.5f,
                                                  ui->viewport.y + ui->viewport.h * 0.5f, 1.0 / 1.35);
                else if (id == W_RESETVIEW) app_reset_view(app);
                else if (id == W_NBODY) {
                    app->sim.nbody = !app->sim.nbody;
                    app_toast(app, app->sim.nbody ? "N-body gravity enabled"
                                                  : "Gravity off: bodies coast straight", 0);
                } else if (id == W_MIN) { if (app->on_minimize) app->on_minimize(app); }
                else if (id == W_MAX) { if (app->on_maximize) app->on_maximize(app); }
                else if (id == W_CLOSE) { if (app->on_quit) app->on_quit(app); }
            } else if (!id && rect_contains(ui->viewport, ev->x, ev->y)) {
                int bid = scene_pick(app, ev->x, ev->y);
                if (bid) {
                    ui->selected_id = bid;
                    ui->tab = 0;
                } else if (ui->preset_open) {
                    ui->preset_open = 0;
                } else if (ui->theme_open) {
                    ui->theme_open = 0;
                }
            }
            break;
        }
        case UI_EV_DBLCLICK: {
            int bid = scene_pick(app, ev->x, ev->y);
            if (bid) {
                ui->selected_id = bid;
                app_focus_selected(app);
            }
            break;
        }
        case UI_EV_WHEEL: {
            if (ui->picker_open && rect_contains(ui->picker_rect, ev->x, ev->y)) {
                float list_h = ui->picker_rect.h - 40 * app->dpi;
                float max_scroll = ui->picker_content_h - list_h;
                if (max_scroll < 0) max_scroll = 0;
                ui->picker_scroll -= ev->wheel * 40 * app->dpi;
                if (ui->picker_scroll < 0) ui->picker_scroll = 0;
                if (ui->picker_scroll > max_scroll) ui->picker_scroll = max_scroll;
                break;
            }
            if (ui->panel_open && rect_contains(ui->panel_rect, ev->x, ev->y)) {
                ui->panel_scroll -= ev->wheel * 44 * app->dpi;
                if (ui->panel_scroll < 0) ui->panel_scroll = 0;
                if (ui->panel_scroll > ui->panel_scroll_max)
                    ui->panel_scroll = ui->panel_scroll_max;
                break;
            }
            if (ui->tab == 2 && rect_contains(ui->inspector, ev->x, ev->y)) {
                float max_scroll = ui->library_content_h - ui->library_area.h;
                if (max_scroll < 0) max_scroll = 0;
                ui->library_scroll -= ev->wheel * 42 * app->dpi;
                if (ui->library_scroll < 0) ui->library_scroll = 0;
                if (ui->library_scroll > max_scroll) ui->library_scroll = max_scroll;
            } else if (rect_contains(ui->viewport, ev->x, ev->y)) {
                double factor = ev->wheel > 0 ? 1.16 : 1.0 / 1.16;
                zoom_about(app, ev->x, ev->y, factor);
            }
            break;
        }
        case UI_EV_KEY: {
            switch (ev->key) {
                case UI_KEY_SPACE: app->sim.paused = !app->sim.paused; break;
                case 0x31: app->sim.speed = 1.0 / 24.0; break;
                case 0x32: app->sim.speed = 1.0; break;
                case 0x33: app->sim.speed = 7.0; break;
                case 0x34: app->sim.speed = 30.44; break;
                case 0x35: app->sim.speed = 365.25; break;
                case 'N': app->sim.nbody = !app->sim.nbody; break;
                case 'O': ui->show_orbits = !ui->show_orbits; break;
                case 'L': ui->show_labels = !ui->show_labels; break;
                case 'T': ui->show_trails = !ui->show_trails; break;
                case 'M': ui->show_moons = !ui->show_moons; break;
                case 'F': app_focus_selected(app); break;
                case UI_KEY_F3: app_next_theme(app); break;
                case UI_KEY_F4:
                    ui->tab = (ui->tab == 2) ? 0 : 2;
                    ui->library_scroll = 0;
                    break;
                case 'C':
                    ui->panel_open = !ui->panel_open;
                    ui->picker_open = 0;
                    ui->panel_scroll = 0;
                    if (ui->panel_open) app_sync_date_fields(app);
                    break;
                case 'R': app_reset_view(app); break;
                case UI_KEY_DELETE: app_remove_selected(app); break;
                case UI_KEY_ESC:
                    if (ui->picker_open) ui->picker_open = 0;
                    else if (ui->help_open) ui->help_open = 0;
                    else if (ui->preset_open) ui->preset_open = 0;
                    else if (ui->theme_open) ui->theme_open = 0;
                    else if (ui->panel_open) ui->panel_open = 0;
                    else { ui->follow_id = 0; ui->selected_id = 0; }
                    break;
                case UI_KEY_F1: ui->help_open = !ui->help_open; break;
                case UI_KEY_F12: if (app->on_screenshot) app->on_screenshot(app); break;
                case UI_KEY_F5: if (app->on_save) app->on_save(app); break;
                case UI_KEY_F9: if (app->on_load) app->on_load(app); break;
                case UI_KEY_LEFT: break;
                case UI_KEY_RIGHT: break;
                case UI_KEY_PLUS:
                    zoom_about(app, ui->viewport.x + ui->viewport.w * 0.5f,
                               ui->viewport.y + ui->viewport.h * 0.5f, 1.25);
                    break;
                case UI_KEY_MINUS:
                    zoom_about(app, ui->viewport.x + ui->viewport.w * 0.5f,
                               ui->viewport.y + ui->viewport.h * 0.5f, 1.0 / 1.25);
                    break;
                default: break;
            }
            break;
        }
        case UI_EV_RESIZE:
            app_resize(app, (int)ev->x, (int)ev->y);
            break;
        default: break;
    }
}
