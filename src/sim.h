/* sim.h -- solar system model, ephemeris and sandbox physics.
 *
 * Distances are AU, time days (TDB), angles degrees.
 * Planet positions use the JPL "approximate positions of the major planets"
 * Keplerian elements (Standish), valid 1800-2050, with Pluto appended.
 * Moons ride analytic circular rails around their parent; user bodies can be
 * promoted to full N-body integration at any time.
 */
#ifndef SSS_SIM_H
#define SSS_SIM_H

#include "core.h"

#define MAX_BODIES 192
#define TRAIL_MAX 640
#define KM_PER_AU 149597870.7
#define SEC_PER_DAY 86400.0
#define G_AU_D2_PER_KG 1.488164e-34 /* AU^3 / day^2 / kg */
#define AU_KM (1.0 / KM_PER_AU)

typedef enum {
    BT_STAR = 0,
    BT_TERRESTRIAL,
    BT_GIANT,
    BT_ICE,
    BT_DWARF,
    BT_MOON,
    BT_ASTEROID,
    BT_COMET,
    BT_BLACKHOLE,
    BT_KIND_COUNT
} BodyKind;

typedef struct {
    double a, e, i, L, peri, node;       /* AU, -, deg, deg, deg, deg */
    double da, de, di, dL, dperi, dnode; /* per Julian century */
} Elements;

typedef struct {
    int id;
    char name[26];
    char label[14];
    BodyKind kind;
    double mass_kg;
    double radius_km;
    double day_hours;    /* rotation period, 0 = unknown */
    double axial_tilt;   /* degrees */
    int moon_count;
    const char *fact;
    SsColor col1, col2;    /* lit colour, shadow colour */
    SsColor accent;        /* UI dot / glow */
    int ringed;
    double ring_in, ring_out; /* in planet radii */
    SsColor ring_col;
    int atmosphere;
    int tex_seed;
    int has_elements;

    /* orbital state */
    int parent;          /* index of parent body, -1 = star */
    int on_rail;
    Elements el;
    double rail_a_km, rail_period_d, rail_phase, rail_incl, rail_node;
    Vec3 pos, vel;       /* used when dynamic */
    int dynamic;
    int pinned;
    int alive;
    int is_sun;

    /* history trail (dynamic bodies) */
    int trail_head, trail_count;
    Vec3 trail[TRAIL_MAX];

    /* resolved world state for the current frame */
    Vec3 wpos, wvel;
} Body;

typedef struct {
    int type; /* 1 = collision, 2 = spawn, 3 = message */
    Vec3 pos; /* AU, for the impact flash */
    int a, b; /* body ids */
    char text[80];
} SimEvent;

typedef struct {
    Body bodies[MAX_BODIES];
    int count;
    int next_id;
    double t;        /* days since J2000 */
    double speed;    /* simulated days per real second */
    int paused;
    int nbody;       /* 0 = ephemeris rails, 1 = gravity sandbox */
    int collisions;
    int preset;
    double elapsed_real; /* seconds of wall clock the sim has run */
    SimEvent events[12];
    int event_count;
} Sim;

enum {
    PRESET_FULL = 0,
    PRESET_INNER,
    PRESET_JOVIAN,
    PRESET_OUTER,
    PRESET_BINARY,
    PRESET_EMPTY,
    PRESET_COUNT
};

void sim_init(Sim *s);
void sim_load_preset(Sim *s, int preset, double start_days);
const char *sim_preset_name(int preset);

/* Set time keeping rails in sync (does not integrate). */
void sim_set_time(Sim *s, double days);
/* Advance by real seconds. Handles substepping, collisions, trails. */
void sim_advance(Sim *s, double real_seconds);
/* Recompute wpos/wvel for every body at the current sim time. */
void sim_resolve(Sim *s);

int sim_find(const Sim *s, int id);
int sim_add_body(Sim *s, const Body *proto, Vec3 pos, Vec3 vel, int parent);
void sim_remove_body(Sim *s, int index);
void sim_clear_trail(Sim *s, int index);

/* Orbital helpers */
void kepler_position(const Elements *el, double days, Vec3 *out);
void kepler_velocity(const Elements *el, double days, Vec3 *out);
double body_orbit_period(const Body *b, const Sim *s);
double body_distance_to_parent(const Body *b, const Sim *s);
void sim_orbit_points(const Sim *s, int index, Vec3 *out, int count);
void sim_rail_velocity(const Body *b, const Sim *s, Vec3 *out);
void sim_set_dynamic(Sim *s, int index, int dynamic);
void sim_circular_velocity(const Sim *s, Vec3 pos, double central_mass_kg, Vec3 *out);

/* Serialization (compact text format, versioned). */
void sim_save(const Sim *s, Str *out);
int sim_load(Sim *s, const char *text);

/* Names for custom bodies. */
const char *sim_kind_name(BodyKind k);
const char *sim_kind_plural(BodyKind k);

/* Constant used by the renderer so hit-testing and drawing agree. */
double body_visual_radius_px(const Body *b, double zoom, double dpi_scale, int true_scale,
                             double nearest_px);

#endif /* SSS_SIM_H */
