/* sim.c -- solar system model, ephemeris and sandbox physics. */
#include "sim.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define SIM_COLLIDE_MUL 300.0

/* ------------------------------------------------------------------ data */

#define EL(a, e, i, L, peri, node, da, de, di, dL, dperi, dnode) \
    { a, e, i, L, peri, node, da, de, di, dL, dperi, dnode }

typedef struct {
    const char *name;
    const char *label;
    const char *fact;
    BodyKind kind;
    double mass_kg, radius_km, day_hours, tilt;
    int moons;
    SsColor c1, c2, accent;
    int ringed;
    double ring_in, ring_out;
    SsColor ring_col;
    int atmo;
    int tex;
    Elements el;
} PlanetData;

/* JPL approximate elements, epoch J2000, rates per Julian century. */
static const PlanetData PLANETS[] = {
    { "Mercury", "MERCURY", "Smallest planet; a day outlasts its year.",
      BT_TERRESTRIAL, 3.3011e23, 2439.7, 1407.6, 0.03, 0,
      RGB_A(196, 188, 178), RGB_A(96, 88, 80), RGB_A(176, 168, 156),
      0, 0, 0, 0, 0, 11,
      EL(0.38709927, 0.20563593, 7.00497902, 252.25032350, 77.45779628, 48.33076593,
         0.00000037, 0.00001906, -0.00594749, 149472.67411175, 0.16047689, -0.12534081) },
    { "Venus", "VENUS", "Runaway greenhouse; 465 °C under sulphuric clouds.",
      BT_TERRESTRIAL, 4.8675e24, 6051.8, 5832.5, 177.4, 0,
      RGB_A(240, 214, 166), RGB_A(146, 112, 68), RGB_A(236, 200, 150),
      0, 0, 0, 0, 1, 12,
      EL(0.72333566, 0.00677672, 3.39467605, 181.97909950, 131.60246718, 76.67984255,
         0.00000390, -0.00004107, -0.00078890, 58517.81538729, 0.00268329, -0.27769418) },
    { "Earth", "EARTH", "The only world known to carry liquid-water oceans.",
      BT_TERRESTRIAL, 5.97237e24, 6371.0, 23.934, 23.44, 1,
      RGB_A(112, 180, 244), RGB_A(24, 62, 112), RGB_A(96, 176, 255),
      0, 0, 0, 0, 1, 13,
      EL(1.00000261, 0.01671123, -0.00001531, 100.46457166, 102.93768193, 0.0,
         0.00000562, -0.00004392, -0.01294668, 35999.37244981, 0.32327364, 0.0) },
    { "Mars", "MARS", "Olympus Mons rises 22 km above the plains.",
      BT_TERRESTRIAL, 6.4171e23, 3389.5, 24.623, 25.19, 2,
      RGB_A(216, 122, 78), RGB_A(104, 50, 32), RGB_A(228, 138, 92),
      0, 0, 0, 0, 1, 14,
      EL(1.52371034, 0.09339410, 1.84969142, -4.55343205, -23.94362959, 49.55953891,
         0.00001847, 0.00007882, -0.00813131, 19140.30268499, 0.44441088, -0.29257343) },
    { "Jupiter", "JUPITER", "More massive than every other planet combined.",
      BT_GIANT, 1.8982e27, 69911.0, 9.925, 3.13, 4,
      RGB_A(232, 206, 172), RGB_A(120, 88, 62), RGB_A(226, 190, 146),
      1, 1.38, 2.28, RGBA(226, 208, 178, 170), 1, 15,
      EL(5.20288700, 0.04838624, 1.30439695, 34.39644051, 14.72847983, 100.47390909,
         -0.00011607, -0.00013253, -0.00183714, 3034.74612775, 0.21252668, 0.20469106) },
    { "Saturn", "SATURN", "Least dense planet; it would float on water.",
      BT_GIANT, 5.6834e26, 58232.0, 10.656, 26.73, 5,
      RGB_A(232, 216, 176), RGB_A(132, 112, 78), RGB_A(236, 220, 178),
      1, 1.24, 2.40, RGBA(240, 226, 192, 190), 1, 16,
      EL(9.53667594, 0.05386179, 2.48599187, 49.95424423, 92.59887831, 113.66242448,
         -0.00125060, -0.00050991, 0.00193609, 1222.49362201, -0.41897216, -0.28867794) },
    { "Uranus", "URANUS", "Rolls around the Sun tipped 98 degrees on its side.",
      BT_ICE, 8.6810e25, 25362.0, 17.24, 97.77, 5,
      RGB_A(168, 226, 226), RGB_A(66, 128, 140), RGB_A(160, 226, 226),
      1, 1.60, 2.00, RGBA(170, 220, 224, 110), 1, 17,
      EL(19.18916464, 0.04725744, 0.77263783, 313.23810451, 170.95427630, 74.01692503,
         -0.00196176, -0.00004397, -0.00242939, 428.48202785, 0.40805281, 0.04240589) },
    { "Neptune", "NEPTUNE", "Supersonic winds reach 2,100 km/h.",
      BT_ICE, 1.02413e26, 24622.0, 16.11, 28.32, 1,
      RGB_A(76, 118, 224), RGB_A(22, 44, 112), RGB_A(86, 128, 240),
      0, 0, 0, 0, 1, 18,
      EL(30.06992276, 0.00859048, 1.77004347, -55.12002969, 44.96476227, 131.78422574,
         0.00026291, 0.00005105, 0.00035372, 218.45945325, -0.32241464, -0.00508664) },
    { "Pluto", "PLUTO", "Dwarf planet with a heart-shaped nitrogen glacier.",
      BT_DWARF, 1.303e22, 1188.3, 153.3, 122.5, 1,
      RGB_A(214, 196, 176), RGB_A(112, 96, 84), RGB_A(206, 188, 168),
      0, 0, 0, 0, 0, 19,
      EL(39.48211675, 0.24882730, 17.14001206, 238.92903833, 224.06891629, 110.30393684,
         -0.00031596, 0.00005170, 0.00004818, 145.20780515, -0.04062942, -0.01183482) },
};

typedef struct {
    const char *name;
    const char *parent;
    const char *fact;
    double a_km, period_d, radius_km, mass_kg, incl;
    int tex;
    SsColor c1, c2;
} MoonData;

static const MoonData MOONS[] = {
    { "Moon", "Earth", "Tidally locked; drifting away 3.8 cm per year.",
      384400, 27.3217, 1737.4, 7.342e22, 5.1, 31, RGB_A(206, 204, 200), RGB_A(88, 86, 84) },
    { "Phobos", "Mars", "Orbits faster than Mars rotates; will crash in ~50 Myr.",
      9376, 0.31891, 11.3, 1.0659e16, 1.1, 32, RGB_A(158, 146, 132), RGB_A(72, 64, 56) },
    { "Deimos", "Mars", "Tiny, dusty and slowly spiralling outward.",
      23463, 1.26244, 6.2, 1.4762e15, 1.8, 33, RGB_A(168, 156, 142), RGB_A(78, 70, 62) },
    { "Io", "Jupiter", "Most volcanically active body in the solar system.",
      421700, 1.769138, 1821.6, 8.93e22, 0.04, 34, RGB_A(232, 210, 120), RGB_A(148, 112, 40) },
    { "Europa", "Jupiter", "An ocean hides beneath its cracked ice shell.",
      671100, 3.551181, 1560.8, 4.8e22, 0.47, 35, RGB_A(226, 220, 206), RGB_A(150, 140, 128) },
    { "Ganymede", "Jupiter", "Largest moon in the solar system.",
      1070400, 7.154553, 2634.1, 1.4819e23, 0.20, 36, RGB_A(186, 176, 160), RGB_A(96, 88, 78) },
    { "Callisto", "Jupiter", "The most heavily cratered object known.",
      1882700, 16.689, 2410.3, 1.0759e23, 0.28, 37, RGB_A(150, 142, 132), RGB_A(74, 68, 62) },
    { "Mimas", "Saturn", "The 'Death Star' moon, split by Herschel crater.",
      185540, 0.942, 198.2, 3.75e19, 1.6, 38, RGB_A(206, 204, 200), RGB_A(108, 104, 100) },
    { "Enceladus", "Saturn", "Geysers of water ice feed Saturn's E ring.",
      238040, 1.370218, 252.1, 1.08e20, 0.01, 39, RGB_A(240, 242, 244), RGB_A(150, 160, 170) },
    { "Tethys", "Saturn", "Nearly pure water ice, scarred by Ithaca Chasma.",
      294670, 1.887802, 531.1, 6.17e20, 1.1, 40, RGB_A(214, 212, 206), RGB_A(112, 112, 108) },
    { "Dione", "Saturn", "Wispy ice cliffs cross its trailing hemisphere.",
      377420, 2.736915, 561.4, 1.095e21, 0.02, 41, RGB_A(200, 198, 192), RGB_A(104, 102, 98) },
    { "Rhea", "Saturn", "Second-largest moon of Saturn, mostly water ice.",
      527108, 4.518212, 763.8, 2.307e21, 0.35, 42, RGB_A(198, 196, 190), RGB_A(100, 98, 94) },
    { "Titan", "Saturn", "Thick orange nitrogen haze over methane lakes.",
      1221870, 15.945, 2574.7, 1.3452e23, 0.33, 43, RGB_A(226, 176, 96), RGB_A(140, 96, 40) },
    { "Iapetus", "Saturn", "One hemisphere is as black as asphalt.",
      3560820, 79.3215, 734.5, 1.806e21, 15.47, 44, RGB_A(172, 160, 142), RGB_A(64, 58, 50) },
    { "Miranda", "Uranus", "A patchwork world of ice canyons.",
      129390, 1.413479, 235.8, 6.59e19, 4.34, 45, RGB_A(198, 202, 206), RGB_A(104, 110, 118) },
    { "Ariel", "Uranus", "Brightest of Uranus' moons, cut by rift valleys.",
      190900, 2.520379, 578.9, 1.353e21, 0.26, 46, RGB_A(206, 210, 214), RGB_A(110, 116, 122) },
    { "Umbriel", "Uranus", "Darkest Uranian moon, capped by a bright ring.",
      266000, 4.144177, 584.7, 1.172e21, 0.13, 47, RGB_A(146, 150, 156), RGB_A(70, 74, 80) },
    { "Titania", "Uranus", "Largest moon of Uranus, etched with faults.",
      435910, 8.705872, 788.4, 3.527e21, 0.34, 48, RGB_A(186, 190, 194), RGB_A(92, 96, 102) },
    { "Oberon", "Uranus", "Ancient, cratered and stained dark red.",
      583520, 13.463, 761.4, 3.014e21, 0.06, 49, RGB_A(172, 170, 168), RGB_A(84, 82, 80) },
    { "Triton", "Neptune", "Orbits backwards; probably a captured Kuiper body.",
      354759, -5.876854, 1353.4, 2.139e22, 23.0, 50, RGB_A(216, 214, 210), RGB_A(116, 112, 108) },
    { "Charon", "Pluto", "Half Pluto's size; the pair orbit a common point.",
      19591, 6.3872, 606.0, 1.586e21, 0.08, 51, RGB_A(190, 186, 180), RGB_A(96, 92, 88) },
};

typedef struct {
    const char *name;
    const char *label;
    const char *fact;
    BodyKind kind;
    double mass_kg, radius_km, day_hours;
    SsColor c1, c2, accent;
    int tex;
    Elements el;
} SmallBody;

static const SmallBody SMALL[] = {
    { "Ceres", "CERES", "Largest object in the asteroid belt; a dwarf planet.",
      BT_DWARF, 9.3839e20, 473.0, 9.07,
      RGB_A(156, 148, 138), RGB_A(74, 68, 62), RGB_A(150, 142, 132), 61,
      EL(2.7675, 0.07582, 10.593, 249.9, 153.91, 80.31, 0, 0, 0, 0, 0, 0) },
    { "Vesta", "VESTA", "Second-most massive asteroid; a protoplanet survivor.",
      BT_ASTEROID, 2.589e20, 262.7, 5.34,
      RGB_A(176, 166, 152), RGB_A(86, 78, 70), RGB_A(166, 156, 142), 62,
      EL(2.3615, 0.08874, 7.140, 275.9, 255.0, 103.8, 0, 0, 0, 0, 0, 0) },
    { "Pallas", "PALLAS", "A battered protoplanet on a steeply tilted orbit.",
      BT_ASTEROID, 2.04e20, 256.0, 7.81,
      RGB_A(150, 152, 148), RGB_A(70, 72, 70), RGB_A(140, 142, 138), 63,
      EL(2.7727, 0.2313, 34.84, 201.3, 123.1, 173.1, 0, 0, 0, 0, 0, 0) },
    { "Haumea", "HAUMEA", "Egg-shaped dwarf planet with a two-hour day.",
      BT_DWARF, 4.006e21, 816.0, 3.92,
      RGB_A(212, 214, 220), RGB_A(104, 106, 114), RGB_A(200, 204, 212), 64,
      EL(43.13, 0.1913, 28.19, 219.4, 1.2, 122.2, 0, 0, 0, 0, 0, 0) },
    { "Makemake", "MAKEMAKE", "Bright, reddish and ring-free - so far.",
      BT_DWARF, 3.1e21, 715.0, 22.83,
      RGB_A(214, 188, 166), RGB_A(108, 88, 72), RGB_A(206, 180, 158), 65,
      EL(45.79, 0.159, 29.0, 168.4, 15.4, 79.4, 0, 0, 0, 0, 0, 0) },
    { "Eris", "ERIS", "More massive than Pluto; the discovery that redefined 'planet'.",
      BT_DWARF, 1.6466e22, 1163.0, 379.2,
      RGB_A(226, 224, 220), RGB_A(116, 114, 112), RGB_A(216, 214, 210), 66,
      EL(67.78, 0.44068, 44.04, 31.75, 187.55, 35.95, 0, 0, 0, 0, 0, 0) },
    { "Halley", "HALLEY", "Returns to the inner system roughly every 76 years.",
      BT_COMET, 2.2e14, 5.5, 52.8,
      RGB_A(196, 220, 236), RGB_A(80, 104, 128), RGB_A(180, 220, 255), 67,
      EL(17.834, 0.96714, 162.26, 236.2, 169.75, 58.42, 0, 0, 0, 0, 0, 0) },
    { "Encke", "ENCKE", "Shortest known orbital period of any comet.",
      BT_COMET, 5.0e13, 4.8, 11.0,
      RGB_A(206, 216, 220), RGB_A(90, 100, 108), RGB_A(190, 226, 240), 68,
      EL(2.2152, 0.8483, 11.78, 191.0, 161.12, 334.57, 0, 0, 0, 0, 0, 0) },
    { "67P", "67P/C-G", "Rosetta's comet: a duck-shaped pile of ice and dust.",
      BT_COMET, 1.0e13, 2.0, 12.4,
      RGB_A(186, 180, 170), RGB_A(76, 72, 66), RGB_A(200, 220, 230), 69,
      EL(3.4630, 0.6410, 7.04, 82.9, 62.9, 50.1, 0, 0, 0, 0, 0, 0) },
};

static const char *KIND_NAMES[BT_KIND_COUNT] = {
    "Star", "Terrestrial planet", "Gas giant", "Ice giant", "Dwarf planet",
    "Moon", "Asteroid", "Comet", "Black hole"
};
static const char *KIND_PLURALS[BT_KIND_COUNT] = {
    "Stars", "Terrestrial planets", "Gas giants", "Ice giants", "Dwarf planets",
    "Moons", "Asteroids", "Comets", "Black holes"
};

const char *sim_kind_name(BodyKind k) {
    return (k >= 0 && k < BT_KIND_COUNT) ? KIND_NAMES[k] : "Object";
}
const char *sim_kind_plural(BodyKind k) {
    return (k >= 0 && k < BT_KIND_COUNT) ? KIND_PLURALS[k] : "Objects";
}

static const char *PRESET_NAMES[PRESET_COUNT] = {
    "Solar System", "Inner System", "Jupiter & Saturn", "Outer System",
    "Binary Star", "Empty Sandbox"
};
const char *sim_preset_name(int preset) {
    return PRESET_NAMES[preset >= 0 && preset < PRESET_COUNT ? preset : 0];
}

/* ------------------------------------------------------------- kepler math */

static double norm360(double v) {
    v = fmod(v, 360.0);
    return v < 0 ? v + 360.0 : v;
}

static void elements_at(const Elements *e, double days, double *a, double *ec, double *inc,
                        double *L, double *peri, double *node) {
    double T = days / 36525.0;
    *a = e->a + e->da * T;
    *ec = e->e + e->de * T;
    *inc = e->i + e->di * T;
    *L = e->L + e->dL * T;
    *peri = e->peri + e->dperi * T;
    *node = e->node + e->dnode * T;
    if (*ec < 0) *ec = 0;
    if (*ec > 0.999) *ec = 0.999;
}

void kepler_position(const Elements *el, double days, Vec3 *out) {
    double a, e, inc, L, peri, node;
    elements_at(el, days, &a, &e, &inc, &L, &peri, &node);
    double M = norm360(L - peri) * DEG2RAD;
    double E = M;
    for (int i = 0; i < 12; i++) {
        double d = (E - e * sin(E) - M) / (1 - e * cos(E));
        E -= d;
        if (fabs(d) < 1e-12) break;
    }
    double xv = a * (cos(E) - e);
    double yv = a * sqrt(1 - e * e) * sin(E);
    double w = (peri - node) * DEG2RAD;
    double om = node * DEG2RAD;
    double ii = inc * DEG2RAD;
    double cw = cos(w), sw = sin(w), co = cos(om), so = sin(om), ci = cos(ii), si = sin(ii);
    out->x = (cw * co - sw * so * ci) * xv + (-sw * co - cw * so * ci) * yv;
    out->y = (cw * so + sw * co * ci) * xv + (-sw * so + cw * co * ci) * yv;
    out->z = (sw * si) * xv + (cw * si) * yv;
}

void kepler_velocity(const Elements *el, double days, Vec3 *out) {
    Vec3 p0, p1;
    kepler_position(el, days - 0.5, &p0);
    kepler_position(el, days + 0.5, &p1);
    out->x = p1.x - p0.x;
    out->y = p1.y - p0.y;
    out->z = p1.z - p0.z;
}

static void orbit_point_at(const Body *b, double days, Vec3 *out) {
    kepler_position(&b->el, days, out);
}

/* ------------------------------------------------------------- sim basics */

void sim_init(Sim *s) {
    memset(s, 0, sizeof(*s));
    s->next_id = 1;
    s->speed = 1.0;
    s->t = 0;
    s->collisions = 1;
    s->preset = PRESET_FULL;
}

static int find_by_name(const Sim *s, const char *name) {
    for (int i = 0; i < s->count; i++)
        if (s->bodies[i].alive && strcmp(s->bodies[i].name, name) == 0) return i;
    return -1;
}

static int add_proto(Sim *s, const Body *proto) {
    if (s->count >= MAX_BODIES) return -1;
    int idx = s->count++;
    s->bodies[idx] = *proto;
    s->bodies[idx].id = s->next_id++;
    s->bodies[idx].alive = 1;
    s->bodies[idx].trail_head = 0;
    s->bodies[idx].trail_count = 0;
    return idx;
}

static void make_sun(Body *b, const char *name, double mass, double radius, SsColor c1, SsColor c2) {
    memset(b, 0, sizeof(*b));
    strncpy(b->name, name, sizeof(b->name) - 1);
    strncpy(b->label, name, sizeof(b->label) - 1);
    b->kind = BT_STAR;
    b->mass_kg = mass;
    b->radius_km = radius;
    b->day_hours = 609.12;
    b->col1 = c1;
    b->col2 = c2;
    b->accent = RGB_A(255, 200, 110);
    b->atmosphere = 1;
    b->tex_seed = 7;
    b->parent = -1;
    b->pinned = 1;
    b->is_sun = 1;
    b->fact = "G-type main-sequence star holding 99.86% of the system's mass.";
    b->moon_count = 0;
}

static void add_planet_from_data(Sim *s, const PlanetData *p) {
    Body b;
    memset(&b, 0, sizeof(b));
    strncpy(b.name, p->name, sizeof(b.name) - 1);
    strncpy(b.label, p->label, sizeof(b.label) - 1);
    b.kind = p->kind;
    b.mass_kg = p->mass_kg;
    b.radius_km = p->radius_km;
    b.day_hours = p->day_hours;
    b.axial_tilt = p->tilt;
    b.moon_count = p->moons;
    b.fact = p->fact;
    b.col1 = p->c1;
    b.col2 = p->c2;
    b.accent = p->accent;
    b.ringed = p->ringed;
    b.ring_in = p->ring_in;
    b.ring_out = p->ring_out;
    b.ring_col = p->ring_col;
    b.atmosphere = p->atmo;
    b.tex_seed = p->tex;
    b.el = p->el;
    b.has_elements = 1;
    b.on_rail = 1;
    b.parent = -1;
    b.pinned = 0;
    add_proto(s, &b);
}

static void add_small_from_data(Sim *s, const SmallBody *p) {
    Body b;
    memset(&b, 0, sizeof(b));
    strncpy(b.name, p->name, sizeof(b.name) - 1);
    strncpy(b.label, p->label, sizeof(b.label) - 1);
    b.kind = p->kind;
    b.mass_kg = p->mass_kg;
    b.radius_km = p->radius_km;
    b.day_hours = p->day_hours;
    b.fact = p->fact;
    b.col1 = p->c1;
    b.col2 = p->c2;
    b.accent = p->accent;
    b.tex_seed = p->tex;
    b.el = p->el;
    b.has_elements = 1;
    b.on_rail = 1;
    b.parent = -1;
    add_proto(s, &b);
}

static void add_moon_from_data(Sim *s, const MoonData *m, double phase) {
    int p = find_by_name(s, m->parent);
    if (p < 0) return;
    Body b;
    memset(&b, 0, sizeof(b));
    strncpy(b.name, m->name, sizeof(b.name) - 1);
    strncpy(b.label, m->name, sizeof(b.label) - 1);
    b.kind = BT_MOON;
    b.mass_kg = m->mass_kg;
    b.radius_km = m->radius_km;
    b.fact = m->fact;
    b.col1 = m->c1;
    b.col2 = m->c2;
    b.accent = m->c1;
    b.tex_seed = m->tex;
    b.parent = p;
    b.on_rail = 1;
    b.rail_a_km = m->a_km;
    b.rail_period_d = m->period_d;
    b.rail_incl = m->incl;
    b.rail_node = phase * 37.0;
    b.rail_phase = phase;
    b.day_hours = fabs(m->period_d) * 24.0;
    add_proto(s, &b);
    if (p >= 0) s->bodies[p].moon_count++;
}

void sim_load_preset(Sim *s, int preset, double start_days) {
    int keep_speed = 0;
    double speed = s->speed;
    int paused = s->paused;
    (void)keep_speed;
    memset(s->bodies, 0, sizeof(s->bodies));
    s->count = 0;
    s->next_id = 1;
    s->event_count = 0;
    s->preset = preset;
    s->t = start_days;
    s->speed = speed;
    s->paused = paused;
    s->nbody = 0;

    if (preset != PRESET_BINARY) {
        Body sun;
        make_sun(&sun, "Sun", 1.98847e30, 696340.0, RGB_A(255, 226, 156), RGB_A(255, 132, 40));
        sun.tex_seed = 7;
        add_proto(s, &sun);
    }

    if (preset == PRESET_EMPTY) {
        sim_resolve(s);
        return;
    }

    if (preset == PRESET_BINARY) {
        Body b;
        make_sun(&b, "Alpha", 1.0 * 1.98847e30, 610000.0, RGB_A(255, 236, 190), RGB_A(255, 150, 60));
        b.pos = (Vec3){-0.22, 0, 0};
        b.pinned = 0;
        b.dynamic = 1;
        b.vel = (Vec3){0, 0.0180, 0};
        b.on_rail = 0;
        b.is_sun = 1;
        add_proto(s, &b);
        make_sun(&b, "Beta", 0.82 * 1.98847e30, 560000.0, RGB_A(255, 214, 168), RGB_A(255, 108, 48));
        b.pos = (Vec3){0.22, 0, 0};
        b.pinned = 0;
        b.dynamic = 1;
        b.vel = (Vec3){0, -0.0219, 0};
        b.on_rail = 0;
        b.tex_seed = 9;
        add_proto(s, &b);
        /* circumbinary planets */
        struct { const char *name; double a, ph, rad, m; SsColor c; } pdata[3] = {
            { "Tatooine I", 2.4, 0.2, 6200, 5.2e24, RGB_A(206, 156, 108) },
            { "Tatooine II", 3.6, 3.4, 9800, 1.1e25, RGB_A(126, 180, 226) },
            { "Tatooine III", 5.8, 5.1, 15800, 2.4e25, RGB_A(196, 186, 168) },
        };
        for (int i = 0; i < 3; i++) {
            Body p;
            memset(&p, 0, sizeof(p));
            strncpy(p.name, pdata[i].name, sizeof(p.name) - 1);
            snprintf(p.label, sizeof(p.label), "BARY-%d", i + 1);
            p.kind = BT_GIANT;
            p.mass_kg = pdata[i].m;
            p.radius_km = pdata[i].rad;
            p.col1 = pdata[i].c;
            p.col2 = RGBA(COL_R(pdata[i].c) / 2, COL_G(pdata[i].c) / 2, COL_B(pdata[i].c) / 2, 255);
            p.accent = pdata[i].c;
            p.tex_seed = 70 + i;
            p.fact = "A circumbinary world in a shared orbit around two suns.";
            p.rail_a_km = 0;
            p.el = (Elements){pdata[i].a, 0.02 * i, 1.5 * i, pdata[i].ph * RAD2DEG, 0, 0, 0, 0, 0, 0, 0, 0};
            p.has_elements = 1;
            p.on_rail = 1;
            p.parent = -1;
            add_proto(s, &p);
        }
        s->nbody = 1;
        sim_resolve(s);
        return;
    }

    int full = preset == PRESET_FULL;
    int inner = preset == PRESET_INNER;
    int jovian = preset == PRESET_JOVIAN;
    int outer = preset == PRESET_OUTER;

    for (int i = 0; i < ARRAY_LEN(PLANETS); i++) {
        const PlanetData *p = &PLANETS[i];
        int is_dwarf = p->kind == BT_DWARF;
        if (inner && (!(strcmp(p->name, "Mercury") == 0 || strcmp(p->name, "Venus") == 0 ||
                       strcmp(p->name, "Earth") == 0 || strcmp(p->name, "Mars") == 0 || is_dwarf)))
            continue;
        if (jovian && !(strcmp(p->name, "Jupiter") == 0 || strcmp(p->name, "Saturn") == 0)) continue;
        if (outer && (strcmp(p->name, "Jupiter") == 0 || strcmp(p->name, "Saturn") == 0)) continue;
        add_planet_from_data(s, p);
    }
    if (full || inner) {
        add_small_from_data(s, &SMALL[0]); /* Ceres */
    }
    if (full || outer) {
        for (int k = 1; k < ARRAY_LEN(SMALL); k++) add_small_from_data(s, &SMALL[k]);
    }
    for (int i = 0; i < ARRAY_LEN(MOONS); i++) {
        const MoonData *m = &MOONS[i];
        int keep = full;
        if (inner) keep = strcmp(m->name, "Moon") == 0;
        if (jovian) keep = strcmp(m->parent, "Jupiter") == 0;
        if (outer) keep = strcmp(m->name, "Charon") == 0 || strcmp(m->name, "Triton") == 0;
        if (!keep) continue;
        add_moon_from_data(s, m, (double)i * 2.399963);
    }
    sim_resolve(s);
}

/* ------------------------------------------------------------- resolution */

void sim_resolve(Sim *s) {
    for (int i = 0; i < s->count; i++) {
        Body *b = &s->bodies[i];
        if (!b->alive) continue;
        if (b->on_rail && b->has_elements) {
            orbit_point_at(b, s->t, &b->wpos);
            if (b->parent >= 0 && b->parent < s->count) {
                /* moon: circular rail around parent */
                Body *p = &s->bodies[b->parent];
                double a_au = b->rail_a_km / KM_PER_AU;
                double ang = b->rail_phase + TAU * (s->t / b->rail_period_d);
                double inc = b->rail_incl * DEG2RAD;
                double node = b->rail_node * DEG2RAD;
                double cx = a_au * cos(ang), cy = a_au * sin(ang);
                double y2 = cy * cos(inc);
                double z2 = cy * sin(inc);
                double x3 = cx * cos(node) - y2 * sin(node);
                double y3 = cx * sin(node) + y2 * cos(node);
                b->wpos.x = p->wpos.x + x3;
                b->wpos.y = p->wpos.y + y3;
                b->wpos.z = p->wpos.z + z2;
                double pd = (b->rail_period_d != 0) ? b->rail_period_d : 1.0;
                double vx = -a_au * sin(ang) * TAU / pd;
                double vy = a_au * cos(ang) * TAU / pd;
                b->wvel.x = p->wvel.x + vx * cos(node) - vy * cos(inc) * sin(node);
                b->wvel.y = p->wvel.y + vx * sin(node) + vy * cos(inc) * cos(node);
                b->wvel.z = p->wvel.z + vy * sin(inc);
            } else {
                kepler_velocity(&b->el, s->t, &b->wvel);
            }
        } else if (b->parent >= 0 && b->on_rail) {
            Body *p = &s->bodies[b->parent];
            double a_au = b->rail_a_km / KM_PER_AU;
            double ang = b->rail_phase + TAU * (s->t / (b->rail_period_d ? b->rail_period_d : 1.0));
            b->wpos.x = p->wpos.x + a_au * cos(ang);
            b->wpos.y = p->wpos.y + a_au * sin(ang);
            b->wpos.z = p->wpos.z;
            b->wvel = p->wvel;
        } else {
            b->wpos = b->pos;
            b->wvel = b->vel;
        }
    }
}

/* ------------------------------------------------------------ integration */

static void accel_all(Sim *s, Vec3 *acc) {
    for (int i = 0; i < s->count; i++) {
        acc[i] = (Vec3){0, 0, 0};
        if (!s->bodies[i].alive) continue;
    }
    for (int i = 0; i < s->count; i++) {
        Body *a = &s->bodies[i];
        if (!a->alive || !a->dynamic || a->pinned) continue;
        for (int j = 0; j < s->count; j++) {
            Body *b = &s->bodies[j];
            if (i == j || !b->alive) continue;
            if (b->kind == BT_MOON) continue; /* moons ride rails, not sources */
            if (b->parent == i) continue;
            Vec3 d = {b->pos.x - a->pos.x, b->pos.y - a->pos.y, b->pos.z - a->pos.z};
            double r2 = d.x * d.x + d.y * d.y + d.z * d.z;
            double soft = 2e-4; /* AU: avoids singularities in close passes */
            if (r2 < 1e-8) r2 = 1e-8;
            double inv = 1.0 / (r2 + soft * soft);
            double invr = sqrt(inv);
            double f = G_AU_D2_PER_KG * b->mass_kg * invr * inv;
            acc[i].x += d.x * f;
            acc[i].y += d.y * f;
            acc[i].z += d.z * f;
        }
    }
}

static void integrate(Sim *s, double dt) {
    Vec3 acc[MAX_BODIES];
    accel_all(s, acc);
    for (int i = 0; i < s->count; i++) {
        Body *b = &s->bodies[i];
        if (!b->alive || !b->dynamic || b->pinned) continue;
        b->vel.x += acc[i].x * dt * 0.5;
        b->vel.y += acc[i].y * dt * 0.5;
        b->vel.z += acc[i].z * dt * 0.5;
        b->pos.x += b->vel.x * dt;
        b->pos.y += b->vel.y * dt;
        b->pos.z += b->vel.z * dt;
    }
    accel_all(s, acc);
    for (int i = 0; i < s->count; i++) {
        Body *b = &s->bodies[i];
        if (!b->alive || !b->dynamic || b->pinned) continue;
        b->vel.x += acc[i].x * dt * 0.5;
        b->vel.y += acc[i].y * dt * 0.5;
        b->vel.z += acc[i].z * dt * 0.5;
    }
    s->t += dt;
}

static void push_event(Sim *s, int type, Vec3 pos, int a, int b, const char *text) {
    if (s->event_count >= (int)ARRAY_LEN(s->events)) {
        memmove(&s->events[0], &s->events[1], sizeof(SimEvent) * (ARRAY_LEN(s->events) - 1));
        s->event_count = ARRAY_LEN(s->events) - 1;
    }
    SimEvent *e = &s->events[s->event_count++];
    memset(e, 0, sizeof(*e));
    e->type = type;
    e->pos = pos;
    e->a = a;
    e->b = b;
    if (text) {
        strncpy(e->text, text, sizeof(e->text) - 1);
    }
}

static void trail_push(Body *b) {
    Vec3 last;
    int have_last = b->trail_count > 0;
    if (have_last) {
        int idx = (b->trail_head - 1 + TRAIL_MAX) % TRAIL_MAX;
        last = b->trail[idx];
        double dx = b->pos.x - last.x, dy = b->pos.y - last.y, dz = b->pos.z - last.z;
        if (dx * dx + dy * dy + dz * dz < 4e-6) return;
    }
    b->trail[b->trail_head] = b->pos;
    b->trail_head = (b->trail_head + 1) % TRAIL_MAX;
    if (b->trail_count < TRAIL_MAX) b->trail_count++;
}

static void merge_bodies(Sim *s, int ia, int ib) {
    if (ia < 0 || ib < 0 || ia >= s->count || ib >= s->count) return;
    Body *a = &s->bodies[ia];
    Body *b = &s->bodies[ib];
    if (!a->alive || !b->alive) return;
    Body *big = a->mass_kg >= b->mass_kg ? a : b;
    Body *small = big == a ? b : a;
    double m1 = big->mass_kg, m2 = small->mass_kg;
    double mt = m1 + m2;
    if (mt <= 0) mt = 1;
    Vec3 p = {
        (big->wpos.x * m1 + small->wpos.x * m2) / mt,
        (big->wpos.y * m1 + small->wpos.y * m2) / mt,
        (big->wpos.z * m1 + small->wpos.z * m2) / mt,
    };
    Vec3 v = {
        (big->wvel.x * m1 + small->wvel.x * m2) / mt,
        (big->wvel.y * m1 + small->wvel.y * m2) / mt,
        (big->wvel.z * m1 + small->wvel.z * m2) / mt,
    };
    char text[80];
    snprintf(text, sizeof(text), "%s merged into %s", small->name, big->name);
    big->mass_kg = mt;
    big->radius_km = pow(pow(big->radius_km, 3) + pow(small->radius_km, 3), 1.0 / 3.0);
    big->on_rail = 0;
    big->dynamic = 1;
    big->parent = -1;
    big->pos = p;
    big->vel = v;
    big->wpos = p;
    big->wvel = v;
    small->alive = 0;
    push_event(s, 1, p, big->id, small->id, text);
}

static void check_collisions(Sim *s) {
    if (!s->collisions) return;
    for (int i = 0; i < s->count; i++) {
        Body *a = &s->bodies[i];
        if (!a->alive) continue;
        for (int j = i + 1; j < s->count; j++) {
            Body *b = &s->bodies[j];
            if (!b->alive) continue;
            if (!a->dynamic && !b->dynamic) continue;
            if (a->on_rail && b->on_rail) continue;
            if (a->parent == j || b->parent == i) continue; /* moons don't hit parents */
            if (a->kind == BT_MOON || b->kind == BT_MOON) continue;
            if (a->kind == BT_STAR && b->kind == BT_STAR && !a->dynamic && !b->dynamic) continue;
            double rr = (a->radius_km + b->radius_km) * SIM_COLLIDE_MUL * AU_KM;
            double dx = a->wpos.x - b->wpos.x, dy = a->wpos.y - b->wpos.y, dz = a->wpos.z - b->wpos.z;
            double d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < rr * rr && d2 > 0) {
                merge_bodies(s, i, j);
                if (!s->bodies[i].alive) break;
            }
        }
    }
}

void sim_set_dynamic(Sim *s, int index, int dynamic) {
    if (index < 0 || index >= s->count) return;
    Body *b = &s->bodies[index];
    if (b->kind == BT_MOON) return;
    if (dynamic) {
        if (!b->dynamic) {
            b->pos = b->wpos;
            b->vel = b->wvel;
            b->on_rail = 0;
            b->dynamic = 1;
            b->pinned = 0;
            b->trail_head = b->trail_count = 0;
        }
    } else {
        if (b->has_elements && b->parent < 0) {
            b->on_rail = 1;
            b->dynamic = 0;
            b->pinned = 0;
            b->trail_head = b->trail_count = 0;
        }
    }
}

void sim_advance(Sim *s, double real_seconds) {
    if (s->paused) return;
    if (real_seconds <= 0) return;
    if (real_seconds > 0.25) real_seconds = 0.25;
    s->elapsed_real += real_seconds;
    double dt_days = s->speed * real_seconds;

    if (s->nbody) {
        int any_dynamic = 0;
        for (int i = 0; i < s->count; i++) {
            Body *b = &s->bodies[i];
            if (!b->alive) continue;
            if (b->parent < 0 && b->kind != BT_MOON && !b->on_rail) {
                if (!b->dynamic) {
                    b->pos = b->wpos;
                    b->vel = b->wvel;
                    b->dynamic = 1;
                }
            }
            if (b->dynamic) any_dynamic = 1;
        }
        if (any_dynamic) {
            int steps = (int)fabs(dt_days / 0.02);
            if (steps < 1) steps = 1;
            if (steps > 900) steps = 900;
            double sub = dt_days / steps;
            for (int k = 0; k < steps; k++) {
                integrate(s, sub);
                if ((k & 3) == 0) sim_resolve(s);
            }
            for (int i = 0; i < s->count; i++) {
                Body *b = &s->bodies[i];
                if (b->alive && b->dynamic && !b->pinned) trail_push(b);
            }
            sim_resolve(s);
            check_collisions(s);
            sim_resolve(s);
        } else {
            s->t += dt_days;
            sim_resolve(s);
        }
    } else {
        s->t += dt_days;
        sim_resolve(s);
    }
}

void sim_set_time(Sim *s, double days) {
    s->t = days;
    sim_resolve(s);
}

/* ---------------------------------------------------------------- queries */

int sim_find(const Sim *s, int id) {
    for (int i = 0; i < s->count; i++)
        if (s->bodies[i].alive && s->bodies[i].id == id) return i;
    return -1;
}

int sim_add_body(Sim *s, const Body *proto, Vec3 pos, Vec3 vel, int parent) {
    if (s->count >= MAX_BODIES) return -1;
    Body b = *proto;
    b.parent = parent;
    b.alive = 1;
    b.on_rail = 0;
    b.dynamic = 1;
    b.pinned = 0;
    b.pos = pos;
    b.vel = vel;
    b.wpos = pos;
    b.wvel = vel;
    b.trail_head = b.trail_count = 0;
    int idx = add_proto(s, &b);
    if (idx >= 0) push_event(s, 2, pos, s->bodies[idx].id, -1, s->bodies[idx].name);
    return idx;
}

void sim_remove_body(Sim *s, int index) {
    if (index < 0 || index >= s->count) return;
    if (!s->bodies[index].alive) return;
    int id = s->bodies[index].id;
    s->bodies[index].alive = 0;
    /* orphan any moons */
    for (int i = 0; i < s->count; i++) {
        Body *b = &s->bodies[i];
        if (!b->alive) continue;
        if (b->parent == index) b->alive = 0;
    }
    for (int i = 0; i < s->count; i++) {
        if (s->bodies[i].alive && s->bodies[i].id == id) continue;
    }
}

void sim_clear_trail(Sim *s, int index) {
    if (index < 0 || index >= s->count) return;
    s->bodies[index].trail_head = 0;
    s->bodies[index].trail_count = 0;
}

double body_orbit_period(const Body *b, const Sim *s) {
    if (b->on_rail) {
        if (b->parent >= 0) return fabs(b->rail_period_d);
        if (b->has_elements) return 360.0 / (b->el.dL != 0 ? b->el.dL : 1.0) * 36525.0;
    }
    double mu = 0;
    if (b->parent >= 0) {
        if (b->parent < s->count) mu = G_AU_D2_PER_KG * s->bodies[b->parent].mass_kg;
    } else {
        double best = 0;
        for (int i = 0; i < s->count; i++) {
            if (!s->bodies[i].alive || strcmp(s->bodies[i].name, b->name) == 0) continue;
            if (s->bodies[i].kind == BT_STAR && s->bodies[i].mass_kg > best) best = s->bodies[i].mass_kg;
        }
        mu = G_AU_D2_PER_KG * (best > 0 ? best : 1.98847e30);
    }
    double r = body_distance_to_parent(b, s);
    if (r <= 1e-9 || mu <= 0) return 0;
    return TAU * sqrt(r * r * r / mu);
}

double body_distance_to_parent(const Body *b, const Sim *s) {
    Vec3 ref = {0, 0, 0};
    if (b->parent >= 0 && b->parent < s->count) {
        ref = s->bodies[b->parent].wpos;
    } else {
        double best = 1e30;
        int idx = -1;
        for (int i = 0; i < s->count; i++) {
            if (!s->bodies[i].alive) continue;
            if (strcmp(s->bodies[i].name, b->name) == 0) { idx = i; break; }
        }
        for (int i = 0; i < s->count; i++) {
            if (!s->bodies[i].alive || i == idx) continue;
            if (s->bodies[i].kind != BT_STAR) continue;
            double dx = b->wpos.x - s->bodies[i].wpos.x;
            double dy = b->wpos.y - s->bodies[i].wpos.y;
            double dz = b->wpos.z - s->bodies[i].wpos.z;
            double d = dx * dx + dy * dy + dz * dz;
            if (d < best) { best = d; ref = s->bodies[i].wpos; }
        }
    }
    double dx = b->wpos.x - ref.x, dy = b->wpos.y - ref.y, dz = b->wpos.z - ref.z;
    return sqrt(dx * dx + dy * dy + dz * dz);
}

void sim_orbit_points(const Sim *s, int index, Vec3 *out, int count) {
    if (index < 0 || index >= s->count) return;
    const Body *b = &s->bodies[index];
    if (b->parent >= 0 && b->parent < s->count) {
        const Body *p = &s->bodies[b->parent];
        double a_au = b->rail_a_km / KM_PER_AU;
        double inc = b->rail_incl * DEG2RAD;
        double node = b->rail_node * DEG2RAD;
        for (int i = 0; i < count; i++) {
            double ang = TAU * i / count;
            double cx = a_au * cos(ang), cy = a_au * sin(ang);
            double y2 = cy * cos(inc), z2 = cy * sin(inc);
            out[i].x = p->wpos.x + cx * cos(node) - y2 * sin(node);
            out[i].y = p->wpos.y + cx * sin(node) + y2 * cos(node);
            out[i].z = p->wpos.z + z2;
        }
        return;
    }
    if (b->on_rail && b->has_elements) {
        double period = 360.0 / (b->el.dL != 0 ? fabs(b->el.dL) : 1.0) * 36525.0;
        if (period > 400000 || period < 0.01) period = 365.25;
        double M0 = norm360(b->el.L - b->el.peri);
        for (int i = 0; i < count; i++) {
            double f = (double)i / count;
            Elements el = b->el;
            /* sample one full revolution from the current mean anomaly */
            el.L = M0 + f * 360.0;
            el.dL = 0;
            el.da = el.de = el.di = el.dperi = el.dnode = 0;
            kepler_position(&el, s->t, &out[i]);
        }
        return;
    }
    /* dynamic body: no closed orbit, return the trail instead */
    for (int i = 0; i < count; i++) out[i] = b->wpos;
}

void sim_rail_velocity(const Body *b, const Sim *s, Vec3 *out) {
    if (b->on_rail && b->has_elements && b->parent < 0) {
        kepler_velocity(&b->el, s->t, out);
    } else {
        *out = b->wvel;
    }
}

void sim_circular_velocity(const Sim *s, Vec3 pos, double central_mass_kg, Vec3 *out) {
    (void)s;
    double r = sqrt(pos.x * pos.x + pos.y * pos.y + pos.z * pos.z);
    if (r < 1e-6) { out->x = out->y = out->z = 0; return; }
    double mu = G_AU_D2_PER_KG * central_mass_kg;
    double v = sqrt(mu / r);
    out->x = -pos.y / r * v;
    out->y = pos.x / r * v;
    out->z = 0;
}

double body_visual_radius_px(const Body *b, double zoom, double dpi_scale, int true_scale,
                             double nearest_px) {
    double phys = b->radius_km * AU_KM * zoom;
    double r;
    if (true_scale) {
        r = phys;
    } else {
        if (b->kind == BT_STAR) {
            /* Stars grow with zoom far faster than planets so that they read
             * as points in the overview and as discs when you fly in. */
            double g = pow(zoom / (24.0 * dpi_scale), 0.695);
            if (!(g > 0) || g > 80) g = 1;
            r = 7.0 * g * dpi_scale;
            if (phys > r) r = phys;
            if (r < 1.45 * dpi_scale) return 0;
            return r;
        }
        double rel = b->radius_km / 6371.0;
        if (rel < 0) rel = 0;
        double base = 2.0 + 4.6 * sqrt(rel);
        double mul = 1.0;
        switch (b->kind) {
            case BT_STAR: mul = 0.60; break;
            case BT_GIANT: mul = 0.86; break;
            case BT_ICE: mul = 0.86; break;
            case BT_DWARF: mul = 0.92; break;
            case BT_MOON: mul = 0.80; break;
            case BT_ASTEROID:
            case BT_COMET: mul = 1.0; break;
            case BT_BLACKHOLE: mul = 0.0; break;
            default: mul = 1.0; break;
        }
        base *= mul;
        if (b->kind == BT_BLACKHOLE) base = 9.0;
        if ((b->kind == BT_ASTEROID || b->kind == BT_COMET) && base < 1.9) base = 1.9;
        double grow = pow(zoom / (24.0 * dpi_scale), 0.32);
        if (!(grow > 0) || grow > 8) grow = 1;
        r = base * grow * dpi_scale;
        if (phys > r) r = phys;
    }
    if (nearest_px > 0 && nearest_px < 1e6) {
        double cap = nearest_px * 0.55;
        if (cap < 1.6 * dpi_scale) cap = 1.6 * dpi_scale;
        if (r > cap) r = cap;
    }
    if (r < 1.45 * dpi_scale) return 0;
    return r;
}

/* ------------------------------------------------------------ serialization */

void sim_save(const Sim *s, Str *out) {
    str_add(out, "SSS1\n");
    char line[1024];
    Str l;
    str_init(&l, line, sizeof(line));
    str_add(&l, "sim ");
    str_add_float(&l, s->t, 6); str_addc(&l, ' ');
    str_add_float(&l, s->speed, 6); str_addc(&l, ' ');
    str_add_int(&l, s->paused); str_addc(&l, ' ');
    str_add_int(&l, s->nbody); str_addc(&l, ' ');
    str_add_int(&l, s->preset); str_addc(&l, ' ');
    str_add_int(&l, s->collisions); str_addc(&l, '\n');
    str_add(out, str_cstr(&l));

    for (int i = 0; i < s->count; i++) {
        const Body *b = &s->bodies[i];
        if (!b->alive) continue;
        str_clear(&l);
        str_add(&l, "body ");
        str_add_int(&l, b->id); str_addc(&l, ' ');
        str_add_int(&l, (int)b->kind); str_addc(&l, ' ');
        str_add_float(&l, b->mass_kg, 3); str_addc(&l, ' ');
        str_add_float(&l, b->radius_km, 2); str_addc(&l, ' ');
        str_add_float(&l, b->day_hours, 3); str_addc(&l, ' ');
        str_add_int(&l, b->parent >= 0 && b->parent < s->count ? s->bodies[b->parent].id : -1);
        str_addc(&l, ' ');
        str_add_int(&l, b->on_rail); str_addc(&l, ' ');
        str_add_int(&l, b->dynamic); str_addc(&l, ' ');
        str_add_int(&l, b->pinned); str_addc(&l, ' ');
        str_add_float(&l, b->pos.x, 8); str_addc(&l, ' ');
        str_add_float(&l, b->pos.y, 8); str_addc(&l, ' ');
        str_add_float(&l, b->pos.z, 8); str_addc(&l, ' ');
        str_add_float(&l, b->vel.x, 10); str_addc(&l, ' ');
        str_add_float(&l, b->vel.y, 10); str_addc(&l, ' ');
        str_add_float(&l, b->vel.z, 10); str_addc(&l, ' ');
        str_add_float(&l, b->rail_a_km, 2); str_addc(&l, ' ');
        str_add_float(&l, b->rail_period_d, 6); str_addc(&l, ' ');
        str_add_float(&l, b->rail_phase, 6); str_addc(&l, ' ');
        str_add_float(&l, b->rail_incl, 4); str_addc(&l, ' ');
        str_add_float(&l, b->rail_node, 4); str_addc(&l, ' ');
        str_add_int(&l, b->has_elements); str_addc(&l, ' ');
        str_add_float(&l, b->el.a, 9); str_addc(&l, ' ');
        str_add_float(&l, b->el.e, 9); str_addc(&l, ' ');
        str_add_float(&l, b->el.i, 6); str_addc(&l, ' ');
        str_add_float(&l, b->el.L, 6); str_addc(&l, ' ');
        str_add_float(&l, b->el.peri, 6); str_addc(&l, ' ');
        str_add_float(&l, b->el.node, 6); str_addc(&l, ' ');
        str_add_float(&l, b->el.da, 12); str_addc(&l, ' ');
        str_add_float(&l, b->el.de, 12); str_addc(&l, ' ');
        str_add_float(&l, b->el.di, 12); str_addc(&l, ' ');
        str_add_float(&l, b->el.dL, 8); str_addc(&l, ' ');
        str_add_float(&l, b->el.dperi, 8); str_addc(&l, ' ');
        str_add_float(&l, b->el.dnode, 8); str_addc(&l, ' ');
        str_add_int(&l, (int)b->col1); str_addc(&l, ' ');
        str_add_int(&l, (int)b->col2); str_addc(&l, ' ');
        str_add_int(&l, b->ringed); str_addc(&l, ' ');
        str_add_float(&l, b->ring_in, 3); str_addc(&l, ' ');
        str_add_float(&l, b->ring_out, 3); str_addc(&l, ' ');
        str_add_int(&l, (int)b->ring_col); str_addc(&l, ' ');
        str_add_int(&l, (int)b->accent); str_addc(&l, ' ');
        str_add_int(&l, b->atmosphere); str_addc(&l, ' ');
        str_add_int(&l, b->tex_seed); str_addc(&l, ' ');
        str_add_int(&l, b->is_sun); str_addc(&l, ' ');
        str_addc(&l, '"');
        str_add(&l, b->name);
        str_add(&l, "\" \"");
        str_add(&l, b->label);
        str_add(&l, "\"");
        str_addc(&l, '\n');
        str_add(out, str_cstr(&l));
    }
}

static const char *skip_ws(const char *p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

static const char *read_quoted(const char *p, char *out, int cap) {
    p = skip_ws(p);
    int n = 0;
    if (*p == '"') {
        p++;
        while (*p && *p != '"') {
            if (n < cap - 1) out[n++] = *p;
            p++;
        }
        if (*p == '"') p++;
    } else {
        while (*p && *p != ' ' && *p != '\n' && *p != '\r') {
            if (n < cap - 1) out[n++] = *p;
            p++;
        }
    }
    out[n] = 0;
    return p;
}

static const char *read_double(const char *p, double *v) {
    p = skip_ws(p);
    char *end = NULL;
    *v = strtod(p, &end);
    if (end == p) {
        /* resync: skip one token so a malformed field cannot stall the parser */
        while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
        *v = 0;
        return p;
    }
    return end;
}

int sim_load(Sim *s, const char *text) {
    if (!text) return 0;
    const char *p = text;
    if (strncmp(p, "SSS1", 4) != 0) return 0;
    p = strchr(p, '\n');
    if (!p) return 0;
    p++;
    Sim *newsim = (Sim *)sss_alloc(sizeof(Sim));
    Body *bodies = (Body *)sss_alloc(sizeof(Body) * MAX_BODIES);
    if (!newsim || !bodies) {
        sss_free(newsim);
        sss_free(bodies);
        return 0;
    }
    newsim->speed = 1.0;
    newsim->next_id = 1;
    newsim->collisions = 1;
    int n = 0;
    int parsed_sim = 0;
    while (*p) {
        p = skip_ws(p);
        if (!*p) break;
        if (strncmp(p, "sim ", 4) == 0) {
            p += 4;
            p = read_double(p, &newsim->t);
            p = read_double(p, &newsim->speed);
            double v;
            p = read_double(p, &v); newsim->paused = (int)v;
            p = read_double(p, &v); newsim->nbody = (int)v;
            p = read_double(p, &v); newsim->preset = (int)v;
            p = read_double(p, &v); newsim->collisions = (int)v;
            parsed_sim = 1;
        } else if (strncmp(p, "body ", 5) == 0) {
            p += 5;
            Body *bp = &bodies[n];
            memset(bp, 0, sizeof(Body));
            double v;
            for (int k = 0; k < 9; k++) {
                p = read_double(p, &v);
                switch (k) {
                    case 0: bp->id = (int)v; break;
                    case 1: bp->kind = (BodyKind)(int)v; break;
                    case 2: bp->mass_kg = v; break;
                    case 3: bp->radius_km = v; break;
                    case 4: bp->day_hours = v; break;
                    case 5: bp->parent = (int)v; break;
                    case 6: bp->on_rail = (int)v; break;
                    case 7: bp->dynamic = (int)v; break;
                    case 8: bp->pinned = (int)v; break;
                }
            }
            for (int k = 0; k < 6; k++) {
                p = read_double(p, &v);
                if (k == 0) bp->pos.x = v;
                else if (k == 1) bp->pos.y = v;
                else if (k == 2) bp->pos.z = v;
                else if (k == 3) bp->vel.x = v;
                else if (k == 4) bp->vel.y = v;
                else bp->vel.z = v;
            }
            for (int k = 0; k < 5; k++) {
                p = read_double(p, &v);
                if (k == 0) bp->rail_a_km = v;
                else if (k == 1) bp->rail_period_d = v;
                else if (k == 2) bp->rail_phase = v;
                else if (k == 3) bp->rail_incl = v;
                else bp->rail_node = v;
            }
            p = read_double(p, &v); bp->has_elements = (int)v;
            double *el = (double *)&bp->el;
            for (int k = 0; k < 12; k++) {
                p = read_double(p, &v);
                el[k] = v;
            }
            p = read_double(p, &v); bp->col1 = (SsColor)(unsigned)v;
            p = read_double(p, &v); bp->col2 = (SsColor)(unsigned)v;
            p = read_double(p, &v); bp->ringed = (int)v;
            p = read_double(p, &v); bp->ring_in = v;
            p = read_double(p, &v); bp->ring_out = v;
            p = read_double(p, &v); bp->ring_col = (SsColor)(unsigned)v;
            p = read_double(p, &v); bp->accent = (SsColor)(unsigned)v;
            p = read_double(p, &v); bp->atmosphere = (int)v;
            p = read_double(p, &v); bp->tex_seed = (int)v;
            p = read_double(p, &v); bp->is_sun = (int)v;
            bp->alive = 1;
            bp->wpos = bp->pos;
            p = read_quoted(p, bp->name, sizeof(bp->name));
            p = read_quoted(p, bp->label, sizeof(bp->label));
            n++;
        } else {
            p = strchr(p, '\n');
            if (!p) break;
            p++;
        }
        while (*p == ' ' || *p == '\t' || *p == '\r') p++;
    }
    if (!parsed_sim || n == 0) {
        sss_free(newsim);
        sss_free(bodies);
        return 0;
    }
    newsim->count = n;
    memcpy(newsim->bodies, bodies, sizeof(Body) * (size_t)n);
    sss_free(bodies);
    for (int i = 0; i < n; i++) {
        Body *b = &newsim->bodies[i];
        int parent_id = b->parent;
        b->parent = -1;
        if (parent_id >= 0) {
            for (int j = 0; j < n; j++) {
                if (newsim->bodies[j].id == parent_id) { b->parent = j; break; }
            }
        }
    }
    int max_id = 0;
    for (int i = 0; i < n; i++)
        if (newsim->bodies[i].id > max_id) max_id = newsim->bodies[i].id;
    newsim->next_id = max_id + 1;
    memcpy(s, newsim, sizeof(Sim));
    sss_free(newsim);
    sim_resolve(s);
    return 1;
}
