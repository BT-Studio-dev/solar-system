'use strict';
/* Solar System Sandbox — simulation engine.
 *
 * Planets ride JPL's approximate ephemerides ("rails") until gravity is
 * released or a body is created; from that moment the whole system runs as a
 * symplectic velocity-Verlet N-body integration in AU / days / solar masses.
 * Rails bodies still attract user bodies while they are on rails, so a new
 * planet immediately feels the real, moving gravitational field.
 */

const DEG = Math.PI / 180;
const TAU = Math.PI * 2;

function nowJD() { return Date.now() / DAY_MS + 2440587.5; }

function keplerSolve(M, e) {
  let E = e < 0.8 ? M : Math.PI;
  for (let i = 0; i < 12; i++) {
    const dE = (E - e * Math.sin(E) - M) / (1 - e * Math.cos(E));
    E -= dE;
    if (Math.abs(dE) < 1e-9) break;
  }
  return E;
}

/* Orbital-plane (xp, yp) -> ecliptic (x, y, z) via argument of perihelion w,
 * inclination I and ascending node O (radians). */
function orient(xp, yp, w, O, I) {
  const cw = Math.cos(w), sw = Math.sin(w);
  const cO = Math.cos(O), sO = Math.sin(O);
  const cI = Math.cos(I), sI = Math.sin(I);
  return [
    (cw * cO - sw * sO * cI) * xp + (-sw * cO - cw * sO * cI) * yp,
    (cw * sO + sw * cO * cI) * xp + (-sw * sO + cw * cO * cI) * yp,
    (sw * sI) * xp + (cw * sI) * yp,
  ];
}

/* Element set at Julian century T -> {a, e, I, w, O} in AU / radians. */
function elementsAt(el, T) {
  return {
    a: el.a[0] + el.a[1] * T,
    e: el.e[0] + el.e[1] * T,
    I: (el.I[0] + el.I[1] * T) * DEG,
    w: (el.w[0] + el.w[1] * T) * DEG,
    O: (el.O[0] + el.O[1] * T) * DEG,
    L: (el.L[0] + el.L[1] * T) * DEG,
  };
}

/* Heliocentric ecliptic position of a rails planet at T (centuries past J2000). */
function planetPosition(key, jd) {
  const el = elementsAt(PLANET_ELEMENTS[key], (jd - J2000_JD) / 36525);
  const M = el.L - el.w; // mean anomaly = mean longitude - longitude of perihelion
  const E = keplerSolve(((M % TAU) + TAU) % TAU, el.e);
  const xp = el.a * (Math.cos(E) - el.e);
  const yp = el.a * Math.sqrt(1 - el.e * el.e) * Math.sin(E);
  const p = orient(xp, yp, el.w - el.O, el.O, el.I); // w-arg = varpi - Omega
  return { pos: p, elem: el };
}

/* Moon position relative to its parent: near-circular orbit in the ecliptic. */
function moonOffset(m, jd) {
  const tDays = jd - J2000_JD;
  const ang = m.phase0 + TAU * tDays / m.periodDays;
  const a = m.aAU * (1 - m.mEcc * Math.cos(ang));
  return [a * Math.cos(ang), a * Math.sin(ang), 0];
}

/* ------------------------------------------------------------ Simulation -- */

class Simulation {
  constructor() {
    this.bodies = [];        // all bodies, rails and dynamic
    this.byId = new Map();
    this.mode = 'rails';     // 'rails' | 'nbody'
    this.jd = nowJD();
    this.speed = 1;          // days of sim time per real second
    this.paused = false;
    this.nextId = 1;
    this.events = [];        // {text} — merges, spawns; drained by the UI
    this.onRailsCount = 0;
  }

  loadPreset(id) {
    this.bodies = [];
    this.byId.clear();
    this.mode = 'rails';
    this.jd = nowJD();
    this.nextId = 1;
    if (id === 'empty') {
      this.addBody(this.makeFromCatalog('sun'));
    } else if (id === 'inner') {
      for (const k of ['sun', 'mercury', 'venus', 'earth', 'moon', 'mars'])
        this.addBody(this.makeFromCatalog(k));
    } else if (id === 'giants') {
      for (const k of ['sun', 'jupiter', 'io', 'europa', 'ganymede', 'callisto', 'saturn', 'titan'])
        this.addBody(this.makeFromCatalog(k));
    } else if (id === 'binary') {
      this.loadBinary();
    } else {
      for (const def of CATALOG) this.addBody(this.makeFromCatalog(def.key));
    }
  }

  /* Two stars and three circumbinary planets, integrated from t=0. */
  loadBinary() {
    this.mode = 'nbody';
    const m1 = 1.0, m2 = 0.6, d = 1.2;
    const omega = Math.sqrt(K2 * (m1 + m2) / (d * d * d));
    const r1 = d * m2 / (m1 + m2), r2 = d * m1 / (m1 + m2);
    const a = this.addBody(Object.assign(this.makeFromCatalog('sun'), {
      name: 'Sun A', massKg: m1 * MSUN_KG,
      state: { p: [-r1, 0, 0], v: [0, omega * r1, 0] },
    }));
    this.addBody(Object.assign(this.makeFromCatalog('sun'), {
      name: 'Sun B', massKg: m2 * MSUN_KG, color: '#ff9d5c', glow: '#ffd0a8',
      state: { p: [r2, 0, 0], v: [0, -omega * r2, 0] },
    }));
    const put = (name, dist, massEarth, radiusKm, color, phase, kind) => {
      const M = m1 + m2;
      const v = Math.sqrt(K2 * M / dist);
      this.addBody({
        id: this.nextId++, name, kind: kind || 'terrestrial', catalog: null,
        massKg: massEarth * MEARTH_KG, radiusKm, color,
        state: {
          p: [dist * Math.cos(phase), dist * Math.sin(phase), 0],
          v: [-v * Math.sin(phase), v * Math.cos(phase), 0],
        }, trail: [],
      });
    };
    put('Aegir', 2.6, 0.9, 6300, '#5f9fe8', 0.4);
    put('Brenn', 4.2, 0.4, 5200, '#d97e56', 2.6);
    put('Cael', 6.8, 120, 46000, '#9ed6da', 4.9, 'icegiant');
    return a;
  }

  makeFromCatalog(key) {
    const def = CATALOG.find(c => c.key === key);
    return {
      id: this.nextId++, name: def.name, kind: def.kind, catalog: def.key,
      elemKey: def.elem || null, parentKey: def.parent || null,
      aAU: def.aAU || 0, periodDays: def.periodDays || 0, mEcc: def.e || 0, phase0: def.phase0 || 0,
      massKg: def.massKg, radiusKm: def.radiusKm, color: def.color, ring: !!def.ring,
      glow: def.glow || null, periodLabel: def.period || null,
      pos: [0, 0, 0], state: null, trail: [],
    };
  }

  addBody(b) {
    this.bodies.push(b);
    this.byId.set(b.id, b);
    return b;
  }

  /* Create a custom body; if anything dynamic exists the system keeps running
   * live, otherwise it switches to N-body for the newcomer alone. */
  addCustom(o) {
    // o: {name, kind, massEarth, radiusKm, color, ring, posAU:[x,y], velAUd:[vx,vy]}
    const massKg = o.massEarth * MEARTH_KG;
    const b = {
      id: this.nextId++, name: o.name || this.suggestName(o.kind), kind: o.kind, catalog: null,
      elemKey: null, parentKey: null, massKg, radiusKm: o.radiusKm, color: o.color,
      ring: !!o.ring, glow: o.kind === 'star' ? '#ffe9b8' : (o.kind === 'blackhole' ? null : null),
      periodLabel: null, pos: o.posAU, state: { p: o.posAU, v: o.velAUd }, trail: [],
    };
    this.addBody(b);
    if (this.mode === 'rails') this.releaseToNBody();
    this.events.push({ text: `${b.name} added to the sandbox` });
    return b;
  }

  suggestName(kind) {
    const used = new Set(this.bodies.map(b => b.name));
    const pool = { asteroid: ['Ceres','Vesta','Pallas','Hygiea','Eunomia'], comet: ['Halley','Encke','Tempel','Swift','Borrelly'] }[kind] || [];
    for (const n of pool) if (!used.has(n)) return n;
    for (let i = 2; ; i++) { const n = `${kind[0].toUpperCase()}${kind.slice(1)}-${i}`; if (!used.has(n)) return n; }
  }

  /* Snapshot every rails body into a dynamic state (pos via ephemeris, vel by
   * numeric differentiation), then integrate everything.  All positions are
   * snapshotted BEFORE any state is assigned, so moons see their still-moving
   * parent. */
  releaseToNBody() {
    if (this.mode === 'nbody') return;
    const h = 0.02; // days
    const snap = this.bodies.map(b => ({
      b,
      p0: this.bodyPos(b, this.jd),
      p1: this.bodyPos(b, this.jd + h),
    }));
    for (const { b, p0, p1 } of snap) {
      for (let i = 0; i < 3; i++) b.pos[i] = p0[i];
      b.state = {
        p: p0.slice(),
        v: [(p1[0] - p0[0]) / h, (p1[1] - p0[1]) / h, (p1[2] - p0[2]) / h],
      };
      b.elemKey = null; // stay on the drawn rail, leave the real motion
      b.parentKey = null;
    }
    this.mode = 'nbody';
    this.events.push({ text: 'Gravity released — everything pulls on everything now' });
  }

  removeBody(id) {
    const b = this.byId.get(id);
    if (!b || b.catalog && b.kind === 'star' && this.bodies.filter(x => x.kind === 'star').length === 1) return null;
    this.byId.delete(id);
    this.bodies = this.bodies.filter(x => x.id !== id);
    this.events.push({ text: `${b.name} removed` });
    return b;
  }

  /* Current position of a body (fresh vector — safe to hold across calls). */
  bodyPos(b, jd) { return this.bodyPosInto(b, jd, [0, 0, 0]); }

  bodyPosInto(b, jd, out) {
    if (b.state) {
      out[0] = b.state.p[0]; out[1] = b.state.p[1]; out[2] = b.state.p[2];
      return out;
    }
    if (b.parentKey) {
      const parent = this.bodies.find(x => x.catalog === b.parentKey);
      const off = moonOffset(b, jd);
      const pp = parent ? this.bodyPos(parent, jd) : [0, 0, 0];
      out[0] = pp[0] + off[0]; out[1] = pp[1] + off[1]; out[2] = pp[2] + off[2];
      return out;
    }
    if (b.elemKey) {
      const { pos } = planetPosition(b.elemKey, jd);
      out[0] = pos[0]; out[1] = pos[1]; out[2] = pos[2];
      return out;
    }
    out[0] = b.pos[0]; out[1] = b.pos[1]; out[2] = b.pos[2];
    return out;
  }

  advance(dtSec) {
    if (this.paused || dtSec <= 0) return;
    const dDays = this.speed * dtSec;
    const dynamic = this.bodies.some(b => b.state);
    if (!dynamic) {
      this.jd += dDays;
      return;
    }
    // Substep so fast time-lapses stay stable around tight orbits.
    const maxStep = 0.004;
    const n = Math.min(600, Math.max(1, Math.ceil(Math.abs(dDays) / maxStep)));
    const h = dDays / n;
    for (let s = 0; s < n; s++) this.integrate(h);
    this.jd += dDays;
    for (const b of this.bodies) {
      if (!b.state) continue;
      b.trail.push(b.state.p[0], b.state.p[1]);
      if (b.trail.length > 1400) b.trail.splice(0, b.trail.length - 1400);
    }
  }

  /* Velocity Verlet; rails bodies act as moving gravitational sources. */
  integrate(h) {
    const bodies = this.bodies;
    const dyn = [];
    for (const b of bodies) if (b.state) dyn.push(b);
    if (!dyn.length) { this.jd += h; return; }

    // sources: masses that pull (everything except the moving body itself)
    const src = [];
    for (const b of bodies) {
      if (b.state) continue;
      src.push({ p: this.bodyPos(b, this.jd), m: b.massKg / MSUN_KG });
    }
    const G = K2;
    const EPS2 = 1e-10; // ~1.5e-5 AU softening

    const accelOf = (i) => {
      const p = dyn[i].state.p;
      let ax = 0, ay = 0, az = 0;
      for (const s of src) {
        const dx = s.p[0] - p[0], dy = s.p[1] - p[1], dz = s.p[2] - p[2];
        const r2 = dx * dx + dy * dy + dz * dz + EPS2;
        const f = G * s.m / (r2 * Math.sqrt(r2));
        ax += f * dx; ay += f * dy; az += f * dz;
      }
      for (let j = 0; j < dyn.length; j++) {
        if (j === i) continue;
        const q = dyn[j].state.p;
        const dx = q[0] - p[0], dy = q[1] - p[1], dz = q[2] - p[2];
        const r2 = dx * dx + dy * dy + dz * dz + EPS2;
        const f = G * (dyn[j].massKg / MSUN_KG) / (r2 * Math.sqrt(r2));
        ax += f * dx; ay += f * dy; az += f * dz;
      }
      return [ax, ay, az];
    };

    // (re)use cached acceleration from the previous step when available
    if (!dyn[0]._acc) for (const b of dyn) b._acc = [0, 0, 0];
    for (let i = 0; i < dyn.length; i++) {
      const st = dyn[i].state;
      st.p[0] += st.v[0] * h + 0.5 * dyn[i]._acc[0] * h * h;
      st.p[1] += st.v[1] * h + 0.5 * dyn[i]._acc[1] * h * h;
      st.p[2] += st.v[2] * h + 0.5 * dyn[i]._acc[2] * h * h;
    }
    const newAcc = new Array(dyn.length);
    for (let i = 0; i < dyn.length; i++) newAcc[i] = accelOf(i);
    for (let i = 0; i < dyn.length; i++) {
      const st = dyn[i].state;
      st.v[0] += 0.5 * (dyn[i]._acc[0] + newAcc[i][0]) * h;
      st.v[1] += 0.5 * (dyn[i]._acc[1] + newAcc[i][1]) * h;
      st.v[2] += 0.5 * (dyn[i]._acc[2] + newAcc[i][2]) * h;
      dyn[i]._acc = newAcc[i];
    }
    this.jd += h;
    // collisions every substep: fast bodies must not tunnel through a star,
    // and dynamic bodies can be captured by bodies still on their rails
    this.checkCollisions(dyn);
  }

  /* Merge any overlapping bodies.  Dynamic bodies collide with everything:
   * each other, and bodies still on their ephemeris rails. */
  checkCollisions(dyn) {
    for (let i = 0; i < dyn.length; i++) {
      const a = dyn[i];
      if (!this.byId.has(a.id)) continue;
      for (let j = 0; j < this.bodies.length; j++) {
        const b = this.bodies[j];
        if (b === a || !this.byId.has(b.id)) continue;
        const pb = b.state ? b.state.p : this.bodyPos(b, this.jd);
        const dx = a.state.p[0] - pb[0], dy = a.state.p[1] - pb[1];
        if (!isFinite(dx) || !isFinite(dy)) continue;
        const rSum = (a.radiusKm + b.radiusKm) / AU_KM;
        if (dx * dx + dy * dy > rSum * rSum) continue;
        const big = b.massKg >= a.massKg ? b : a;
        const small = big === b ? a : b;
        this.absorbInto(big, small, dyn);
        if (small === a) break;
      }
    }
  }

  absorbInto(big, small, dynList) {
    const mt = big.massKg + small.massKg;
    if (big.state && small.state) {
      for (let k = 0; k < 3; k++) {
        big.state.v[k] = (big.state.v[k] * big.massKg + small.state.v[k] * small.massKg) / mt;
        big.state.p[k] = (big.state.p[k] * big.massKg + small.state.p[k] * small.massKg) / mt;
      }
    }
    big.massKg = mt;
    if (small.radiusKm > big.radiusKm * 0.4) {
      big.radiusKm = Math.cbrt(big.radiusKm ** 3 + small.radiusKm ** 3);
    }
    this.bodies = this.bodies.filter(x => x !== small);
    this.byId.delete(small.id);
    if (dynList) {
      const idx = dynList.indexOf(small);
      if (idx >= 0) dynList.splice(idx, 1);
    }
    this.events.push({ text: `${small.name} absorbed by ${big.name}`, kind: 'merge' });
  }


  /* Osculating orbital elements of a dynamic body w.r.t. the dominant mass. */
  osculating(b) {
    if (!b.state) return null;
    let cx = 0, cy = 0, mt = 0;
    for (const o of this.bodies) {
      if (o === b) continue;
      if (o.kind !== 'star' && o.massKg <= b.massKg * 20) continue;
      const p = o.state ? o.state.p : this.bodyPos(o, this.jd);
      cx += p[0] * o.massKg; cy += p[1] * o.massKg; mt += o.massKg;
    }
    if (mt === 0) return null;
    cx /= mt; cy /= mt;
    const px = b.state.p[0] - cx, py = b.state.p[1] - cy;
    const vx = b.state.v[0], vy = b.state.v[1];
    const r = Math.hypot(px, py);
    const v2 = vx * vx + vy * vy;
    const mu = K2 * (mt / MSUN_KG);
    if (r === 0 || mu === 0) return null;
    const aInv = 2 / r - v2 / mu;
    if (aInv <= 0) return { a: Infinity, periodDays: Infinity };
    const a = 1 / aInv;
    return { a, periodDays: TAU * Math.sqrt(a * a * a / mu) };
  }

  drainEvents() {
    const e = this.events;
    this.events = [];
    return e;
  }

  serialize() {
    // snapshot as a fully dynamic scenario at the current epoch
    const h = 0.02;
    const out = [];
    for (const b of this.bodies) {
      const p = this.bodyPos(b, this.jd).slice();
      let v = [0, 0, 0];
      if (b.state) v = b.state.v.slice();
      else {
        const p1 = this.bodyPos(b, this.jd + h);
        v = [(p1[0] - p[0]) / h, (p1[1] - p[1]) / h, 0];
      }
      out.push({
        name: b.name, kind: b.kind, massKg: b.massKg, radiusKm: b.radiusKm,
        color: b.color, ring: b.ring || false, p, v,
      });
    }
    return { app: 'solar-system-sandbox', version: 1, epochJD: this.jd, bodies: out };
  }

  loadScenario(data) {
    if (!data || data.app !== 'solar-system-sandbox' || !Array.isArray(data.bodies)) return false;
    this.bodies = [];
    this.byId.clear();
    this.mode = 'nbody';
    this.jd = data.epochJD || nowJD();
    this.nextId = 1;
    for (const d of data.bodies) {
      this.addBody({
        id: this.nextId++, name: String(d.name || 'Body'), kind: d.kind || 'terrestrial', catalog: null,
        elemKey: null, parentKey: null, massKg: +d.massKg || MEARTH_KG, radiusKm: +d.radiusKm || 6371,
        color: d.color || '#b0aa9e', ring: !!d.ring, glow: d.kind === 'star' ? '#ffe9b8' : null,
        periodLabel: null, pos: d.p.slice(), state: { p: d.p.slice(), v: d.v.slice() }, trail: [],
      });
    }
    return true;
  }
}
