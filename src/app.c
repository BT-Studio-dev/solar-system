/* app.c -- application state machine shared by Win32 and the test harness. */
#include "app.h"
#include "theme.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

/* ------------------------------------------------------------------ camera */

void scene_build_camera(App *app) {
    UI *ui = &app->ui;
    float yaw = ui->yaw, pitch = ui->pitch;
    if (pitch < 0.05f) pitch = 0.05f;
    if (pitch > 1.5533f) pitch = 1.5533f;
    ui->pitch = pitch;
    ui->cam_right = (Vec3){cosf(yaw), -sinf(yaw), 0};
    ui->cam_up = (Vec3){sinf(yaw) * sinf(pitch), cosf(yaw) * sinf(pitch), cosf(pitch)};
    ui->cam_fwd = (Vec3){sinf(yaw) * cosf(pitch), cosf(yaw) * cosf(pitch), -sinf(pitch)};
}

void scene_project(App *app, Vec3 p, float *sx, float *sy, float *depth) {
    UI *ui = &app->ui;
    double dx = p.x - ui->center.x, dy = p.y - ui->center.y, dz = p.z - ui->center.z;
    double u = cos(ui->yaw) * dx - sin(ui->yaw) * dy;
    double v = sin(ui->yaw) * dx + cos(ui->yaw) * dy;
    double zoom = ui->zoom;
    if (sx) *sx = (float)(app->ui.viewport.x + app->ui.viewport.w * 0.5 + u * zoom);
    if (sy) *sy = (float)(app->ui.viewport.y + app->ui.viewport.h * 0.5 -
                          (v * sin(ui->pitch) + dz * cos(ui->pitch)) * zoom);
    if (depth) *depth = v * cos(ui->pitch) - dz * sin(ui->pitch);
}

void scene_project_dir(App *app, Vec3 p, float *sx, float *sy) {
    UI *ui = &app->ui;
    double u = cos(ui->yaw) * p.x - sin(ui->yaw) * p.y;
    double v = sin(ui->yaw) * p.x + cos(ui->yaw) * p.y;
    if (sx) *sx = (float)(u * ui->zoom);
    if (sy) *sy = (float)(-(v * sin(ui->pitch) + p.z * cos(ui->pitch)) * ui->zoom);
}

/* --------------------------------------------------------------------- ids */

uint32_t ui_id(const char *label) {
    uint32_t h = 2166136261u;
    while (*label) {
        h ^= (unsigned char)*label++;
        h *= 16777619u;
    }
    return h ? h : 1u;
}

double clamp_zoom(App *app, double zoom) {
    (void)app;
    if (zoom < 1.2) zoom = 1.2;
    if (zoom > 60000.0) zoom = 60000.0;
    return zoom;
}

void app_reset_view(App *app) {
    UI *ui = &app->ui;
    ui->center = (Vec3){0, 0, 0};
    ui->zoom = 26.0 * app->dpi;
    ui->yaw = -0.55f;
    ui->pitch = 1.02f;
    ui->follow_id = 0;
    scene_build_camera(app);
}

void app_select(App *app, int body_id) {
    app->ui.selected_id = body_id;
    app->ui.tab = 0;
}

void app_focus_selected(App *app) {
    int idx = sim_find(&app->sim, app->ui.selected_id);
    if (idx < 0) return;
    Body *b = &app->sim.bodies[idx];
    app->ui.follow_id = b->id;
    app->ui.center = b->wpos;
    double r = body_visual_radius_px(b, app->ui.zoom, app->dpi, 0, 0);
    if (b->parent < 0) {
        /* frame the local system: bring the camera close enough for moons */
        if (app->ui.zoom < 2200.0 * app->dpi) app->ui.zoom = 2400.0 * app->dpi;
    } else {
        app->ui.zoom += r * 12.0;
    }
    app->ui.zoom = clamp_zoom(app, app->ui.zoom);
}

/* Default pair for the object-separation readout: the star plus the first
 * world that circles it. */
static void default_separation_pair(App *app) {
    app->ui.sep_a_id = 0;
    app->ui.sep_b_id = 0;
    for (int i = 0; i < app->sim.count; i++) {
        const Body *b = &app->sim.bodies[i];
        if (!b->alive) continue;
        if (!app->ui.sep_a_id && b->is_sun) app->ui.sep_a_id = b->id;
    }
    for (int i = 0; i < app->sim.count; i++) {
        const Body *b = &app->sim.bodies[i];
        if (!b->alive || b->is_sun) continue;
        app->ui.sep_b_id = b->id;
        if (strcmp(b->name, "Earth") == 0) break;
    }
}

void app_apply_preset(App *app, int preset) {
    double t = app->sim.t;
    sim_load_preset(&app->sim, preset, t);
    if (preset == PRESET_BINARY) app->sim.speed = 30.0;
    else app->sim.speed = preset == PRESET_EMPTY ? 1.0 : 1.0;
    app->sim.paused = 0;
    app->ui.selected_id = 0;
    app->ui.follow_id = 0;
    app->ui.tab = 0;
    app->ui.preset_open = 0;
    app_reset_view(app);
    if (preset == PRESET_FULL) app->ui.zoom = 46.0 * app->dpi;
    if (preset == PRESET_OUTER) app->ui.zoom = 15.0 * app->dpi;
    if (preset == PRESET_EMPTY) app->ui.zoom = 260.0 * app->dpi;
    default_separation_pair(app);
    app_toast(app, sim_preset_name(preset), 0);
}

void app_set_time_now(App *app) {
    app->sim.t = wall_days_since_j2000();
    sim_resolve(&app->sim);
    app_sync_date_fields(app);
    app_toast(app, "Jumped to the present", 0);
}

void app_sync_date_fields(App *app) {
    int hh, mm, ss;
    jd_to_date(app->sim.t, &app->ui.date_year, &app->ui.date_month, &app->ui.date_day,
               &hh, &mm, &ss);
}

static int days_in_month(int year, int month) {
    static const int DIM[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int d = DIM[(month - 1) % 12];
    if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) d = 29;
    return d;
}

void app_set_start_date(App *app, int year, int month, int day) {
    UI *ui = &app->ui;
    if (year < 1800) year = 1800;
    if (year > 2100) year = 2100;
    if (month < 1) month = 1;
    if (month > 12) month = 12;
    if (day < 1) day = 1;
    int dim = days_in_month(year, month);
    if (day > dim) day = dim;
    ui->date_year = year;
    ui->date_month = month;
    ui->date_day = day;
    app->sim.t = date_to_jd(year, month, day, 0, 0, 0);
    sim_resolve(&app->sim);
    char buf[96];
    snprintf(buf, sizeof(buf), "Start date: %04d-%02d-%02d 00:00 UTC", year, month, day);
    app_toast(app, buf, 0);
}

void app_set_track(App *app, int body_id) {
    UI *ui = &app->ui;
    if (!body_id) {
        ui->follow_id = 0;
        app_toast(app, "Auto track off: free camera", 0);
        return;
    }
    int idx = sim_find(&app->sim, body_id);
    if (idx < 0) return;
    ui->follow_id = body_id;
    char buf[96];
    snprintf(buf, sizeof(buf), "Auto track: %s", app->sim.bodies[idx].name);
    app_toast(app, buf, 0);
}

double app_separation_au(const App *app, int a_id, int b_id) {
    if (!a_id || !b_id) return -1.0;
    int ia = sim_find(&app->sim, a_id);
    int ib = sim_find(&app->sim, b_id);
    if (ia < 0 || ib < 0) return -1.0;
    Vec3 a = app->sim.bodies[ia].wpos, b = app->sim.bodies[ib].wpos;
    double dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

void app_set_theme(App *app, int index) {
    if (index < 0) index = 0;
    if (index >= sss_theme_count()) index = 0;
    sss_theme_set(index);
    app->theme_index = index;
    app->ui.theme_open = 0;
    app->ui.preset_open = 0;
    char buf[96];
    snprintf(buf, sizeof(buf), "Theme: %s", sss_theme()->name);
    app_toast(app, buf, 0);
    if (app->on_theme) app->on_theme(app, index);
}

void app_next_theme(App *app) {
    app_set_theme(app, (app->theme_index + 1) % sss_theme_count());
}

void app_toast(App *app, const char *text, int kind) {
    strncpy(app->ui.toast, text, sizeof(app->ui.toast) - 1);
    app->ui.toast[sizeof(app->ui.toast) - 1] = 0;
    app->ui.toast_until = app->now + 2.6;
    app->ui.toast_kind = kind;
    if (app->on_toast && kind == 2) app->on_toast(app, text, kind);
}

void app_remove_selected(App *app) {
    int idx = sim_find(&app->sim, app->ui.selected_id);
    if (idx < 0) return;
    if (app->sim.bodies[idx].is_sun) {
        app_toast(app, "The star cannot be deleted in this sandbox", 1);
        return;
    }
    char name[32];
    strncpy(name, app->sim.bodies[idx].name, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    sim_remove_body(&app->sim, idx);
    app->ui.selected_id = 0;
    app->ui.follow_id = 0;
    char msg[96];
    snprintf(msg, sizeof(msg), "Removed %s", name);
    app_toast(app, msg, 0);
}

/* ---------------------------------------------------------- content folders */

static int add_kind_index(BodyKind kind) {
    switch (kind) {
        case BT_STAR: return 0;
        case BT_GIANT: return 2;
        case BT_ICE: return 3;
        case BT_DWARF: return 4;
        case BT_ASTEROID: return 5;
        case BT_COMET: return 6;
        case BT_BLACKHOLE: return 7;
        case BT_MOON:
        case BT_TERRESTRIAL:
        default: return 1;
    }
}

void app_rescan_content(App *app) {
    content_scan();
    if (app->pack_index >= content_pack_count()) app->pack_index = -1;
    char msg[96];
    snprintf(msg, sizeof(msg), "Content: %d parts, %d texture packs, %d mods",
             content_part_count(), content_pack_count(), content_mod_count());
    app_toast(app, msg, 0);
    content_log("scanned content: %d parts, %d packs, %d mods", content_part_count(),
                content_pack_count(), content_mod_count());
}

void app_use_part(App *app, int index) {
    const ContentPart *part = content_part(index);
    if (!part) return;
    UI *ui = &app->ui;
    int kind = content_kind_from_name(part->kind);
    if (kind < 0) kind = BT_TERRESTRIAL;
    ui->add_kind = add_kind_index((BodyKind)kind);
    if (part->mass_kg > 0) ui->add_mass_log = (float)log10(part->mass_kg);
    if (part->radius_km > 0) ui->add_radius_log = (float)log10(part->radius_km);
    if (part->orbit_au > 0) ui->add_orbit_log = (float)log10(part->orbit_au);
    ui->add_ecc = (float)part->ecc;
    ui->part_colors = 1;
    snprintf(ui->part_name, sizeof(ui->part_name), "%s", part->name);
    ui->part_col1 = part->col1;
    ui->part_col2 = part->col2;
    ui->part_accent = part->accent;
    ui->part_ringed = part->ringed;
    ui->part_ring_in = part->ring_in > 0 ? part->ring_in : 1.24;
    ui->part_ring_out = part->ring_out > 0 ? part->ring_out : 2.27;
    ui->part_ring_col = part->ring_col;
    ui->tab = 1;
    char msg[96];
    snprintf(msg, sizeof(msg), "%s loaded into the composer", part->name);
    app_toast(app, msg, 0);
    content_log("part '%s' loaded from %s", part->name, part->file);
}

void app_apply_pack(App *app, int index) {
    if (index < 0 || index >= content_pack_count()) {
        app->pack_index = -1;
        app_toast(app, "Texture pack: theme colours", 0);
        content_log("texture pack cleared");
        return;
    }
    const ContentPack *pack = content_pack(index);
    app->pack_index = index;
    char msg[96];
    snprintf(msg, sizeof(msg), "Texture pack: %s", pack->name);
    app_toast(app, msg, 0);
    content_log("texture pack '%s' (%d%%) from %s", pack->name, pack->strength, pack->file);
}

SsColor app_body_tint(const App *app, SsColor base, int kind) {
    if (!app || app->pack_index < 0) return base;
    const ContentPack *pack = content_pack(app->pack_index);
    if (!pack) return base;
    if (kind == BT_BLACKHOLE) return base;
    float t = (float)pack->strength / 100.0f;
    if (kind == BT_STAR) t *= 0.55f;
    return col_blend(base, pack->tint, t);
}

void app_open_mod(App *app, int index) {
    const ContentMod *mod = content_mod(index);
    if (!mod) return;
    if (!app->read_text) {
        app_toast(app, "Cannot read mod files on this platform", 1);
        return;
    }
    int len = 0;
    char *text = app->read_text(mod->file, &len);
    if (!text) {
        app_toast(app, "Could not read that mod", 1);
        content_log("failed to read mod %s", mod->file);
        return;
    }
    app_load_text(app, text);
    sss_free(text);
    content_log("mod '%s' loaded from %s", mod->name, mod->file);
}

static const char *KIND_DEFAULT_NAMES[] = {
    "Nova", "Terra Nova", "Giant", "Ice World", "Dwarf", "Luna", "Asteroid", "Comet", "Singularity"
};
static const BodyKind ADD_KINDS[] = {BT_STAR, BT_TERRESTRIAL, BT_GIANT, BT_ICE, BT_DWARF, BT_ASTEROID, BT_COMET, BT_BLACKHOLE};

void app_add_custom(App *app) {
    Sim *s = &app->sim;
    int kind_idx = CLAMP(app->ui.add_kind, 0, (int)ARRAY_LEN(ADD_KINDS) - 1);
    BodyKind kind = ADD_KINDS[kind_idx];

    Body b;
    memset(&b, 0, sizeof(b));
    double mass = pow(10.0, app->ui.add_mass_log);
    double radius = pow(10.0, app->ui.add_radius_log);
    if (kind == BT_STAR) {
        if (mass < 1e29) mass = 1e29;
        if (radius < 3e4) radius = 3e4;
    }
    if (kind == BT_BLACKHOLE) {
        if (mass < 1e29) mass = 1e29;
        radius = 12.0;
    }
    snprintf(b.name, sizeof(b.name), "%s %d", KIND_DEFAULT_NAMES[kind_idx], ++app->ui.add_count);
    strncpy(b.label, b.name, sizeof(b.label) - 1);
    b.kind = kind;
    b.mass_kg = mass;
    b.radius_km = radius;
    b.day_hours = 12.0 + (app->ui.add_count % 7) * 6.0;
    b.tex_seed = 100 + app->ui.add_count;
    b.atmosphere = (kind == BT_TERRESTRIAL || kind == BT_GIANT || kind == BT_ICE) ? 1 : 0;
    b.ringed = 0;
    b.fact = "A body you placed in the sandbox. Its orbit is integrated live.";
    switch (kind) {
        case BT_STAR:
            b.col1 = RGB_A(255, 230, 176); b.col2 = RGB_A(255, 140, 54); b.accent = RGB_A(255, 214, 140);
            b.is_sun = 0;
            break;
        case BT_GIANT:
            b.col1 = RGB_A(226, 196, 152); b.col2 = RGB_A(116, 84, 56); b.accent = RGB_A(226, 190, 146);
            break;
        case BT_ICE:
            b.col1 = RGB_A(140, 214, 232); b.col2 = RGB_A(44, 96, 132); b.accent = RGB_A(140, 214, 232);
            break;
        case BT_DWARF:
            b.col1 = RGB_A(208, 196, 184); b.col2 = RGB_A(96, 88, 80); b.accent = RGB_A(208, 196, 184);
            break;
        case BT_ASTEROID:
            b.col1 = RGB_A(184, 172, 154); b.col2 = RGB_A(80, 72, 62); b.accent = RGB_A(184, 172, 154);
            break;
        case BT_COMET:
            b.col1 = RGB_A(206, 228, 240); b.col2 = RGB_A(86, 108, 128); b.accent = RGB_A(190, 226, 255);
            break;
        case BT_BLACKHOLE:
            b.col1 = RGB_A(28, 22, 40); b.col2 = RGB_A(4, 4, 8); b.accent = RGB_A(186, 140, 255);
            break;
        default:
            b.col1 = RGB_A(110, 176, 240); b.col2 = RGB_A(26, 60, 110); b.accent = RGB_A(110, 176, 240);
            break;
    }

    /* A Library part can override the palette, the rings and the name. */
    if (app->ui.part_colors) {
        b.col1 = app->ui.part_col1;
        b.col2 = app->ui.part_col2;
        b.accent = app->ui.part_accent;
        b.ringed = app->ui.part_ringed;
        b.ring_in = app->ui.part_ring_in;
        b.ring_out = app->ui.part_ring_out;
        b.ring_col = app->ui.part_ring_col;
        if (app->ui.part_name[0]) {
            size_t n = 0;
            while (app->ui.part_name[n] && n < sizeof(b.name) - 6) {
                b.name[n] = app->ui.part_name[n];
                n++;
            }
            char suffix[16];
            snprintf(suffix, sizeof(suffix), " %d", app->ui.add_count);
            for (int i = 0; suffix[i] && n < sizeof(b.name) - 1; i++) b.name[n++] = suffix[i];
            b.name[n] = 0;
        }
        strncpy(b.label, b.name, sizeof(b.label) - 1);
        content_log("added '%s' from a Library part (%.3g kg, %.0f km)", b.name, b.mass_kg,
                    b.radius_km);
    }

    double a_au = pow(10.0, app->ui.add_orbit_log);
    double phase = fmod((double)app->ui.add_count * 2.399963 + 0.7, TAU);
    Vec3 pos = {cos(phase) * a_au, sin(phase) * a_au, (app->ui.add_count % 3 - 1) * 0.05};
    Vec3 vel = {0, 0, 0};

    int central = -1;
    double best = 0;
    for (int i = 0; i < s->count; i++) {
        if (!s->bodies[i].alive) continue;
        if (s->bodies[i].kind == BT_STAR && s->bodies[i].mass_kg > best) {
            best = s->bodies[i].mass_kg;
            central = i;
        }
    }
    if (central >= 0) {
        Vec3 ref = s->bodies[central].wpos;
        Vec3 rel = {pos.x - ref.x, pos.y - ref.y, pos.z - ref.z};
        Vec3 cv = {0, 0, 0};
        sim_circular_velocity(s, rel, s->bodies[central].mass_kg, &cv);
        vel.x = cv.x + s->bodies[central].wvel.x;
        vel.y = cv.y + s->bodies[central].wvel.y;
        vel.z = cv.z + s->bodies[central].wvel.z;
        pos.x += ref.x;
        pos.y += ref.y;
        pos.z += ref.z;
    }
    int idx = sim_add_body(s, &b, pos, vel, -1);
    if (idx < 0) {
        app_toast(app, "Body limit reached (192)", 1);
        return;
    }
    s->nbody = 1;
    app->ui.selected_id = s->bodies[idx].id;
    app->ui.tab = 0;
    char msg[96];
    snprintf(msg, sizeof(msg), "%s placed at %.2f AU", b.name, a_au);
    app_toast(app, msg, 0);
}

/* ------------------------------------------------------------------- init */

void app_init(App *app, int width, int height) {
    memset(app, 0, sizeof(*app));
    app->width = width;
    app->height = height;
    app->dpi = 1.0f;
    app->now = 0;
    app->running = 1;
    app->show_fps = 1;
    app->theme_index = sss_theme_index();
    app->pack_index = -1;
    sim_init(&app->sim);
    double t = wall_days_since_j2000();
    sim_load_preset(&app->sim, PRESET_FULL, t);
    app->sim.paused = 0;
    app->sim.speed = 1.0;

    UI *ui = &app->ui;
    ui->yaw = -0.55f;
    ui->pitch = 1.02f;
    ui->zoom = 46.0;
    ui->show_orbits = 1;
    ui->show_labels = 1;
    ui->show_trails = 1;
    ui->show_moons = 1;
    ui->show_sun = 1;
    ui->show_fade_orbits = 0;
    ui->show_moon_names = 0;
    ui->show_axes = 0;
    ui->simplified = 0;
    ui->panel_scroll = 0;
    ui->tab = 0;
    ui->add_kind = 1;
    ui->add_mass_log = 24.6f;   /* Earth-ish */
    ui->add_radius_log = 3.8f;  /* 6,300 km */
    ui->add_orbit_log = 0.18f;  /* 1.5 AU */
    ui->add_ecc = 0.0f;
    ui->part_colors = 0;
    ui->selected_id = 0;
    app_reset_view(app);
    ui->zoom = 46.0;

    /* preselect Earth so the inspector has content on first launch */
    int e = -1;
    for (int i = 0; i < app->sim.count; i++) {
        if (app->sim.bodies[i].alive && strcmp(app->sim.bodies[i].name, "Earth") == 0) {
            e = i;
            break;
        }
    }
    if (e >= 0) ui->selected_id = app->sim.bodies[e].id;
    app_sync_date_fields(app);
    default_separation_pair(app);
    scene_build_camera(app);
}

void app_shutdown(App *app) {
    if (app->scene_scratch) {
        sss_free(app->scene_scratch);
        app->scene_scratch = NULL;
    }
}

void app_resize(App *app, int width, int height) {
    if (width < 720) width = 720;
    if (height < 480) height = 480;
    app->width = width;
    app->height = height;
    app->ui.viewport = rect_make(0, TITLEBAR_H + TRANSPORT_H,
                                 (float)width - PANEL_W,
                                 (float)height - TITLEBAR_H - TRANSPORT_H - STATUS_H);
    app->ui.inspector = rect_make((float)width - PANEL_W, TITLEBAR_H + TRANSPORT_H,
                                  PANEL_W, (float)height - TITLEBAR_H - TRANSPORT_H - STATUS_H);
    app->ui.titlebar = rect_make(0, 0, (float)width, TITLEBAR_H);
    app->ui.transport = rect_make(0, TITLEBAR_H, (float)width, TRANSPORT_H);
    app->ui.status = rect_make(0, (float)height - STATUS_H, (float)width, STATUS_H);
}

/* ------------------------------------------------------------------ ticks */

static void smooth_follow(App *app, double dt) {
    UI *ui = &app->ui;
    if (!ui->follow_id) return;
    int idx = sim_find(&app->sim, ui->follow_id);
    if (idx < 0) {
        ui->follow_id = 0;
        return;
    }
    Vec3 target = app->sim.bodies[idx].wpos;
    double k = 1.0 - exp(-9.0 * dt);
    ui->center.x += (target.x - ui->center.x) * k;
    ui->center.y += (target.y - ui->center.y) * k;
    ui->center.z += (target.z - ui->center.z) * k;
}

void app_tick(App *app, double dt) {
    if (dt < 0) dt = 0;
    if (dt > 0.12) dt = 0.12;
    app->now += dt;
    if (!app->sim.paused) sim_advance(&app->sim, dt);
    smooth_follow(app, dt);
    app->ui.pulse += dt;
    if (app->ui.hover_id) app->ui.hover_anim += (1.0f - app->ui.hover_anim) * (float)(dt * 14.0);
    else app->ui.hover_anim += (0.0f - app->ui.hover_anim) * (float)(dt * 14.0);
}

/* ---------------------------------------------------------------- commands */

int app_save_text(App *app, char *buf, int cap) {
    Str s;
    str_init(&s, buf, cap);
    sim_save(&app->sim, &s);
    /* The scene's colour theme travels with the simulation. */
    str_add(&s, "theme ");
    str_add(&s, sss_theme()->id);
    str_addc(&s, '\n');
    return s.len;
}

void app_load_text(App *app, const char *text) {
    if (sim_load(&app->sim, text)) {
        const char *line = text;
        while ((line = strstr(line, "theme ")) != NULL) {
            if (line == text || line[-1] == '\n') {
                char id[40];
                int n = 0;
                const char *v = line + 6;
                while (v[n] && v[n] != '\n' && v[n] != '\r' && n < (int)sizeof(id) - 1) {
                    id[n] = v[n];
                    n++;
                }
                id[n] = 0;
                int index = sss_theme_find(id);
                if (index >= 0) app_set_theme(app, index);
                break;
            }
            line += 6;
        }
        app->ui.selected_id = 0;
        app->ui.follow_id = 0;
        app_reset_view(app);
        app_sync_date_fields(app);
        app_toast(app, "Simulation loaded", 0);
    } else {
        app_toast(app, "That file is not a valid simulation", 1);
    }
}
