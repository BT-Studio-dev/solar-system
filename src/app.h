/* app.h -- application state, shared by the Win32 shell and the test harness. */
#ifndef SSS_APP_H
#define SSS_APP_H

#include "core.h"
#include "sim.h"
#include "gfx.h"
#include "content.h"

/* ------------------------------------------------------------------- input */

enum {
    UI_EV_NONE = 0,
    UI_EV_MOVE,
    UI_EV_DOWN,
    UI_EV_UP,
    UI_EV_WHEEL,
    UI_EV_KEY,
    UI_EV_RESIZE,
    UI_EV_DBLCLICK
};

enum { UI_MB_LEFT = 0, UI_MB_RIGHT = 1, UI_MB_MIDDLE = 2 };

enum {
    UI_KEY_NONE = 0,
    UI_KEY_SPACE = 32,
    UI_KEY_ESC = 27,
    UI_KEY_ENTER = 13,
    UI_KEY_DELETE = 46,
    UI_KEY_F1 = 112,
    UI_KEY_F2, UI_KEY_F3, UI_KEY_F4, UI_KEY_F5, UI_KEY_F6, UI_KEY_F7, UI_KEY_F8,
    UI_KEY_F9, UI_KEY_F10, UI_KEY_F11, UI_KEY_F12,
    UI_KEY_LEFT = 200, UI_KEY_RIGHT, UI_KEY_UP, UI_KEY_DOWN,
    UI_KEY_PLUS, UI_KEY_MINUS
};

typedef struct {
    int type;
    float x, y;
    int button;
    float wheel;
    int key;
    int shift, ctrl, alt;
} UiEvent;

/* --------------------------------------------------------------------- UI */

typedef struct {
    /* camera */
    Vec3 center;
    double zoom;      /* pixels per AU */
    float yaw;        /* radians */
    float pitch;      /* radians, 0 = edge-on, pi/2 = top-down */
    Vec3 cam_right, cam_up, cam_fwd;

    int selected_id;
    int follow_id;
    int hover_id;

    int show_orbits, show_labels, show_trails, show_moons, true_scale;
    int show_fade_orbits;  /* trails fade behind the body */
    int show_moon_names;   /* label satellites as well as planets */
    int show_sun;          /* draw the central star */
    int show_axes;         /* world X/Y axes through the barycentre */
    int simplified;        /* flat discs instead of shaded spheres */
    int tab;          /* 0 = inspect, 1 = add body, 2 = presets */
    int preset_open;
    int theme_open;
    int help_open;
    float library_scroll; /* content library scroll offset, px */
    float library_content_h;
    SsRect library_area;

    /* simulation panel (start date, view switches, object separation) */
    int panel_open;
    SsRect panel_rect;
    float panel_scroll;      /* content offset when the panel is height-limited */
    float panel_scroll_max;
    int low_power;           /* 1 GB / 1 CPU machines render the lighter path */
    int picker_open;       /* body list popup: auto-track or separation */
    int picker_target;     /* 1 = auto-track, 2 = object A, 3 = object B */
    int picker_scroll;
    float picker_content_h;
    SsRect picker_rect;
    int date_year, date_month, date_day;
    int sep_a_id, sep_b_id;

    /* interaction state */
    int drag_mode;    /* 0 none, 1 pan, 2 orbit */
    float drag_x, drag_y;
    int dragging_speed;
    uint32_t hot_id, active_id;
    float hover_anim;
    double pulse;

    /* add-body composer */
    int add_kind;
    float add_mass_log;    /* log10 kg, 18..31 */
    float add_radius_log;  /* log10 km, -1..6 */
    float add_orbit_log;   /* log10 AU, -0.3..1.6 */
    float add_ecc;
    int add_ecc_dir;
    int add_count;

    /* a Part from the Library loaded into the composer */
    int part_colors;
    char part_name[CONTENT_NAME];
    SsColor part_col1, part_col2, part_accent;
    int part_ringed;
    double part_ring_in, part_ring_out;
    SsColor part_ring_col;

    /* overlays */
    char toast[160];
    double toast_until;
    int toast_kind;

    /* hit-test regions captured by the last draw pass */
    int hit_count;
    uint32_t hit_id[512];
    SsRect hit_rect[512];
    int hit_order[512];
    float mouse_x, mouse_y;
    int mouse_down;
    int press_consumed;

    /* caches filled by layout each frame */
    SsRect viewport;
    SsRect titlebar;
    SsRect transport;
    SsRect inspector;
    SsRect status;
} UI;

/* -------------------------------------------------------------------- App */

typedef struct App App;

struct App {
    Sim sim;
    UI ui;
    int width, height;
    float dpi;
    double now;       /* wall seconds since start */
    double last_tick;
    int running;
    int32_t quit;
    int32_t show_fps;
    int32_t last_input_ms;
    float fps;
    int theme_index;
    int pack_index;       /* active texture pack, -1 when off */

    /* platform hooks (set by main_win32.c; optional in the harness) */
    void (*on_save)(App *);
    void (*on_load)(App *);
    void (*on_screenshot)(App *);
    void (*on_quit)(App *);
    void (*on_minimize)(App *);
    void (*on_maximize)(App *);
    void (*on_toast)(App *, const char *text, int kind);
    void (*on_theme)(App *, int index);
    /* Reads a file the content scanner found; returns a block from sss_alloc. */
    char *(*read_text)(const char *utf8_path, int *out_len);
    /* Opens a folder in the platform file manager. */
    void (*open_path)(App *, const char *utf8_path);

    /* scene scratch buffers (heap-owned) */
    void *scene_scratch;
};

void app_init(App *app, int width, int height);
void app_shutdown(App *app);
void app_resize(App *app, int width, int height);

/* Advance simulation + animations. dt in seconds. */
void app_tick(App *app, double dt);

/* Draw the whole window into g (0,0 is the client origin). */
void app_draw(App *app, SsGfx *g);

/* Feed one input event. */
void app_event(App *app, const UiEvent *ev);

/* Commands (also bound to keyboard shortcuts). */
void app_apply_preset(App *app, int preset);
void app_select(App *app, int body_id);
void app_focus_selected(App *app);
void app_reset_view(App *app);
void app_add_custom(App *app);
void app_remove_selected(App *app);
void app_set_time_now(App *app);
/* Jump the ephemerides to 00:00 UTC on the given date (entry clamped to
 * 1800-2100; the JPL fit is sharpest over 1800-2050). */
void app_set_start_date(App *app, int year, int month, int day);
/* Copy the current simulation date into the panel's year/month/day fields. */
void app_sync_date_fields(App *app);
/* Auto-track a body (0 = free camera). */
void app_set_track(App *app, int body_id);
/* Distance between two bodies in AU, or -1 when either id is missing. */
double app_separation_au(const App *app, int a_id, int b_id);
void app_set_theme(App *app, int index);
void app_next_theme(App *app);

/* content folders */
void app_rescan_content(App *app);
void app_use_part(App *app, int index);      /* fills the create panel */
void app_apply_pack(App *app, int index);    /* -1 restores the theme colours */
void app_open_mod(App *app, int index);      /* loads Mods/Custom_/<file>.sims */
/* Tint for a body's lit colour under the active texture pack. */
SsColor app_body_tint(const App *app, SsColor base, int kind);
void app_toast(App *app, const char *text, int kind);
int app_save_text(App *app, char *buf, int cap);
void app_load_text(App *app, const char *text);

/* scene.c */
void scene_draw(App *app, SsGfx *g, SsRect viewport);
void scene_project(App *app, Vec3 p, float *sx, float *sy, float *depth);
void scene_project_dir(App *app, Vec3 p, float *sx, float *sy);
void scene_build_camera(App *app);
int scene_pick(App *app, float x, float y);

/* ui.c */
void ui_draw(App *app, SsGfx *g);
void ui_layout(App *app);
void ui_event(App *app, const UiEvent *ev);
void ui_draw_caption(App *app, SsGfx *g, SsRect r);

/* helpers used by both */
uint32_t ui_id(const char *label);
double clamp_zoom(App *app, double zoom);

#endif
