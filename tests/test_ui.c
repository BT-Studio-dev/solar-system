/* Headless checks for the simulation panel: start date, view switches,
 * auto-track picker and the object-separation readout.
 *
 * The clicks use offsets inside ui.panel_rect / ui.picker_rect, so the layout
 * can move without invalidating the test.
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "app.h"
#include "theme.h"

static App *app;
static SsSurface *surf;
static SsGfx *g;
static int failures;

#define CHECK(c, msg) do { if (!(c)) { printf("FAIL %s (line %d)\n", msg, __LINE__); failures++; } } while (0)

static void draw(void) { app_draw(app, g); }

static void ev_move(float x, float y) { UiEvent e; memset(&e, 0, sizeof e); e.type = UI_EV_MOVE; e.x = x; e.y = y; app_event(app, &e); }
static void ev_down(float x, float y, int b) { UiEvent e; memset(&e, 0, sizeof e); e.type = UI_EV_DOWN; e.x = x; e.y = y; e.button = b; app_event(app, &e); }
static void ev_up(float x, float y) { UiEvent e; memset(&e, 0, sizeof e); e.type = UI_EV_UP; e.x = x; e.y = y; app_event(app, &e); }
static void ev_key(int k) { UiEvent e; memset(&e, 0, sizeof e); e.type = UI_EV_KEY; e.key = k; app_event(app, &e); }
static void click(float x, float y) { ev_move(x, y); ev_down(x, y, 0); ev_up(x, y); }

static int body_index(const char *name) {
    for (int i = 0; i < app->sim.count; i++)
        if (app->sim.bodies[i].alive && strcmp(app->sim.bodies[i].name, name) == 0) return i;
    return -1;
}

static int alive_rank(int body_idx) {
    int n = 0;
    for (int i = 0; i < app->sim.count; i++) {
        if (!app->sim.bodies[i].alive) continue;
        if (i == body_idx) return n;
        n++;
    }
    return -1;
}

/* Panel layout, in dpi-scaled pixels from the panel origin. */
#define PANEL_PAD     (16.0f)
#define DATE_TOP      (38.0f)
#define FIELD_TOP     (DATE_TOP + 16.0f)
#define FIELD_UP_Y    (FIELD_TOP + 43.0f)
#define SET_Y         (FIELD_TOP + 61.0f)
#define VIEW_TOP      (SET_Y + 32.0f + 16.0f)
#define TRACK_Y       (VIEW_TOP + 5 * 22.0f + 6.0f + 16.0f)
#define SEP_Y         (TRACK_Y + 34.0f + 16.0f)

int main(void) {
    surf = surf_create(1280, 800);
    g = gfx_create(surf);
    app = (App *)sss_alloc(sizeof(App));
    app_init(app, 1280, 800);
    app->dpi = 1.0f;
    app->fps = 60;
    draw();

    ev_key('C');
    draw();
    CHECK(app->ui.panel_open == 1, "C opens the controls panel");
    SsRect p = app->ui.panel_rect;
    CHECK(p.w > 280 && p.h > 400, "panel has a sensible size");
    CHECK(app->ui.date_year >= 1800 && app->ui.date_month >= 1 && app->ui.date_month <= 12,
          "date fields mirror sim.t");

    /* pointer drags over the panel must not move the camera */
    Vec3 before = app->ui.center;
    ev_down(p.x + 100, p.y + 200, 0);
    ev_move(p.x + 220, p.y + 200);
    ev_up(p.x + 220, p.y + 200);
    CHECK(fabs(app->ui.center.x - before.x) < 1e-9 &&
          fabs(app->ui.center.y - before.y) < 1e-9, "panel swallows drags");

    /* year / month / day steppers, then Set start date */
    int y0 = app->ui.date_year, m0 = app->ui.date_month;
    float yw = 88.0f, mw = 56.0f, gap = 8.0f;
    for (int i = 0; i < 3; i++) click(p.x + PANEL_PAD + yw * 0.75f, p.y + FIELD_UP_Y);
    click(p.x + PANEL_PAD + yw + gap + mw * 0.75f, p.y + FIELD_UP_Y);
    CHECK(app->ui.date_year == y0 + 3, "year stepper adds three years");
    CHECK(app->ui.date_month == (m0 % 12) + 1, "month stepper wraps December to January");
    click(p.x + PANEL_PAD + 70, p.y + SET_Y + 13);
    draw();
    double want = date_to_jd(app->ui.date_year, app->ui.date_month, app->ui.date_day, 0, 0, 0);
    CHECK(fabs(app->sim.t - want) < 1e-6, "Set start date drives sim.t");

    /* view switches (row 0 right, row 2 left, row 3 left) */
    float cellw = (p.w - 32 - 8) * 0.5f;
    click(p.x + PANEL_PAD + cellw + 20, p.y + VIEW_TOP + 10);
    draw();
    CHECK(app->ui.show_fade_orbits == 1, "fade orbits toggles");
    click(p.x + PANEL_PAD + 20, p.y + VIEW_TOP + 2 * 22 + 10);
    draw();
    CHECK(app->ui.show_sun == 0, "sun toggles");
    click(p.x + PANEL_PAD + 20, p.y + VIEW_TOP + 3 * 22 + 10);
    draw();
    CHECK(app->ui.simplified == 1, "simplified toggles");
    click(p.x + PANEL_PAD + 20, p.y + VIEW_TOP + 2 * 22 + 10);
    click(p.x + PANEL_PAD + 20, p.y + VIEW_TOP + 3 * 22 + 10);
    click(p.x + PANEL_PAD + cellw + 20, p.y + VIEW_TOP + 10);

    /* auto track: open the popup and click Earth */
    int earth = body_index("Earth");
    CHECK(earth >= 0, "Earth exists in the full preset");
    click(p.x + PANEL_PAD + 40, p.y + TRACK_Y + 14);
    draw();
    CHECK(app->ui.picker_open == 1 && app->ui.picker_target == 1, "track picker opens");
    float row_y = app->ui.picker_rect.y + 34 + (alive_rank(earth) + 1) * 25 + 12;
    click(app->ui.picker_rect.x + 30, row_y);
    draw();
    CHECK(app->ui.picker_open == 0, "picking a body closes the popup");
    CHECK(app->ui.follow_id == app->sim.bodies[earth].id, "auto track follows Earth");

    /* object separation: clear B, then read Sun-Earth */
    click(p.x + PANEL_PAD + (p.w - 32 + 8) * 0.5f + 20, p.y + SEP_Y + 14);
    draw();
    CHECK(app->ui.picker_open == 1 && app->ui.picker_target == 3, "separation picker opens");
    click(app->ui.picker_rect.x + 30, app->ui.picker_rect.y + 34 + 12);
    draw();
    CHECK(app->ui.sep_b_id == 0, "separation B clears");
    CHECK(app_separation_au(app, app->ui.sep_a_id, app->ui.sep_b_id) < 0,
          "no distance without B");

    app->ui.sep_a_id = app->sim.bodies[body_index("Sun")].id;
    app->ui.sep_b_id = app->sim.bodies[body_index("Earth")].id;
    double au = app_separation_au(app, app->ui.sep_a_id, app->ui.sep_b_id);
    CHECK(au > 0.9 && au < 1.1, "Sun-Earth separation is about one AU");

    /* start date clamping, leap years */
    app_set_start_date(app, 1700, 13, 40);
    CHECK(app->ui.date_year == 1800 && app->ui.date_month == 12 &&
          app->ui.date_day == 31, "start date clamps to 1800-12-31");
    app_set_start_date(app, 2100, 2, 30);
    CHECK(app->ui.date_year == 2100 && app->ui.date_month == 2 &&
          app->ui.date_day == 28, "2100 is not a leap year");
    app_set_start_date(app, 2029, 11, 29);
    CHECK(fabs(app->sim.t - date_to_jd(2029, 11, 29, 0, 0, 0)) < 1e-6,
          "2029-11-29 lands at midnight UTC");

    /* Escape closes the panel */
    ev_key(UI_KEY_ESC);
    draw();
    CHECK(app->ui.panel_open == 0, "Escape closes the panel");

    /* A 1024x600-class window: the panel scrolls instead of overflowing. */
    App *big_app = app;
    SsGfx *big_g = g;
    SsSurface *small_surf = surf_create(1008, 481);
    SsGfx *small_g = gfx_create(small_surf);
    App *small = (App *)sss_alloc(sizeof(App));
    app_init(small, 1008, 481);
    small->dpi = 1.0f;
    small->fps = 60;
    app = small;
    g = small_g;
    draw();
    ev_key('C');
    draw();
    SsRect sp = small->ui.panel_rect;
    CHECK(sp.y + sp.h <= 481 - 26, "panel stops above the status bar");
    CHECK(small->ui.panel_scroll_max > 0, "short windows expose a scroll range");
    CHECK(small->ui.panel_scroll == 0, "panel starts at the top");
    UiEvent we;
    memset(&we, 0, sizeof we);
    we.type = UI_EV_WHEEL;
    we.x = sp.x + 100;
    we.y = sp.y + 200;
    we.wheel = -1;
    app_event(small, &we);
    draw();
    CHECK(small->ui.panel_scroll > 0, "wheel scrolls the panel");
    float sep_y = sp.y + SEP_Y + 14 - small->ui.panel_scroll;
    click(sp.x + PANEL_PAD + 40, sep_y);
    draw();
    CHECK(small->ui.picker_open == 1 && small->ui.picker_target == 2,
          "separation A is reachable after scrolling");
    ev_key(UI_KEY_ESC);   /* the picker first */
    ev_key(UI_KEY_ESC);   /* then the panel */
    CHECK(small->ui.panel_open == 0, "Escape closes the short panel");

    /* the lighter scene path renders without changing the simulation */
    small->ui.low_power = 1;
    double t_before = small->sim.t;
    draw();
    CHECK(fabs(small->sim.t - t_before) < 1e-9, "low-power draw leaves the clock alone");

    gfx_destroy(small_g);
    surf_destroy(small_surf);
    sss_free(small);
    app = big_app;
    g = big_g;

    printf(failures ? "RESULT FAIL (%d)\n" : "RESULT PASS\n", failures);
    return failures ? 1 : 0;
}
