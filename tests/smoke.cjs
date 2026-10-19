// Headless smoke test: stub the DOM/canvas, run the app scripts (paths from
// argv), exercise boot, frames, ephemeris accuracy, N-body stability, merges,
// and scenario round-trip.
'use strict';
const fs = require('fs');
const vm = require('vm');

function magicFn() {
  return new Proxy(function () {}, {
    get: (t, p) => {
      if (p === Symbol.toPrimitive) return () => 0;
      if (p === 'length') return 0;
      return magicFn();
    },
    apply: () => magicFn(),
    set: () => true,
  });
}
const ctx2d = magicFn();

function makeEl(id) {
  const listeners = {};
  const el = {
    id, style: {}, dataset: {}, children: [],
    value: '', checked: false, textContent: '', innerHTML: '',
    min: 0, max: 100, step: 1, title: '', className: '',
    clientWidth: 1280, clientHeight: 800, width: 1280, height: 800,
    files: [],
    classList: { add() {}, remove() {}, toggle() {}, contains: () => false },
    setAttribute() {}, getAttribute: () => null,
    appendChild(c) { this.children.push(c); return c; },
    addEventListener(type, fn) { (listeners[type] = listeners[type] || []).push(fn); },
    dispatch(type, ev) { for (const f of listeners[type] || []) f(ev || { target: el, preventDefault() {}, stopPropagation() {}, clientX: 0, clientY: 0 }); },
    querySelectorAll: () => [],
    querySelector: () => makeEl('q'),
    closest: () => null,
    getBoundingClientRect: () => ({ left: 0, top: 0, width: 1280, height: 800 }),
    remove() {}, click() { el.dispatch('click'); },
    getContext: () => ctx2d,
  };
  return el;
}

const els = new Map();
// Faithful DOM: only IDs that actually exist in index.html resolve; anything
// else returns null exactly like a browser, so phantom element references
// crash the harness instead of hiding until real users hit them.
const fsForIds = require('fs');
const htmlSrc = fsForIds.existsSync('index.html')
  ? fsForIds.readFileSync('index.html', 'utf8')
  : '';
const REAL_IDS = new Set([...htmlSrc.matchAll(/id="([^"]+)"/g)].map(m => m[1]));
const documentStub = {
  readyState: 'complete',
  documentElement: { dataset: {} },
  body: makeEl('body'),
  getElementById(id) {
    if (!REAL_IDS.has(id)) return null;
    if (!els.has(id)) els.set(id, makeEl(id));
    return els.get(id);
  },
  createElement(tag) {
    const el = makeEl('dyn-' + tag);
    if (tag === 'canvas') { el.getContext = () => ctx2d; }
    return el;
  },
  addEventListener() {},
  querySelectorAll: () => [],
};

let rafCb = null;
const store = {};
Object.assign(global, {
  document: documentStub,
  window: global,
  devicePixelRatio: 1,
  performance: { now: () => Date.now() },
  requestAnimationFrame: (cb) => { rafCb = cb; },
  localStorage: { getItem: (k) => store[k] ?? null, setItem: (k, v) => { store[k] = String(v); } },
  addEventListener() {},
  Blob: class { constructor(a) { this.a = a; } },
  URL: { createObjectURL: () => 'blob:x', revokeObjectURL() {} },
  FileReader: class { readAsText() {} },
});

const src = process.argv.slice(2)
  .map((f) => fs.readFileSync(f, 'utf8')).join('\n;\n');
vm.runInThisContext(src, { filename: 'bundle.js' });

const sss = global.__sss;
if (!sss) throw new Error('boot failed: no __sss hook');
const { sim } = sss;

let failures = 0;
function check(cond, label) {
  console.log((cond ? '  ok    ' : '  FAIL  ') + label);
  if (!cond) failures++;
}
function near(a, b, tol) { return Math.abs(a - b) <= tol; }

/* ------------------------------------------------- ephemeris accuracy -- */
{
  const earth = sim.bodies.find(b => b.name === 'Earth');
  const p = sim.bodyPos(earth, J2000_JD);
  const r = Math.hypot(p[0], p[1]);
  const lon = ((Math.atan2(p[1], p[0]) / DEG) + 360) % 360;
  check(near(r, 0.9833, 0.003), `Earth at J2000 perihelion (r=${r.toFixed(4)} AU ~0.983)`);
  check(near(lon, 100, 3), `Earth at J2000 longitude (${lon.toFixed(1)}° ~100°)`);

  const p2 = sim.bodyPos(earth, nowJD());
  const r2 = Math.hypot(p2[0], p2[1]);
  check(r2 > 0.95 && r2 < 1.03, `Earth today in range (r=${r2.toFixed(4)} AU)`);

  const moon = sim.bodies.find(b => b.name === 'Moon');
  const pm = sim.bodyPos(moon, nowJD());
  const pe = sim.bodyPos(earth, nowJD());
  const dMoon = Math.hypot(pm[0] - pe[0], pm[1] - pe[1]);
  check(near(dMoon, 0.00257, 0.0002), `Moon distance (${(dMoon * 384400 / 0.002570).toFixed(0)}k km)`);

  const jup = sim.bodies.find(b => b.name === 'Jupiter');
  const rj = Math.hypot(...sim.bodyPos(jup, nowJD()));
  check(rj > 4.9 && rj < 5.5, `Jupiter today (r=${rj.toFixed(3)} AU ~5.1)`);
}

/* ------------------------------------------------------- rails frames -- */
function frames(n, dtms = 16.7) {
  let t = performance.now();
  for (let i = 0; i < n; i++) { t += dtms; if (rafCb) { const cb = rafCb; rafCb = null; cb(t); } }
}
frames(30);
check(sim.bodies.length === CATALOG.length, `solar preset loaded (${sim.bodies.length} bodies)`);
check(sim.mode === 'rails', 'mode is rails initially');

/* ------------------------------------------------------------ n-body -- */
sim.releaseToNBody();
check(sim.mode === 'nbody', 'release to N-body');
frames(30);
const earth2 = sim.bodies.find(b => b.name === 'Earth');
check(!!earth2.state, 'Earth has dynamic state after release');
frames(300);
const pNow = sim.bodyPos(earth2, sim.jd);
const rNow = Math.hypot(pNow[0], pNow[1]);
check(isFinite(rNow) && rNow > 0.5 && rNow < 1.6, `Earth stays on ~1 AU after integration (r=${rNow.toFixed(3)})`);

/* moon stability through 40 days at 2 day/s */
sim.loadPreset('solar');
sim.releaseToNBody();
sim.speed = 2;
for (let d = 0; d < 40; d++) {
  for (let i = 0; i < 60; i++) sim.advance(1 / 60);
}
const lost = ['Io', 'Europa', 'Ganymede', 'Callisto', 'Titan', 'Triton', 'Moon']
  .filter(n => !sim.bodies.some(b => b.name === n));
check(lost.length === 0, `all moons stable over 40 days${lost.length ? ' (lost: ' + lost + ')' : ''}`);

/* energy conservation on the binary preset (rough) */
{
  sim.loadPreset('binary');
  const E = () => {
    let ke = 0, pe = 0;
    for (const a of sim.bodies) {
      const v = a.state.v, p = a.state.p;
      ke += 0.5 * (a.massKg / MSUN_KG) * (v[0] ** 2 + v[1] ** 2);
      for (const b of sim.bodies) {
        if (b === a) continue;
        const dx = b.state.p[0] - p[0], dy = b.state.p[1] - p[1];
        pe -= 0.5 * K2 * (a.massKg / MSUN_KG) * (b.massKg / MSUN_KG) / Math.hypot(dx, dy);
      }
    }
    return ke + pe;
  };
  sim.paused = false;
  const e0 = E();
  sim.speed = 5;
  for (let i = 0; i < 120; i++) sim.advance(1 / 60);
  const e1 = E();
  const dE = Math.abs((e1 - e0) / e0);
  check(dE < 0.01, `binary energy drift ${(dE * 100).toFixed(3)}% < 1%`);
}

/* ------------------------------------------------------- add + merge -- */
sim.loadPreset('solar');
frames(3);
const before = sim.bodies.length;
sim.addCustom({
  kind: 'comet', massEarth: 1e-7, radiusKm: 12, color: '#bcd8e8',
  name: 'Icarus', posAU: [0.02, 0, 0], velAUd: [0, 0.005, 0],
});
check(sim.bodies.length === before + 1, 'custom body added');
check(sim.mode === 'nbody', 'system switched to live gravity on add');
sim.speed = 2;
for (let i = 0; i < 400; i++) sim.advance(1 / 60);
check(!sim.bodies.some(b => b.name === 'Icarus'), 'comet dropped near the Sun is absorbed');

/* ------------------------------------------------------ serialize IO -- */
{
  const data = JSON.parse(JSON.stringify(sim.serialize()));
  const ok = sim.loadScenario(data);
  check(ok && sim.bodies.length === before, `serialize/load round-trip (${sim.bodies.length} bodies)`);
}

/* --------------------------------------------------------- UI wiring -- */
frames(60);
els.get('btn-play').dispatch('click');
check(els.get('btn-play').textContent === '▶', 'play button toggles to play state');
els.get('btn-play').dispatch('click');
els.get('tab-create').dispatch('click');
els.get('btn-add').dispatch('click');
for (let i = 0; i < THEMES.length; i++) els.get('btn-theme').dispatch('click');
frames(20);

/* ------------------------------------------------------- audio module -- */
check(typeof Sfx === 'object' && !!Sfx, 'Sfx module present');
check(Math.abs(Sfx.volume - 0.7) < 0.01, `default volume 0.7 (${Sfx.volume})`);
check(Sfx.muted === false, 'not muted by default');
Sfx.setVolume(0.35);
check(Sfx.volume === 0.35, 'setVolume persists state');
Sfx.setMuted(true);
check(Sfx.muted === true, 'setMuted(true)');
Sfx.setMuted(false);
Sfx.setAmbient(true);
Sfx.setAmbient(false);
for (const fn of ['select', 'add', 'remove', 'merge', 'gravity', 'load', 'theme', 'tick']) Sfx[fn]();
check(true, 'all sound effects callable without AudioContext (safe no-op)');
els.get('btn-volume').dispatch('click');
els.get('btn-mute').dispatch('click');
els.get('vol-slider').dispatch('input', { target: els.get('vol-slider') });
els.get('vol-ambient').dispatch('change', { target: els.get('vol-ambient') });
check(true, 'volume UI interactions do not throw');

/* ----------------------------------------------- simulation extras checks -- */
sss.setType('terrestrial');
els.get('qa-asteroid').dispatch('click');
const ast = sim.bodies[sim.bodies.length - 1];
check(sim.bodies.some(b => b.kind === 'asteroid'), 'quick action: spawn asteroid');
els.get('qa-comet').dispatch('click');
check(sim.bodies.some(b => b.kind === 'comet'), 'quick action: spawn comet');
els.get('qa-trails').dispatch('click');
check(sim.bodies.every(b => b.trail.length === 0), 'quick action: clear trails');
els.get('btn-set-date').dispatch('click');
check(true, 'start-date set does not throw');
els.get('track-sel').dispatch('change', { target: els.get('track-sel') });
els.get('sep-a').dispatch('change', { target: els.get('sep-a') });
els.get('sep-b').dispatch('change', { target: els.get('sep-b') });
check(true, 'track + separation selects do not throw');
els.get('view-chips').dispatch('click', { target: els.get('view-chips') });
check(true, 'view chips do not throw');
const frame = () => { let t = performance.now(); t += 16; if (rafCb) { const cb = rafCb; rafCb = null; cb(t); } };
for (let i = 0; i < 3; i++) frame();

console.log(failures ? `\nFAILED (${failures})` : '\nALL SMOKE TESTS PASSED');
process.exit(failures ? 1 : 0);
