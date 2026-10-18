/* Headless screenshot harness: renders the real UI through gfx_soft.c. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "app.h"
#include "theme.h"

static App *app;
static SsSurface *surface;
static SsGfx *gfx;

static void render(const char *path) {
    app_draw(app, gfx);
    surf_write_ppm(surface, path);
    printf("wrote %s\n", path);
}

static void ev_move(float x, float y) { UiEvent e; memset(&e,0,sizeof e); e.type=UI_EV_MOVE; e.x=x; e.y=y; app_event(app,&e); }
static void ev_down(float x, float y, int btn) { UiEvent e; memset(&e,0,sizeof e); e.type=UI_EV_DOWN; e.x=x; e.y=y; e.button=btn; app_event(app,&e); }
static void ev_up(float x, float y) { UiEvent e; memset(&e,0,sizeof e); e.type=UI_EV_UP; e.x=x; e.y=y; app_event(app,&e); }
static void ev_key(int k) { UiEvent e; memset(&e,0,sizeof e); e.type=UI_EV_KEY; e.key=k; app_event(app,&e); }
static void ev_wheel(float x, float y, float d) { UiEvent e; memset(&e,0,sizeof e); e.type=UI_EV_WHEEL; e.x=x; e.y=y; e.wheel=d; app_event(app,&e); }
static void click(float x, float y) { ev_move(x,y); ev_down(x,y,0); ev_up(x,y); }
static void settle(int frames) { for (int i=0;i<frames;i++){ app_tick(app, 1.0/60.0); app_draw(app, gfx); } }

int main(void) {
    surface = surf_create(1280, 800);
    gfx = gfx_create(surface);
    app = (App*)sss_alloc(sizeof(App));
    app_init(app, 1280, 800);
    app->dpi = 1.0f;
    app->ui.zoom = 46;
    app->ui.mouse_x = 1150; app->ui.mouse_y = 700;
    app->fps = 60;
    settle(30);
    render("/tmp/shot_01_overview.ppm");

    /* pick Earth in the viewport: find its screen position through scene_pick candidates */
    int earth_id = 0;
    for (int i=0;i<app->sim.count;i++) if (app->sim.bodies[i].alive && strcmp(app->sim.bodies[i].name,"Earth")==0) earth_id = app->sim.bodies[i].id;
    app_select(app, earth_id);
    ev_move(430, 228); /* hover Jupiter so the readout chip is exercised */
    settle(4);
    render("/tmp/shot_02_selected.ppm");

    /* create tab: tab bar sits at the top of the inspector (three equal tabs) */
    click(1120, 118);
    settle(2);
    render("/tmp/shot_03_create.ppm");

    /* detail view: focus Saturn so the rings fill the frame */
    int saturn_id = 0;
    for (int i = 0; i < app->sim.count; i++)
        if (app->sim.bodies[i].alive && strcmp(app->sim.bodies[i].name, "Saturn") == 0)
            saturn_id = app->sim.bodies[i].id;
    app_select(app, saturn_id);
    settle(2);
    click(1048, 397); /* Focus */
    settle(10);
    render("/tmp/shot_04_focus.ppm");
    app_reset_view(app);
    app->ui.zoom = 46;

    /* library tab: parts, texture packs and mods scanned from content/ */
    content_init("content", "build/tmp/render_user");
    app_rescan_content(app);
    click(1218, 118);
    settle(2);
    app_apply_pack(app, 0);
    settle(2);
    render("/tmp/shot_11_library.ppm");
    app_apply_pack(app, -1);
    click(1075, 118); /* back to the create tab */
    settle(1);

    /* theme gallery: the picker itself, then every palette over the overview */
    click(1218, 62); /* the palette button in the transport bar */
    settle(2);
    render("/tmp/shot_05_themes.ppm");
    click(1218, 62);

    app_select(app, earth_id);
    app_reset_view(app);
    app->ui.zoom = 46;
    for (int i = 0; i < sss_theme_count(); i++) {
        app_set_theme(app, i);
        char path[128];
        snprintf(path, sizeof(path), "/tmp/shot_%02d_theme_%s.ppm", 6 + i, sss_theme_at(i)->id);
        settle(2);
        render(path);
    }
    app_set_theme(app, 0);

    /* simulation panel: start date, view switches, auto track, separation */
    app_select(app, earth_id);
    app_reset_view(app);
    app->ui.zoom = 46;
    ev_key('C');
    settle(2);
    app->sim.paused = 1;
    app_set_start_date(app, 2029, 11, 29);
    app_set_track(app, saturn_id);
    settle(30);
    render("/tmp/shot_12_simulation.ppm");
    click(150, 413); /* auto-track dropdown */
    settle(2);
    render("/tmp/shot_13_track.ppm");

    printf("done\n");
    return 0;
}
