/* scene.c -- the 3D viewport: starfield, orbits, planets, rings, labels. */
#include "app.h"
#include "theme.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

#define ORBIT_SEGMENTS 168
#define MAX_MOON_ORDINAL 32

typedef struct {
    int body;      /* index into sim.bodies */
    float x, y;    /* screen */
    float depth;
    float r;       /* visual radius px */
    float factor;  /* moon separation scale */
    Vec3 disp;     /* display world position */
    int ordinal;
} BodyScreen;

static Vec3 v3sub(Vec3 a, Vec3 b) { Vec3 r = {a.x - b.x, a.y - b.y, a.z - b.z}; return r; }
static Vec3 v3add(Vec3 a, Vec3 b) { Vec3 r = {a.x + b.x, a.y + b.y, a.z + b.z}; return r; }
static Vec3 v3mul(Vec3 a, double s) { Vec3 r = {a.x * s, a.y * s, a.z * s}; return r; }
static double v3len(Vec3 a) { return sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }
static Vec3 v3norm(Vec3 a) {
    double l = v3len(a);
    if (l < 1e-12) { Vec3 z = {0, 0, 0}; return z; }
    return v3mul(a, 1.0 / l);
}
static Vec3 v3cross(Vec3 a, Vec3 b) {
    Vec3 r = {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
    return r;
}

/* ----------------------------------------------------------------- stars */

static uint32_t hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static float hashf(uint32_t a, uint32_t b) {
    return (float)(hash_u32(a * 374761393u + b * 668265263u) & 0xFFFFFF) / 16777215.0f;
}

static void draw_starfield(App *app, SsGfx *g, SsRect vp, double t) {
    UI *ui = &app->ui;
    int count = ui->low_power ? 110 : 240;
    double yaw_off = ui->yaw * 90.0;
    double pan_off = (ui->center.x * 2.0 + ui->center.y * 1.3) * 1.4;
    for (int i = 0; i < count; i++) {
        float h1 = hashf(1, (uint32_t)i), h2 = hashf(2, (uint32_t)i), h3 = hashf(3, (uint32_t)i);
        float h4 = hashf(4, (uint32_t)i);
        double x = fmod((double)h1 * (vp.w + 160) + yaw_off + pan_off, (double)vp.w + 160) - 80;
        double y = fmod((double)h2 * (vp.h + 160) - pan_off * 0.55, (double)vp.h + 160) - 80;
        double sx = vp.x + x, sy = vp.y + y;
        if (sx < vp.x - 4 || sx > vp.x + vp.w + 4 || sy < vp.y - 4 || sy > vp.y + vp.h + 4) continue;
        float r = 0.5f + h3 * 1.15f;
        const SsTheme *th = sss_theme();
        int a = (int)((28 + h4 * 120) * th->star_alpha / 255);
        SsColor cool = col_with_alpha(th->star_cool, a);
        SsColor warm = col_with_alpha(th->star_warm, a);
        gfx_fill_circle(g, (float)sx, (float)sy, r, PS(h3 > 0.86f ? warm : cool));
        if (h3 > 0.955f) {
            if (ui->low_power) continue;
            float glow = 3.5f + h3 * 3.0f;
            SsColor g0 = col_with_alpha(th->glow, 26 * th->star_alpha / 255);
            SsColor g1 = col_with_alpha(th->glow, 0);
            SsColor cross = col_with_alpha(th->glow, 60 * th->star_alpha / 255);
            gfx_fill_circle(g, (float)sx, (float)sy, glow,
                            paint_rad((float)sx, (float)sy, glow, g0, g1));
            gfx_line(g, (float)sx - glow, (float)sy, (float)sx + glow, (float)sy, 0.8f, PS(cross));
            gfx_line(g, (float)sx, (float)sy - glow, (float)sx, (float)sy + glow, 0.8f, PS(cross));
        }
    }
    (void)t;
}

/* ----------------------------------------------------------- body layout */

static double body_radius_screen(App *app, const Body *b, double nearest) {
    return body_visual_radius_px(b, app->ui.zoom, (double)app->dpi, app->ui.true_scale, nearest);
}

static void layout_bodies(App *app, BodyScreen *out, int *count) {
    Sim *s = &app->sim;
    UI *ui = &app->ui;
    int n = 0;
    int moon_ordinal[MAX_BODIES];
    memset(moon_ordinal, 0, sizeof(moon_ordinal));

    /* Spacing between root bodies on screen, used to keep discs from merging
     * when the whole system is squeezed into a few pixels. */
    float root_x[MAX_BODIES], root_y[MAX_BODIES];
    int root_n = 0;
    double nearest_of[MAX_BODIES];
    for (int i = 0; i < s->count; i++) {
        Body *b = &s->bodies[i];
        if (!b->alive || b->parent >= 0) continue;
        scene_project(app, b->wpos, &root_x[root_n], &root_y[root_n], NULL);
        root_n++;
    }
    for (int i = 0; i < s->count; i++) nearest_of[i] = 1e30;
    for (int i = 0, a = 0; i < s->count && a < root_n; i++) {
        if (!s->bodies[i].alive || s->bodies[i].parent >= 0) continue;
        for (int j = 0, c = 0; j < s->count && c < root_n; j++) {
            if (!s->bodies[j].alive || s->bodies[j].parent >= 0) continue;
            if (j != i) {
                double d = hypot(root_x[a] - root_x[c], root_y[a] - root_y[c]);
                if (d < nearest_of[i]) nearest_of[i] = d;
            }
            c++;
        }
        a++;
    }

    for (int i = 0; i < s->count; i++) {
        Body *b = &s->bodies[i];
        if (!b->alive) continue;
        int is_moon = b->parent >= 0;
        if (is_moon && !ui->show_moons) continue;

        Vec3 disp = b->wpos;
        int pidx = b->parent;
        double pr = 0;
        if (is_moon && pidx >= 0 && pidx < s->count) {
            Body *p = &s->bodies[pidx];
            pr = body_radius_screen(app, p, 0.0);
            double phys = v3len(v3sub(b->wpos, p->wpos)) * ui->zoom;
            double min_sep = pr + (5.0 + 1.8 * moon_ordinal[pidx]) * app->dpi;
            if (phys < min_sep && phys > 1e-12) {
                double factor = min_sep / phys;
                if (factor > 6000.0) factor = 6000.0;
                disp = v3add(p->wpos, v3mul(v3sub(b->wpos, p->wpos), factor));
                out[n].factor = (float)factor;
            } else {
                out[n].factor = 1.0f;
            }
            out[n].ordinal = moon_ordinal[pidx]++;
        } else {
            out[n].factor = 1.0f;
            out[n].ordinal = 0;
        }
        out[n].disp = disp;
        out[n].body = i;
        scene_project(app, disp, &out[n].x, &out[n].y, &out[n].depth);
        double r = body_radius_screen(app, b, is_moon ? 0.0 : nearest_of[i]);
        if (is_moon) {
            double minr = (1.3 + 0.22 * b->radius_km / 1737.0) * app->dpi;
            if (r < minr) r = minr;
            if (r > pr * 0.34 && pr > 0) r = pr * 0.34;
        }
        out[n].r = (float)r;
        n++;
    }

    /* painter's order: far to near */
    for (int i = 1; i < n; i++) {
        BodyScreen key = out[i];
        int j = i - 1;
        while (j >= 0 && out[j].depth > key.depth) {
            out[j + 1] = out[j];
            j--;
        }
        out[j + 1] = key;
    }
    *count = n;
}

static int body_screen_index(BodyScreen *list, int count, int body_idx) {
    for (int i = 0; i < count; i++)
        if (list[i].body == body_idx) return i;
    return -1;
}

/* ------------------------------------------------------------------ tints */

static SsColor sun_dir_color(App *app, Vec3 from, Vec3 *out_dir) {
    int sun = -1;
    for (int i = 0; i < app->sim.count; i++) {
        if (app->sim.bodies[i].alive && app->sim.bodies[i].kind == BT_STAR) {
            sun = i;
            break;
        }
    }
    if (sun < 0) {
        out_dir->x = -0.5; out_dir->y = -0.5; out_dir->z = 0;
        return RGB_A(255, 240, 210);
    }
    Vec3 s = app->sim.bodies[sun].wpos;
    *out_dir = v3norm(v3sub(s, from));
    return app->sim.bodies[sun].col1;
}

/* --------------------------------------------------------------- planets */

static void draw_rings(App *app, SsGfx *g, Body *b, float cx, float cy, float r, float depth,
                       int front_pass) {
    if (!b->ringed || b->ring_out <= 0) return;
    UI *ui = &app->ui;
    double tilt = b->axial_tilt * DEG2RAD;
    double az = b->tex_seed * 0.61;
    Vec3 axis = {sin(tilt) * cos(az), sin(tilt) * sin(az), cos(tilt)};
    Vec3 ref = {0, 0, 1};
    Vec3 ex = v3cross(axis, ref);
    if (v3len(ex) < 1e-3) { ex = (Vec3){1, 0, 0}; }
    ex = v3norm(ex);
    Vec3 ey = v3norm(v3cross(axis, ex));

#define RING_SEGMENTS 72
    float outer_x[RING_SEGMENTS], outer_y[RING_SEGMENTS], inner_x[RING_SEGMENTS], inner_y[RING_SEGMENTS];
    float seg_depth[RING_SEGMENTS];
    int front[RING_SEGMENTS];
    for (int i = 0; i < RING_SEGMENTS; i++) {
        double t = TAU * i / RING_SEGMENTS;
        double co = cos(t), si = sin(t);
        Vec3 dir = v3add(v3mul(ex, co), v3mul(ey, si));
        Vec3 po = v3add(b->wpos, v3mul(dir, b->ring_out * b->radius_km * AU_KM));
        Vec3 pin = v3add(b->wpos, v3mul(dir, b->ring_in * b->radius_km * AU_KM));
        float ox, oy, d1, ix, iy, d2;
        scene_project(app, po, &ox, &oy, &d1);
        scene_project(app, pin, &ix, &iy, &d2);
        outer_x[i] = ox; outer_y[i] = oy;
        inner_x[i] = ix; inner_y[i] = iy;
        seg_depth[i] = (d1 + d2) * 0.5f;
        front[i] = seg_depth[i] < depth;
    }
    SsColor ring_col = b->ring_col;
    float base_a = (float)COL_A(ring_col) / 255.0f;
    (void)cx; (void)cy;

    int i = 0;
    while (i < RING_SEGMENTS) {
        int side = front[i];
        if (side != front_pass) {
            i++;
            continue;
        }
        int j = i;
        while (j + 1 < RING_SEGMENTS && front[j + 1] == side) j++;
        float pts[2 * (RING_SEGMENTS + 2) * 2];
        int npts = 0;
        for (int k = i; k <= j; k++) {
            pts[npts * 2] = outer_x[k];
            pts[npts * 2 + 1] = outer_y[k];
            npts++;
        }
        /* close with the matching inner arc, reversed */
        for (int k = j; k >= i; k--) {
            pts[npts * 2] = inner_x[k];
            pts[npts * 2 + 1] = inner_y[k];
            npts++;
        }
        if (npts >= 3) {
            gfx_fill_poly(g, pts, npts, PS(col_with_alpha(ring_col, (int)(base_a * 255.0f * (front_pass ? 1.0f : 0.72f)))));
            /* bright inner/outer edges */
            gfx_polyline(g, pts, j - i + 2, 1.0f,
                         PS(col_with_alpha(ring_col, (int)(base_a * 90.0f))), 0);
        }
        i = j + 1;
    }
    /* thin Cassini-like gaps: draw two darker circles inside the ring band */
    double gap_r[2] = {b->ring_in + (b->ring_out - b->ring_in) * 0.42,
                       b->ring_in + (b->ring_out - b->ring_in) * 0.74};
    for (int gi = 0; gi < 2; gi++) {
        float prev_x = 0, prev_y = 0;
        int started = 0;
        for (int k = 0; k <= RING_SEGMENTS; k++) {
            int idx = k % RING_SEGMENTS;
            int side = front[idx];
            if (side != front_pass) { started = 0; continue; }
            double t = TAU * idx / RING_SEGMENTS;
            Vec3 dir = v3add(v3mul(ex, cos(t)), v3mul(ey, sin(t)));
            Vec3 p = v3add(b->wpos, v3mul(dir, gap_r[gi] * b->radius_km * AU_KM));
            float x, y;
            scene_project(app, p, &x, &y, NULL);
            if (started) gfx_line(g, prev_x, prev_y, x, y, 1.2f * app->dpi, PS(RGBA(6, 10, 20, 120)));
            prev_x = x;
            prev_y = y;
            started = 1;
        }
    }
    (void)ui;
}

static void draw_star_body(App *app, SsGfx *g, Body *b, float x, float y, float r) {
    float pulse = 1.0f + 0.02f * sinf((float)app->now * 1.7f + (float)b->tex_seed);
    float R = r * pulse;
    if (!app->ui.low_power) {
        gfx_fill_circle(g, x, y, R * 3.6f, paint_rad(x, y, R * 3.6f,
                                                     col_with_alpha(b->col1, 18), col_with_alpha(b->col1, 0)));
        gfx_fill_circle(g, x, y, R * 2.1f, paint_rad(x, y, R * 2.1f,
                                                     RGBA(255, 198, 110, 32), RGBA(255, 150, 60, 0)));
    }
    gfx_fill_circle(g, x, y, R * 1.35f, paint_rad(x, y, R * 1.35f,
                                                  RGBA(255, 224, 156, 100), RGBA(255, 160, 60, 0)));
    gfx_fill_circle(g, x, y, R, paint_rad(x - R * 0.2f, y - R * 0.22f, R * 1.45f,
                                          RGB_A(255, 253, 240), RGB_A(255, 186, 84)));
    /* granulation */
    int grains = app->ui.low_power ? 9 : 18;
    for (int i = 0; i < grains; i++) {
        float a = hashf(11u, (uint32_t)(b->tex_seed * 31 + i)) * TAU;
        float d = sqrtf(hashf(12u, (uint32_t)(b->tex_seed * 31 + i))) * R * 0.86f;
        float rr = (0.05f + 0.12f * hashf(13u, (uint32_t)(b->tex_seed * 31 + i))) * R;
        gfx_fill_circle(g, x + cosf(a) * d, y + sinf(a) * d, rr,
                        PS(RGBA(255, 226, 150, 26 + (int)(hashf(14u, (uint32_t)i) * 26))));
    }
    gfx_stroke_circle(g, x, y, R * 1.02f, 1.2f, PS(RGBA(255, 236, 180, 90)));
}

static void draw_planet_body(App *app, SsGfx *g, Body *b, float x, float y, float r, float depth) {
    /* The active texture pack tints the lit side of every body. */
    SsColor lit = app_body_tint(app, b->col1, (int)b->kind);
    SsColor shade = b->col2;
    if (app->pack_index >= 0) {
        const ContentPack *pack = content_pack(app->pack_index);
        if (pack && b->kind != BT_BLACKHOLE)
            shade = col_blend(b->col2, pack->tint, (float)pack->strength / 100.0f * 0.45f);
    }
    if (r < 0.8f) {
        gfx_fill_circle(g, x, y, 1.2f * app->dpi, PS(lit));
        return;
    }
    if (b->kind == BT_STAR) {
        draw_star_body(app, g, b, x, y, r);
        return;
    }
    if (app->ui.simplified && b->kind != BT_BLACKHOLE && b->kind != BT_COMET) {
        if (b->ringed) draw_rings(app, g, b, x, y, r, depth, 0);
        gfx_fill_circle(g, x, y, r, PS(lit));
        gfx_stroke_circle(g, x, y, r * 1.01f, 1.0f, PS(col_with_alpha(shade, 210)));
        if (b->ringed) draw_rings(app, g, b, x, y, r, depth, 1);
        return;
    }
    if (b->kind == BT_BLACKHOLE) {
        float gr = r * 2.2f;
        gfx_fill_circle(g, x, y, gr, paint_rad(x, y, gr, RGBA(170, 130, 255, 70), RGBA(120, 90, 220, 0)));
        gfx_fill_circle(g, x, y, r, PS(RGB_A(5, 4, 10)));
        gfx_stroke_circle(g, x, y, r * 1.06f, 1.6f, PS(RGBA(214, 186, 255, 170)));
        float aa = (float)app->now * 0.9f;
        for (int i = 0; i < 3; i++) {
            float t = aa + i * TAU / 3.0f;
            gfx_line(g, x + cosf(t) * r * 1.7f, y + sinf(t) * r * 1.7f,
                     x + cosf(t + 0.6f) * r * 2.6f, y + sinf(t + 0.6f) * r * 2.6f,
                     1.2f, PS(RGBA(198, 160, 255, 120)));
        }
        return;
    }
    if (b->kind == BT_COMET) {
        Vec3 sun_dir;
        sun_dir_color(app, b->wpos, &sun_dir);
        float tx, ty;
        scene_project_dir(app, v3mul(sun_dir, -1.0), &tx, &ty);
        float len = sqrtf(tx * tx + ty * ty);
        if (len > 1e-4f) {
            tx /= len;
            ty /= len;
            float l = 26.0f + r * 16.0f;
            float pts[8] = {x + tx * r * 3 + ty * r * 1.4f, y + ty * r * 3 - tx * r * 1.4f,
                            x + tx * l, y + ty * l,
                            x + tx * l * 0.55f, y + ty * l * 0.55f,
                            x + tx * r * 3 - ty * r * 1.4f, y + ty * r * 3 + tx * r * 1.4f};
            gfx_fill_poly(g, pts, 4, PS(RGBA(170, 220, 255, 34)));
            gfx_line(g, x, y, x + tx * l, y + ty * l, 1.0f, PS(RGBA(210, 240, 255, 40)));
        }
        gfx_fill_circle(g, x, y, r * 1.9f, paint_rad(x, y, r * 1.9f,
                                                     col_with_alpha(lit, 60), col_with_alpha(lit, 0)));
        gfx_fill_circle(g, x, y, r, PS(lit));
        return;
    }

    Vec3 sun_dir;
    sun_dir_color(app, b->wpos, &sun_dir);
    float sdx, sdy;
    scene_project_dir(app, sun_dir, &sdx, &sdy);
    float slen = sqrtf(sdx * sdx + sdy * sdy);
    if (slen > 1e-5f) { sdx /= slen; sdy /= slen; } else { sdx = -0.7f; sdy = -0.7f; }
    float lx = x + sdx * r * 0.42f;
    float ly = y + sdy * r * 0.42f;

    /* atmosphere halo */
    if (b->atmosphere && !app->ui.low_power) {
        SsColor halo = col_with_alpha(lit, 60);
        gfx_fill_circle(g, x, y, r * 1.28f, paint_rad(x, y, r * 1.28f, col_with_alpha(lit, 0), halo));
    }

    /* rings behind the planet */
    if (b->ringed) draw_rings(app, g, b, x, y, r, depth, 0);

    /* body */
    gfx_fill_circle(g, x, y, r, paint_rad(lx, ly, r * 1.62f, lit, shade));
    /* terminator darkening */
    gfx_fill_circle(g, x, y, r, paint_rad(lx, ly, r * 1.08f, RGBA(0, 0, 0, 0), RGBA(2, 6, 14, 116)));
    /* limb rim light */
    {
        float a0 = atan2f(sdy, sdx) * RAD2DEG;
        float start = a0 + 118.0f;
        gfx_arc(g, rect_make(x - r + 1, y - r + 1, r * 2 - 2, r * 2 - 2), start, 124.0f,
                (b->atmosphere ? 1.8f : 1.1f) * app->dpi,
                PS(col_with_alpha(b->col1, b->atmosphere ? 150 : 70)));
    }

    /* surface character */
    if (b->kind == BT_GIANT || b->kind == BT_ICE) {
        int bands = b->kind == BT_GIANT ? 7 : 5;
        for (int i = 0; i < bands; i++) {
            float fy = -0.72f + (i + 0.5f) * (1.44f / bands);
            float dy = fy * r;
            float half = sqrtf(fmaxf(0.0f, 1.0f - fy * fy)) * r * 0.985f;
            float bh = r * (0.055f + 0.035f * hashf(21u, (uint32_t)(b->tex_seed + i)));
            SsColor bc = (i % 2 == 0) ? col_with_alpha(b->col1, 46) : RGBA(8, 14, 28, 42);
            if (b->kind == BT_ICE) bc = (i % 2 == 0) ? RGBA(220, 250, 255, 26) : RGBA(6, 20, 40, 26);
            gfx_fill_ellipse(g, rect_make(x - half, y + dy - bh, half * 2, bh * 2), PS(bc));
        }
        /* a spot for Jupiter / Neptune */
        if (b->tex_seed == 15 || b->tex_seed == 18) {
            float ox = (b->tex_seed == 15 ? 0.26f : -0.22f) * r;
            float oy = 0.22f * r;
            gfx_fill_ellipse(g, rect_make(x + ox - r * 0.26f, y + oy - r * 0.12f, r * 0.52f, r * 0.24f),
                             PS(b->tex_seed == 15 ? RGBA(206, 110, 84, 150) : RGBA(16, 34, 74, 130)));
        }
    } else {
        /* rocky speckle + maria */
        int spots = app->ui.low_power ? 6 : 14;
        for (int i = 0; i < spots; i++) {
            float a = hashf(31u, (uint32_t)(b->tex_seed * 17 + i)) * TAU;
            float d = sqrtf(hashf(32u, (uint32_t)(b->tex_seed * 17 + i))) * r * 0.82f;
            float rr = r * (0.06f + 0.16f * hashf(33u, (uint32_t)(b->tex_seed * 17 + i)));
            SsColor c = (i % 3 == 0) ? col_with_alpha(b->col1, 60) : RGBA(6, 10, 20, 40);
            gfx_fill_circle(g, x + cosf(a) * d, y + sinf(a) * d, rr, PS(c));
        }
        if (b->tex_seed == 13) {
            /* Earth: ocean highlight + a hint of continents */
            gfx_fill_circle(g, x + sdx * r * 0.18f, y + sdy * r * 0.18f, r * 0.72f,
                            PS(RGBA(70, 130, 220, 60)));
            for (int i = 0; i < 5; i++) {
                float a = hashf(41u, (uint32_t)i) * TAU;
                float d = hashf(42u, (uint32_t)i) * r * 0.55f;
                gfx_fill_ellipse(g, rect_make(x + cosf(a) * d - r * 0.2f, y + sinf(a) * d - r * 0.13f,
                                              r * 0.4f, r * 0.26f), PS(RGBA(96, 168, 110, 90)));
            }
        }
        if (b->tex_seed == 14) {
            /* Mars polar cap */
            gfx_fill_ellipse(g, rect_make(x - r * 0.34f, y - r * 0.92f, r * 0.68f, r * 0.3f),
                             PS(RGBA(255, 240, 230, 110)));
        }
    }

    /* rings in front */
    if (b->ringed) draw_rings(app, g, b, x, y, r, depth, 1);

    /* specular glint */
    gfx_fill_circle(g, lx, ly, r * 0.20f, paint_rad(lx, ly, r * 0.20f, RGBA(255, 255, 255, 46), RGBA(255, 255, 255, 0)));
}

/* -------------------------------------------------------------- orbits */

static void draw_orbit_for(App *app, SsGfx *g, int idx, int selected, int hovered) {
    Sim *s = &app->sim;
    UI *ui = &app->ui;
    Body *b = &s->bodies[idx];
    if (!b->alive) return;
    if (b->parent >= 0) {
        if (!ui->show_moons) return;
        double pr = body_radius_screen(app, &s->bodies[b->parent], 1e9);
        double phys = v3len(v3sub(b->wpos, s->bodies[b->parent].wpos)) * ui->zoom;
        if (phys < pr * 2.2 && !selected) return;
    }
    static Vec3 pts[ORBIT_SEGMENTS];
    sim_orbit_points(s, idx, pts, ORBIT_SEGMENTS);
    /* moon orbits need to follow the display scaling */
    double factor = 1.0;
    int pidx = b->parent;
    if (pidx >= 0 && pidx < s->count) {
        double pr = body_radius_screen(app, &s->bodies[pidx], 1e9);
        double phys = v3len(v3sub(b->wpos, s->bodies[pidx].wpos)) * ui->zoom;
        double min_sep = pr + 5.0 * app->dpi;
        if (phys < min_sep && phys > 1e-12) factor = min_sep / phys;
    }
    float xy[ORBIT_SEGMENTS * 2];
    float depths[ORBIT_SEGMENTS];
    int n = 0;
    Vec3 ref = {0, 0, 0};
    if (pidx >= 0) ref = s->bodies[pidx].wpos;
    for (int i = 0; i < ORBIT_SEGMENTS; i++) {
        Vec3 p = pts[i];
        if (pidx >= 0) p = v3add(ref, v3mul(v3sub(p, ref), factor));
        float x, y;
        scene_project(app, p, &x, &y, &depths[n]);
        xy[n * 2] = x;
        xy[n * 2 + 1] = y;
        n++;
    }
    SsColor col = sss_theme()->orbit;
    int alpha = 34;
    float width = 1.0f * app->dpi;
    if (selected) { col = C_ACCENT_HI; alpha = 150; width = 1.4f * app->dpi; }
    else if (hovered) { col = sss_theme()->orbit_hot; alpha = 80; width = 1.2f * app->dpi; }
    else if (b->kind == BT_DWARF || b->kind == BT_COMET || b->kind == BT_ASTEROID) alpha = 22;
    if (b->on_rail && b->has_elements) {
        if (ui->show_fade_orbits && !selected && !hovered) {
            /* the half of the path behind the body fades out */
            float body_depth = 0;
            scene_project(app, b->wpos, NULL, NULL, &body_depth);
            for (int i = 1; i < n; i++) {
                float dm = (depths[i - 1] + depths[i]) * 0.5f;
                int a = dm > body_depth ? (int)(alpha * 0.32f) : alpha;
                if (a < 4) a = 4;
                gfx_line(g, xy[(i - 1) * 2], xy[(i - 1) * 2 + 1], xy[i * 2], xy[i * 2 + 1],
                         width, PS(col_with_alpha(col, a)));
            }
        } else {
            gfx_polyline(g, xy, n, width, PS(col_with_alpha(col, alpha)), 1);
        }
    }
}

/* --------------------------------------------------------------- labels */

static void draw_label_rect(App *app, SsGfx *g, const char *name, SsRect r, int selected,
                            int hovered, int moon) {
    const SsTheme *th = sss_theme();
    FontId f = selected ? FONT_UI_BOLD : FONT_UI_SMALL;
    SsColor c = selected ? C_TEXT : (hovered ? C_ACCENT_HI : th->label);
    if (moon) c = th->label_muted;
    float cx = r.x + r.w * 0.5f;
    SsColor shadow = col_with_alpha(th->label_shadow, selected ? 170 : COL_A(th->label_shadow));
    gfx_text(g, cx + 1, r.y + 1, name, f, shadow, SS_ALIGN_CENTER,
             SS_VALIGN_TOP);
    gfx_text(g, cx, r.y, name, f, c, SS_ALIGN_CENTER, SS_VALIGN_TOP);
}

/* Distance from a point to the closest edge of a rectangle (0 inside). */
static float rect_point_distance(SsRect r, float px, float py) {
    float dx = px < r.x ? r.x - px : (px > r.x + r.w ? px - (r.x + r.w) : 0.0f);
    float dy = py < r.y ? r.y - py : (py > r.y + r.h ? py - (r.y + r.h) : 0.0f);
    return sqrtf(dx * dx + dy * dy);
}

/* Rect the hover readout will occupy, so labels can make room for it. */
static int hover_chip_rect(App *app, SsGfx *g, BodyScreen *list, int si, SsRect vp,
                           SsRect *out) {
    Body *b = &app->sim.bodies[list[si].body];
    char sub[64];
    double d = body_distance_to_parent(b, &app->sim);
    if (d * KM_PER_AU > 0.02 * KM_PER_AU) snprintf(sub, sizeof(sub), "%.3f AU", d);
    else snprintf(sub, sizeof(sub), "%.0f km", d * KM_PER_AU);
    float tw = gfx_text_w(g, b->name, FONT_UI_SMALL);
    float sw = gfx_text_w(g, sub, FONT_MONO);
    float w = (tw > sw ? tw : sw) + 18 * app->dpi;
    /* Follow the cursor like a tooltip so it never sits on the hovered body
     * (or the selected one) and flips when it would leave the viewport. */
    float mx = app->ui.mouse_x, my = app->ui.mouse_y;
    SsRect chip = rect_make(mx + 16.0f * app->dpi, my + 16.0f * app->dpi, w, 34 * app->dpi);
    if (chip.x + chip.w > vp.x + vp.w - 8 * app->dpi)
        chip.x = mx - 16.0f * app->dpi - chip.w;
    if (chip.y + chip.h > vp.y + vp.h - 8 * app->dpi)
        chip.y = my - 16.0f * app->dpi - chip.h;
    if (chip.x < vp.x + 6 * app->dpi) chip.x = vp.x + 6 * app->dpi;
    if (chip.y < vp.y + 6 * app->dpi) chip.y = vp.y + 6 * app->dpi;
    if (chip.y + chip.h >= vp.y + vp.h - 4) return 0;
    (void)list;
    *out = chip;
    return 1;
}

static int label_priority(const App *app, const Body *b) {
    if (b->id == app->ui.selected_id) return 100;
    if (b->kind == BT_STAR) return 80;
    if (b->kind == BT_GIANT) return 60;
    if (b->kind == BT_ICE) return 50;
    if (b->kind == BT_TERRESTRIAL) return 40;
    if (b->kind == BT_DWARF) return 25;
    return 10;
}

/* ------------------------------------------------------------- overlays */

static void draw_scale_bar(App *app, SsGfx *g, SsRect vp) {
    UI *ui = &app->ui;
    double target_px = 108.0 * app->dpi;
    double au = target_px / ui->zoom;
    double mag = pow(10.0, floor(log10(au)));
    double pick = mag;
    if (au / mag >= 5.0) pick = mag * 5.0;
    else if (au / mag >= 2.0) pick = mag * 2.0;
    float px = (float)(pick * ui->zoom);
    float x1 = vp.x + vp.w - 18.0f * app->dpi;
    float x0 = x1 - px;
    float y = vp.y + vp.h - 20.0f * app->dpi;
    const SsTheme *th = sss_theme();
    gfx_line(g, x0, y, x1, y, 1.4f * app->dpi, PS(col_with_alpha(th->label, 150)));
    gfx_line(g, x0, y - 4 * app->dpi, x0, y + 4 * app->dpi, 1.2f * app->dpi,
             PS(col_with_alpha(th->label, 120)));
    gfx_line(g, x1, y - 4 * app->dpi, x1, y + 4 * app->dpi, 1.2f * app->dpi,
             PS(col_with_alpha(th->label, 120)));
    char buf[48];
    if (pick >= 1.0) snprintf(buf, sizeof(buf), "%.0f AU", pick);
    else if (pick >= 0.01) snprintf(buf, sizeof(buf), "%.2f AU", pick);
    else snprintf(buf, sizeof(buf), "%.0f km", pick * KM_PER_AU);
    gfx_text(g, (x0 + x1) * 0.5f, y - 7 * app->dpi, buf, FONT_MONO,
             col_with_alpha(th->label, 190),
             SS_ALIGN_CENTER, SS_VALIGN_BOTTOM);
    (void)app;
}

static void draw_selection_marker(App *app, SsGfx *g, float x, float y, float r) {
    UI *ui = &app->ui;
    float pulse = 0.5f + 0.5f * sinf((float)ui->pulse * 2.4f);
    float rr = r + (7.0f + pulse * 3.0f) * app->dpi;
    SsColor c = col_with_alpha(C_ACCENT_HI, (int)(120 + pulse * 70));
    int segs = 4;
    for (int i = 0; i < segs; i++) {
        float a0 = i * 90.0f + (float)ui->pulse * 14.0f;
        gfx_arc(g, rect_make(x - rr, y - rr, rr * 2, rr * 2), a0 + 8, 66, 1.5f * app->dpi, PS(c));
    }
    /* corner ticks */
    float t = 4.0f * app->dpi;
    gfx_line(g, x - rr - t, y, x - rr - t * 0.2f, y, 1.2f, PS(col_with_alpha(c, 90)));
    gfx_line(g, x + rr + t * 0.2f, y, x + rr + t, y, 1.2f, PS(col_with_alpha(c, 90)));
}

static void draw_hud(App *app, SsGfx *g, SsRect vp) {
    UI *ui = &app->ui;
    char buf[160];
    int total = 0;
    for (int i = 0; i < app->sim.count; i++)
        if (app->sim.bodies[i].alive) total++;

    /* top-left identity block */
    float x = vp.x + 18.0f * app->dpi, y = vp.y + 14.0f * app->dpi;
    gfx_text(g, x, y, sim_preset_name(app->sim.preset), FONT_H2, sss_theme()->hud_title,
             SS_ALIGN_LEFT, SS_VALIGN_TOP);
    gfx_text(g, x + 1, y + 20 * app->dpi, "", FONT_UI_TINY, C_TEXT_MUTED, SS_ALIGN_LEFT, SS_VALIGN_TOP);
    snprintf(buf, sizeof(buf), "%d bodies", total);
    gfx_text(g, x, y + 21 * app->dpi, buf, FONT_UI_TINY, sss_theme()->hud_dim, SS_ALIGN_LEFT,
             SS_VALIGN_TOP);

    /* follow chip */
    if (ui->follow_id) {
        int idx = sim_find(&app->sim, ui->follow_id);
        if (idx >= 0) {
            snprintf(buf, sizeof(buf), "TRACKING  %s", app->sim.bodies[idx].name);
            float w = gfx_text_w(g, buf, FONT_UI_TINY) + 18 * app->dpi;
            SsRect chip = rect_make(x, y + 40 * app->dpi, w, 18.0f * app->dpi);
            gfx_fill_round(g, chip, 9 * app->dpi, PS(RGBA(70, 130, 220, 70)));
            gfx_stroke_round(g, chip, 9 * app->dpi, 1.0f, PS(RGBA(140, 190, 255, 90)));
            gfx_text(g, chip.x + chip.w * 0.5f, chip.y + chip.h * 0.5f, buf, FONT_UI_TINY,
                     RGBA(200, 224, 255, 230), SS_ALIGN_CENTER, SS_VALIGN_MIDDLE);
        }
    }

    if (ui->true_scale) {
        snprintf(buf, sizeof(buf), "TRUE SCALE");
        float w = gfx_text_w(g, buf, FONT_UI_TINY) + 16 * app->dpi;
        SsRect chip = rect_make(vp.x + vp.w - w - 18 * app->dpi, vp.y + 14 * app->dpi, w, 18 * app->dpi);
        gfx_fill_round(g, chip, 9 * app->dpi, PS(RGBA(220, 160, 60, 60)));
        gfx_text(g, chip.x + chip.w / 2, chip.y + chip.h / 2, buf, FONT_UI_TINY, RGBA(255, 220, 170, 230),
                 SS_ALIGN_CENTER, SS_VALIGN_MIDDLE);
    }
    draw_scale_bar(app, g, vp);
}

static void dashed_line(SsGfx *g, float x0, float y0, float x1, float y1, float dash,
                        float gap, float width, SsPaint p) {
    float dx = x1 - x0, dy = y1 - y0;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 1.0f) return;
    dx /= len;
    dy /= len;
    for (float t = 0; t < len; t += dash + gap) {
        float e = t + dash < len ? t + dash : len;
        gfx_line(g, x0 + dx * t, y0 + dy * t, x0 + dx * e, y0 + dy * e, width, p);
    }
}

/* World X/Y axes through the barycentre, in the SSG style. */
static void draw_axes(App *app, SsGfx *g) {
    UI *ui = &app->ui;
    if (ui->zoom < 1e-6) return;
    double ax = (ui->viewport.w * 0.5 + 60.0) / ui->zoom;
    double ay = (ui->viewport.h * 0.5 + 60.0) / (ui->zoom * fabs(sin(ui->pitch)) + 1e-9);
    Vec3 xa = {-ax, 0, 0}, xb = {ax, 0, 0}, ya = {0, -ay, 0}, yb = {0, ay, 0};
    float x0, y0, x1, y1;
    scene_project(app, xa, &x0, &y0, NULL);
    scene_project(app, xb, &x1, &y1, NULL);
    dashed_line(g, x0, y0, x1, y1, 7 * app->dpi, 6 * app->dpi, 1.0f * app->dpi,
                PS(RGBA(236, 120, 120, 70)));
    scene_project(app, ya, &x0, &y0, NULL);
    scene_project(app, yb, &x1, &y1, NULL);
    dashed_line(g, x0, y0, x1, y1, 7 * app->dpi, 6 * app->dpi, 1.0f * app->dpi,
                PS(RGBA(120, 224, 160, 66)));
    /* ticks on a 1/2/5 sequence so the grid reads as a ruler */
    double target = ax / 6.0;
    double mag = pow(10.0, floor(log10(target > 1e-9 ? target : 1e-9)));
    double step = mag;
    if (target / mag >= 5.0) step = mag * 5.0;
    else if (target / mag >= 2.0) step = mag * 2.0;
    for (double v = -floor(ax / step) * step; v <= ax; v += step) {
        float tx, ty;
        scene_project(app, (Vec3){v, 0, 0}, &tx, &ty, NULL);
        gfx_line(g, tx, ty - 3 * app->dpi, tx, ty + 3 * app->dpi, 1.0f,
                 PS(RGBA(236, 120, 120, 70)));
        scene_project(app, (Vec3){0, v, 0}, &tx, &ty, NULL);
        gfx_line(g, tx - 3 * app->dpi, ty, tx + 3 * app->dpi, ty, 1.0f,
                 PS(RGBA(120, 224, 160, 66)));
    }
    float lx = CLAMP(x1, ui->viewport.x + 10 * app->dpi,
                     ui->viewport.x + ui->viewport.w - 14 * app->dpi);
    float ly = CLAMP(y1, ui->viewport.y + 10 * app->dpi,
                     ui->viewport.y + ui->viewport.h - 14 * app->dpi);
    gfx_text(g, lx, ly, "X", FONT_UI_TINY, RGBA(236, 140, 140, 160),
             SS_ALIGN_LEFT, SS_VALIGN_MIDDLE);
    scene_project(app, yb, &x1, &y1, NULL);
    lx = CLAMP(x1, ui->viewport.x + 10 * app->dpi,
               ui->viewport.x + ui->viewport.w - 14 * app->dpi);
    ly = CLAMP(y1, ui->viewport.y + 10 * app->dpi,
               ui->viewport.y + ui->viewport.h - 14 * app->dpi);
    gfx_text(g, lx, ly, "Y", FONT_UI_TINY, RGBA(140, 224, 170, 160),
             SS_ALIGN_CENTER, SS_VALIGN_MIDDLE);
}

/* ----------------------------------------------------------------- main */

void scene_draw(App *app, SsGfx *g, SsRect vp) {
    UI *ui = &app->ui;
    if (vp.w <= 1 || vp.h <= 1) return;
    gfx_push_clip(g, vp);

    /* background wash and two soft nebulae tinted by the active theme */
    const SsTheme *th = sss_theme();
    gfx_fill_rect(g, vp, paint_linv(0, vp.y, vp.y + vp.h, th->scene_top, th->scene_bottom));
    if (!ui->low_power) {
        gfx_fill_circle(g, vp.x + vp.w * 0.18f, vp.y + vp.h * 0.22f, vp.w * 0.55f,
                        paint_rad(vp.x + vp.w * 0.18f, vp.y + vp.h * 0.22f, vp.w * 0.55f,
                                  th->nebula_a, col_with_alpha(th->nebula_a, 0)));
        gfx_fill_circle(g, vp.x + vp.w * 0.86f, vp.y + vp.h * 0.82f, vp.w * 0.5f,
                        paint_rad(vp.x + vp.w * 0.86f, vp.y + vp.h * 0.82f, vp.w * 0.5f,
                                  th->nebula_b, col_with_alpha(th->nebula_b, 0)));
    }
    draw_starfield(app, g, vp, app->now);

    /* orbits (behind everything else) */
    if (ui->show_orbits) {
        for (int i = 0; i < app->sim.count; i++) {
            Body *b = &app->sim.bodies[i];
            if (!b->alive) continue;
            if (b->kind == BT_STAR) continue;
            int sel = b->id == ui->selected_id;
            int hov = b->id == ui->hover_id;
            draw_orbit_for(app, g, i, sel, hov);
        }
    }

    if (ui->show_axes) draw_axes(app, g);

    BodyScreen *list = (BodyScreen *)sss_alloc(sizeof(BodyScreen) * MAX_BODIES);
    if (!list) {
        gfx_pop_clip(g);
        return;
    }
    int n = 0;
    layout_bodies(app, list, &n);

    /* trails */
    if (ui->show_trails) {
        for (int i = 0; i < app->sim.count; i++) {
            Body *b = &app->sim.bodies[i];
            if (!b->alive || b->trail_count < 2) continue;
            int count = b->trail_count;
            float x0 = 0, y0 = 0;
            int have = 0;
            int steps = count > 220 ? count / 220 : 1;
            for (int k = 0; k < count; k += steps) {
                int idx = (b->trail_head - count + k + TRAIL_MAX * 2) % TRAIL_MAX;
                float x, y;
                scene_project(app, b->trail[idx], &x, &y, NULL);
                if (have) {
                    float a = (float)k / (float)count;
                    SsColor c = col_with_alpha(b->accent, (int)(12 + a * 90));
                    gfx_line(g, x0, y0, x, y, (0.9f + a * 1.5f) * app->dpi, PS(c));
                }
                x0 = x;
                y0 = y;
                have = 1;
            }
        }
    }

    /* bodies */
    int hovered_idx = -1;
    for (int i = 0; i < n; i++) {
        BodyScreen *bs = &list[i];
        Body *b = &app->sim.bodies[bs->body];
        if (b->is_sun && !ui->show_sun) continue;
        if (bs->x < vp.x - 80 || bs->x > vp.x + vp.w + 80 || bs->y < vp.y - 80 ||
            bs->y > vp.y + vp.h + 80) {
            /* keep the star glow soft even when slightly off-screen */
            if (b->kind != BT_STAR) continue;
        }
        draw_planet_body(app, g, b, bs->x, bs->y, bs->r, bs->depth);
        if (b->id == ui->selected_id) {
            draw_selection_marker(app, g, bs->x, bs->y, bs->r);
        }
        if (b->id == ui->hover_id) hovered_idx = bs->body;
    }

    /* Labels on top.  A greedy solver tries eight positions around each body,
     * keeps the first one that clears both the labels already placed and the
     * star's disc, and simply drops the label when nothing fits.  The selected
     * body wins over the Sun, which wins over planets, which win over moons. */
    SsRect placed[96];
    int placed_n = 0;
    int order[MAX_BODIES];
    int order_n = 0;
    int star_idx = -1;
    for (int i = 0; i < n; i++) {
        if (app->sim.bodies[list[i].body].kind == BT_STAR) {
            star_idx = i;
            break;
        }
    }
    float star_x = 0, star_y = 0, star_r = 0;
    if (star_idx >= 0) {
        star_x = list[star_idx].x;
        star_y = list[star_idx].y;
        star_r = list[star_idx].r;
    }

    if (hovered_idx >= 0 && rect_contains(vp, ui->mouse_x, ui->mouse_y)) {
        int si = body_screen_index(list, n, hovered_idx);
        SsRect chip;
        if (si >= 0 && hover_chip_rect(app, g, list, si, vp, &chip) && placed_n < 90)
            placed[placed_n++] = rect_inset(chip, -4 * app->dpi);
    }

    for (int i = 0; i < n; i++) {
        BodyScreen *bs = &list[i];
        Body *b = &app->sim.bodies[bs->body];
        int selected = b->id == ui->selected_id;
        int hovered = b->id == ui->hover_id;
        int moon = b->parent >= 0;
        if (b->is_sun && !ui->show_sun) continue;
        int important = b->kind == BT_STAR || b->kind == BT_GIANT || b->kind == BT_ICE ||
                        b->kind == BT_TERRESTRIAL || b->kind == BT_DWARF;
        if (hovered) continue;                     /* the hover chip names it */
        if (!ui->show_labels && !selected) continue;
        if (moon && !selected && !ui->show_moon_names) continue;
        if (!important && !selected) continue;
        if (bs->x < vp.x - 2 || bs->x > vp.x + vp.w + 2 || bs->y < vp.y - 2 ||
            bs->y > vp.y + vp.h + 2)
            continue;
        /* planets buried inside the star's disc cannot be seen or labelled */
        if (!selected && b->kind != BT_STAR && star_idx >= 0) {
            float dx = bs->x - star_x, dy = bs->y - star_y;
            if (sqrtf(dx * dx + dy * dy) < star_r * 1.08f) continue;
        }
        order[order_n++] = i;
    }
    /* insertion sort, strongest label first */
    for (int i = 1; i < order_n; i++) {
        int key = order[i];
        int keyp = label_priority(app, &app->sim.bodies[list[key].body]);
        int j = i - 1;
        while (j >= 0 && label_priority(app, &app->sim.bodies[list[order[j]].body]) < keyp) {
            order[j + 1] = order[j];
            j--;
        }
        order[j + 1] = key;
    }

    for (int oi = 0; oi < order_n; oi++) {
        BodyScreen *bs = &list[order[oi]];
        Body *b = &app->sim.bodies[bs->body];
        int selected = b->id == ui->selected_id;
        int moon = b->parent >= 0;
        float tw = gfx_text_w(g, b->name, selected ? FONT_UI_BOLD : FONT_UI_SMALL);
        float th = 14.0f * app->dpi;
        float gap = 5.0f * app->dpi;
        float rr = bs->r;
        if (b->kind == BT_STAR) rr *= 1.06f;
        float cx[8], cy[8];
        for (int c = 0; c < 8; c++) {
            cx[c] = bs->x;
            cy[c] = bs->y;
        }
        float edge = rr + gap;
        cx[0] = bs->x;                     cy[0] = bs->y + edge;            /* below */
        cx[1] = bs->x;                     cy[1] = bs->y - edge - th;       /* above */
        cx[2] = bs->x + edge + tw * 0.5f;  cy[2] = bs->y - th * 0.5f;      /* right */
        cx[3] = bs->x - edge - tw * 0.5f;  cy[3] = bs->y - th * 0.5f;      /* left */
        cx[4] = bs->x + edge * 0.72f + tw * 0.5f; cy[4] = bs->y + edge * 0.72f;
        cx[5] = bs->x - edge * 0.72f - tw * 0.5f; cy[5] = bs->y + edge * 0.72f;
        cx[6] = bs->x + edge * 0.72f + tw * 0.5f; cy[6] = bs->y - edge * 0.72f - th;
        cx[7] = bs->x - edge * 0.72f - tw * 0.5f; cy[7] = bs->y - edge * 0.72f - th;

        SsRect lr;
        int ok = 0;
        for (int c = 0; c < 8 && !ok; c++) {
            lr = rect_make(cx[c] - tw * 0.5f, cy[c], tw, th);
            if (lr.x < vp.x + 4) lr.x = vp.x + 4;
            if (lr.x + lr.w > vp.x + vp.w - 4) lr.x = vp.x + vp.w - 4 - lr.w;
            if (lr.y < vp.y + 4) lr.y = vp.y + 4;
            if (lr.y + lr.h > vp.y + vp.h - 4) lr.y = vp.y + vp.h - 4 - lr.h;
            int clash = 0;
            for (int k = 0; k < placed_n && !clash; k++) {
                SsRect inter;
                if (rect_intersect(rect_inset(placed[k], -3.0f * app->dpi), lr, &inter)) clash = 1;
            }
            /* never cover another body's disc, whatever its size */
            for (int bi = 0; bi < n && !clash; bi++) {
                if (bi == order[oi]) continue;
                if (rect_point_distance(lr, list[bi].x, list[bi].y) <
                    list[bi].r + 3.0f * app->dpi)
                    clash = 1;
            }
            if (!clash) ok = 1;
        }
        if (!ok) {
            if (!selected) continue;
            lr = rect_make(bs->x - tw * 0.5f, bs->y + edge, tw, th);
        }
        if (placed_n < (int)ARRAY_LEN(placed)) placed[placed_n++] = lr;
        draw_label_rect(app, g, b->name, lr, selected, 0, moon);
    }

    /* selection readout near the cursor / body */
    if (hovered_idx >= 0 && rect_contains(vp, ui->mouse_x, ui->mouse_y)) {
        int si = body_screen_index(list, n, hovered_idx);
        SsRect chip;
        if (si >= 0 && hover_chip_rect(app, g, list, si, vp, &chip)) {
            Body *b = &app->sim.bodies[hovered_idx];
            char sub[64];
            double d = body_distance_to_parent(b, &app->sim);
            if (d * KM_PER_AU > 0.02 * KM_PER_AU) snprintf(sub, sizeof(sub), "%.3f AU", d);
            else snprintf(sub, sizeof(sub), "%.0f km", d * KM_PER_AU);
            const SsTheme *th = sss_theme();
            gfx_fill_round(g, chip, 5 * app->dpi, PS(th->chip_bg));
            gfx_stroke_round(g, chip, 5 * app->dpi, 1.0f, PS(th->chip_border));
            gfx_text(g, chip.x + chip.w * 0.5f, chip.y + 5 * app->dpi, b->name, FONT_UI_SMALL,
                     th->chip_text, SS_ALIGN_CENTER, SS_VALIGN_TOP);
            gfx_text(g, chip.x + chip.w * 0.5f, chip.y + 18 * app->dpi, sub, FONT_MONO,
                     th->chip_dim, SS_ALIGN_CENTER, SS_VALIGN_TOP);
        }
    }

    draw_hud(app, g, vp);

    /* vignette: darken the four edges, leaving the middle untouched */
    float vg = 74.0f * app->dpi;
    int va = sss_theme()->vignette;
    if (va > 0) {
        gfx_fill_rect(g, rect_make(vp.x, vp.y, vp.w, vg),
                      paint_linv(0, vp.y, vp.y + vg, RGBA(0, 0, 0, va), RGBA(0, 0, 0, 0)));
        gfx_fill_rect(g, rect_make(vp.x, vp.y + vp.h - vg * 1.6f, vp.w, vg * 1.6f),
                      paint_linv(0, vp.y + vp.h - vg * 1.6f, vp.y + vp.h, RGBA(0, 0, 0, 0),
                                 RGBA(0, 0, 0, va * 10 / 11)));
        gfx_fill_rect(g, rect_make(vp.x, vp.y, vg, vp.h),
                      paint_lin(vp.x, 0, vp.x + vg, 0, RGBA(0, 0, 0, va * 3 / 4),
                                RGBA(0, 0, 0, 0)));
        gfx_fill_rect(g, rect_make(vp.x + vp.w - vg, vp.y, vg, vp.h),
                      paint_lin(vp.x + vp.w - vg, 0, vp.x + vp.w, 0, RGBA(0, 0, 0, 0),
                                RGBA(0, 0, 0, va * 3 / 4)));
    }
    sss_free(list);
    gfx_pop_clip(g);
    (void)hovered_idx;
}

/* ------------------------------------------------------------------ pick */

int scene_pick(App *app, float x, float y) {
    BodyScreen *list = (BodyScreen *)sss_alloc(sizeof(BodyScreen) * MAX_BODIES);
    if (!list) return 0;
    int n = 0;
    layout_bodies(app, list, &n);
    int best = 0;
    double best_d = 1e18;
    for (int i = 0; i < n; i++) {
        BodyScreen *bs = &list[i];
        if (app->sim.bodies[bs->body].is_sun && !app->ui.show_sun) continue;
        double dx = x - bs->x, dy = y - bs->y;
        double d = sqrt(dx * dx + dy * dy);
        double hit = bs->r + 7.0 * app->dpi;
        if (d <= hit && d < best_d) {
            best_d = d;
            best = bs->body;
        }
    }
    int id = best >= 0 ? app->sim.bodies[best].id : 0;
    sss_free(list);
    return id;
}
