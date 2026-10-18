#include <stdio.h>
#include <math.h>
#include <string.h>
#include "sim.h"

static int fails = 0;
static void unbuf(void) { setvbuf(stdout, NULL, _IONBF, 0); }
#define CHECK(cond, msg, ...) do { if (!(cond)) { printf("FAIL: " msg "\n", ##__VA_ARGS__); fails++; } else { printf("ok   : " msg "\n", ##__VA_ARGS__); } } while (0)

static int find(Sim *s, const char *n) { for (int i=0;i<s->count;i++) if (s->bodies[i].alive && !strcmp(s->bodies[i].name,n)) return i; return -1; }

int main(void) {
    unbuf();
    static Sim s; sim_init(&s);
    double jd = date_to_jd(2026, 10, 18, 0, 0, 0);
    printf("J2000 days for 2026-10-18: %.4f\n", jd);
    int y,m,d,h,mi,se; jd_to_date(jd, &y,&m,&d,&h,&mi,&se);
    CHECK(y==2026 && m==10 && d==18, "date roundtrip -> %04d-%02d-%02d %02d:%02d:%02d", y,m,d,h,mi,se);
    double jd2 = date_to_jd(2000,1,1,12,0,0);
    CHECK(fabs(jd2) < 1e-9, "J2000 epoch maps to 0 days (%.6f)", jd2);

    sim_load_preset(&s, PRESET_FULL, jd);
    CHECK(s.count == 40, "full preset body count = %d (1 sun + 9 planets + 9 small + 21 moons)", s.count);
    int e = find(&s, "Earth"), j = find(&s, "Jupiter"), sa = find(&s, "Saturn");
    CHECK(e>=0 && j>=0 && sa>=0, "planets present");
    double de = body_distance_to_parent(&s.bodies[e], &s);
    double dj = body_distance_to_parent(&s.bodies[j], &s);
    double ds = body_distance_to_parent(&s.bodies[sa], &s);
    printf("     distances: Earth %.4f AU, Jupiter %.4f AU, Saturn %.4f AU\n", de, dj, ds);
    CHECK(de > 0.95 && de < 1.05, "Earth near 1 AU at 2026-10-18 (%.4f)", de);
    CHECK(dj > 4.0 && dj < 6.5, "Jupiter in range (%.3f)", dj);
    CHECK(ds > 8.0 && ds < 11.2, "Saturn in range (%.3f)", ds);
    double pe = body_orbit_period(&s.bodies[e], &s) / 365.25;
    double pj = body_orbit_period(&s.bodies[j], &s) / 365.25;
    CHECK(fabs(pe-1.0) < 0.02, "Earth period %.3f y", pe);
    CHECK(fabs(pj-11.86) < 0.1, "Jupiter period %.3f y", pj);

    /* advance 30 days, Earth should have moved ~29.5 deg */
    Vec3 p0 = s.bodies[e].wpos;
    double ang0 = atan2(p0.y, p0.x);
    s.speed = 1.0; /* day per second */
    for (int i=0;i<120;i++) sim_advance(&s, 0.25);
    Vec3 p1 = s.bodies[e].wpos;
    double ang1 = atan2(p1.y, p1.x);
    double swept = fmod(ang1 - ang0 + TAU*2, TAU) * RAD2DEG;
    printf("     30 days swept %.2f deg (expected ~29.7)\n", swept);
    CHECK(fabs(swept - 29.7) < 1.0, "Earth swept %.2f deg in 30 days", swept);

    /* save/load roundtrip */
    static char buf[400000]; Str out; str_init(&out, buf, sizeof(buf));
    sim_save(&s, &out);
    printf("     save size %d bytes\n", out.len);
    static Sim t; sim_init(&t);
    CHECK(sim_load(&t, buf) == 1, "load succeeds");
    CHECK(t.count == s.count, "roundtrip count %d == %d", t.count, s.count);
    int e2 = find(&t, "Earth");
    CHECK(e2 >= 0 && fabs(t.bodies[e2].wpos.x - s.bodies[e].wpos.x) < 1e-6, "Earth position preserved");
    int moon = find(&t, "Moon");
    CHECK(moon >= 0 && t.bodies[moon].parent == e2, "Moon parent remapped to Earth");
    double dm = body_distance_to_parent(&t.bodies[moon], &t);
    CHECK(dm*KM_PER_AU > 3.0e5 && dm*KM_PER_AU < 4.5e5, "Moon distance %.0f km", dm*KM_PER_AU);

    /* nbody sandbox: circular orbit should stay circular for a year */
    sim_load_preset(&t, PRESET_EMPTY, 0);
    Body proto; memset(&proto,0,sizeof(proto));
    strcpy(proto.name, "TestRock"); proto.kind = BT_ASTEROID; proto.mass_kg = 1e20; proto.radius_km = 500;
    proto.col1 = RGB_A(200,180,160); proto.col2 = RGB_A(90,80,70); proto.accent = RGB_A(200,180,160);
    Vec3 pos = {2.0, 0, 0}, vel; sim_circular_velocity(&t, pos, t.bodies[0].mass_kg, &vel);
    int rock = sim_add_body(&t, &proto, pos, vel, -1);
    t.nbody = 1; t.speed = 30.0;
    for (int i=0;i<400;i++) sim_advance(&t, 0.05);
    double rr = sqrt(t.bodies[rock].wpos.x*t.bodies[rock].wpos.x + t.bodies[rock].wpos.y*t.bodies[rock].wpos.y);
    double days = t.t;
    printf("     nbody: %.1f days later r=%.5f AU, z=%.5f\n", days, rr, t.bodies[rock].wpos.z);
    CHECK(fabs(rr-2.0) < 0.02, "circular orbit radius preserved (%.4f)", rr);
    CHECK(t.bodies[rock].trail_count > 50, "trail recorded (%d)", t.bodies[rock].trail_count);

    sim_load_preset(&t, PRESET_BINARY, 0);
    CHECK(t.count == 5 && t.nbody == 1, "binary preset loads (%d bodies)", t.count);
    printf("\n%s (%d failures)\n", fails ? "TESTS FAILED" : "ALL TESTS PASSED", fails);
    return fails ? 1 : 0;
}
