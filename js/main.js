'use strict';
/* Solar System Sandbox — renderer and interface. */

(() => {
  const $ = (id) => document.getElementById(id);
  const canvas = $('view');
  const ctx = canvas.getContext('2d');

  const state = {
    sim: new Simulation(),
    cam: { x: 0, y: 0, ppAU: 46, followId: null },
    selectedId: null,
    hoverId: null,
    theme: THEMES[0],
    mouse: { x: 0, y: 0, down: false, panning: false, lastX: 0, lastY: 0, button: 0 },
    placeMode: false,
    drag: null,           // {ax,ay} world coords while dragging a spawn vector
    fps: 60,
    tab: 'inspect',
    starfield: null,
    starfieldKey: '',
  };

  /* ------------------------------------------------------------ helpers */

  const clamp = (v, a, b) => v < a ? a : v > b ? b : v;

  function hexRGB(hex) {
    const n = parseInt(hex.slice(1), 16);
    return [(n >> 16) & 255, (n >> 8) & 255, n & 255];
  }
  function shade(hex, f) {
    const [r, g, b] = hexRGB(hex);
    const c = (v) => clamp(Math.round(f > 0 ? v + (255 - v) * f : v * (1 + f)), 0, 255);
    return `rgb(${c(r)},${c(g)},${c(b)})`;
  }
  function rgba(hex, a) {
    const [r, g, b] = hexRGB(hex);
    return `rgba(${r},${g},${b},${a})`;
  }

  function jdToDate(jd) { return new Date((jd - 2440587.5) * DAY_MS); }
  const MONTHS = ['JAN','FEB','MAR','APR','MAY','JUN','JUL','AUG','SEP','OCT','NOV','DEC'];
  function fmtDate(jd) {
    const d = jdToDate(jd);
    const p = (n, w) => String(n).padStart(w, '0');
    return `${p(d.getUTCDate(),2)} ${MONTHS[d.getUTCMonth()]} ${d.getUTCFullYear()}  ${p(d.getUTCHours(),2)}:${p(d.getUTCMinutes(),2)}`;
  }

  function fmtSpeed(daysPerSec) {
    const v = daysPerSec;
    if (v < 1 / 24) return (v * 24 * 60).toFixed(0) + ' min/s';
    if (v < 1) return (v * 24).toFixed(1) + ' h/s';
    if (v < 45) return v.toFixed(2) + ' day/s';
    if (v < 365) return (v / 30.44).toFixed(1) + ' mo/s';
    return (v / 365.25).toFixed(2) + ' y/s';
  }

  function fmtPeriod(days) {
    if (!isFinite(days)) return 'escape';
    if (days < 400) return days.toFixed(days < 10 ? 2 : 1) + ' d';
    return (days / 365.25).toFixed(2) + ' y';
  }

  /* ---------------------------------------------------------- starfield */

  function buildStarfield(w, h, theme) {
    const key = `${w}x${h}:${theme.id}`;
    if (state.starfieldKey === key) return state.starfield;
    const off = document.createElement('canvas');
    off.width = w; off.height = h;
    const c = off.getContext('2d');
    // nebula washes
    for (let i = 0; i < theme.canvas.nebula.length; i++) {
      const col = theme.canvas.nebula[i];
      const cx = w * (0.18 + 0.62 * i), cy = h * (i % 2 ? 0.72 : 0.2);
      const g = c.createRadialGradient(cx, cy, 0, cx, cy, Math.max(w, h) * 0.55);
      g.addColorStop(0, col);
      g.addColorStop(1, 'rgba(0,0,0,0)');
      c.fillStyle = g;
      c.fillRect(0, 0, w, h);
    }
    // stars
    let rng = 20261018;
    const rnd = () => (rng = (rng * 1103515245 + 12345) & 0x7fffffff) / 0x7fffffff;
    for (let i = 0; i < theme.canvas.starCount; i++) {
      const x = rnd() * w, y = rnd() * h;
      const r = rnd() < 0.92 ? 0.6 + rnd() * 0.7 : 1.3 + rnd() * 1.0;
      const a = 0.18 + rnd() * (r > 1.2 ? 0.65 : 0.45);
      c.fillStyle = `rgba(${theme.canvas.star},${a})`;
      c.beginPath(); c.arc(x, y, r, 0, TAU); c.fill();
    }
    state.starfield = off;
    state.starfieldKey = key;
    return off;
  }

  /* -------------------------------------------------------- coordinates */

  function worldToScreen(x, y) {
    return [
      (x - state.cam.x) * state.cam.ppAU + canvas.clientWidth / 2,
      -(y - state.cam.y) * state.cam.ppAU + canvas.clientHeight / 2,
    ];
  }
  function screenToWorld(sx, sy) {
    return [
      (sx - canvas.clientWidth / 2) / state.cam.ppAU + state.cam.x,
      -(sy - canvas.clientHeight / 2) / state.cam.ppAU + state.cam.y,
    ];
  }

  function bodyScreenRadius(b) {
    const real = (b.radiusKm / AU_KM) * state.cam.ppAU;
    const visual = 2.1 + 3.4 * Math.log10(1 + b.radiusKm / 2000);
    if (state.theme.canvas.flat) return Math.max(real, b.kind === 'star' ? 9 : 3);
    return clamp(Math.max(real, visual), 1.6, 90);
  }

  /* ------------------------------------------------------------ picking */

  function pickBody(sx, sy) {
    let best = null, bestD = 14;
    for (const b of state.sim.bodies) {
      const [x, y] = worldToScreen(...state.sim.bodyPos(b, state.sim.jd));
      const d = Math.hypot(x - sx, y - sy);
      const r = Math.max(bodyScreenRadius(b) + 4, 9);
      if (d < Math.max(r, 9) && d < bestD + r - 9) { best = b; bestD = d; }
    }
    return best;
  }

  /* ------------------------------------------------------------- render */

  function drawOrbitFor(b) {
    const T = (state.sim.jd - J2000_JD) / 36525;
    const pal = state.theme.canvas;
    ctx.beginPath();
    if (b.parentKey) {
      const parent = state.sim.bodies.find(x => x.catalog === b.parentKey);
      if (!parent) return;
      const [px, py] = worldToScreen(...state.sim.bodyPos(parent, state.sim.jd));
      const r = b.aAU * state.cam.ppAU;
      if (r * 2 > 6) { ctx.strokeStyle = pal.moonOrbit; ctx.arc(px, py, r, 0, TAU); ctx.stroke(); }
      return;
    }
    if (!b.elemKey && !b.railKey) return;
    const el = elementsAt(PLANET_ELEMENTS[b.elemKey || b.railKey], T);
    const cw = Math.cos(el.w - el.O), sw = Math.sin(el.w - el.O);
    const cO = Math.cos(el.O), sO = Math.sin(el.O);
    const cI = Math.cos(el.I), sI = Math.sin(el.I);
    const bx = Math.sqrt(1 - el.e * el.e);
    for (let i = 0; i <= 128; i++) {
      const E = (i / 128) * TAU;
      const xp = el.a * (Math.cos(E) - el.e), yp = el.a * bx * Math.sin(E);
      const x = (cw * cO - sw * sO * cI) * xp + (-sw * cO - cw * sO * cI) * yp;
      const y = (cw * sO + sw * cO * cI) * xp + (-sw * sO + cw * cO * cI) * yp;
      const [sx, sy] = worldToScreen(x, y);
      if (i === 0) ctx.moveTo(sx, sy); else ctx.lineTo(sx, sy);
    }
    ctx.strokeStyle = b.elemKey ? pal.orbit : pal.rail;
    ctx.stroke();
  }

  function drawTrail(b) {
    const t = b.trail;
    if (!t || t.length < 6) return;
    const pal = state.theme.canvas;
    ctx.beginPath();
    let started = false;
    for (let i = 0; i < t.length; i += 2) {
      const [sx, sy] = worldToScreen(t[i], t[i + 1]);
      if (!started) { ctx.moveTo(sx, sy); started = true; } else ctx.lineTo(sx, sy);
    }
    const [lx, ly] = worldToScreen(b.state.p[0], b.state.p[1]);
    ctx.lineTo(lx, ly);
    ctx.strokeStyle = pal.trail + pal.trailAlpha + ')';
    ctx.lineWidth = 1.4;
    ctx.stroke();
    ctx.lineWidth = 1;
  }

  function drawBody(b, sx, sy, r) {
    const pal = state.theme.canvas;
    const flat = pal.flat;

    if (b.kind === 'blackhole') {
      ctx.fillStyle = '#000';
      ctx.beginPath(); ctx.arc(sx, sy, r, 0, TAU); ctx.fill();
      ctx.strokeStyle = 'rgba(255,255,255,0.7)';
      ctx.stroke();
      ctx.strokeStyle = 'rgba(120,140,255,0.35)';
      ctx.beginPath(); ctx.arc(sx, sy, r * 2.2, 0, TAU); ctx.stroke();
      return;
    }
    if (b.kind === 'star' || b.glow) {
      const gr = r * (b.kind === 'star' ? 7 : 4);
      const g = ctx.createRadialGradient(sx, sy, r * 0.3, sx, sy, gr);
      g.addColorStop(0, rgba(b.glow || '#ffe9b8', 0.85));
      g.addColorStop(1, 'rgba(255,220,150,0)');
      ctx.fillStyle = g;
      ctx.beginPath(); ctx.arc(sx, sy, gr, 0, TAU); ctx.fill();
    }
    if (b.kind === 'comet') {
      const [sunX, sunY] = worldToScreen(0, 0);
      const dx = sx - sunX, dy = sy - sunY;
      const dd = Math.hypot(dx, dy) || 1;
      const tailLen = clamp(46 * Math.pow(1.2 / (state.cam.ppAU > 0 ? dd / state.cam.ppAU : 5), 1.2), 0, 64);
      if (tailLen > 4) {
        const tx = sx + dx / dd * tailLen, ty = sy + dy / dd * tailLen;
        const g = ctx.createLinearGradient(sx, sy, tx, ty);
        g.addColorStop(0, rgba(b.color, 0.5));
        g.addColorStop(1, rgba(b.color, 0));
        ctx.strokeStyle = g;
        ctx.lineWidth = r * 1.6;
        ctx.beginPath(); ctx.moveTo(sx, sy); ctx.lineTo(tx, ty); ctx.stroke();
        ctx.lineWidth = 1;
      }
    }
    if (flat) {
      ctx.fillStyle = b.color;
      ctx.beginPath(); ctx.arc(sx, sy, r, 0, TAU); ctx.fill();
      ctx.strokeStyle = 'rgba(128,128,128,0.6)';
      ctx.stroke();
    } else {
      // day-side highlight toward the (nearest) star
      const star = nearestStar(b);
      let hx = sx - r * 0.35, hy = sy - r * 0.35;
      if (star) {
        const [stx, sty] = worldToScreen(...state.sim.bodyPos(star, state.sim.jd));
        const dx = stx - sx, dy = sty - sy, dd = Math.hypot(dx, dy) || 1;
        hx = sx + dx / dd * r * 0.45; hy = sy + dy / dd * r * 0.45;
      }
      const g = ctx.createRadialGradient(hx, hy, r * 0.1, sx, sy, r * 1.25);
      g.addColorStop(0, shade(b.color, 0.35));
      g.addColorStop(0.62, b.color);
      g.addColorStop(1, shade(b.color, -0.62));
      ctx.fillStyle = g;
      ctx.beginPath(); ctx.arc(sx, sy, r, 0, TAU); ctx.fill();
    }
    if (b.ring) {
      ctx.save();
      ctx.translate(sx, sy);
      ctx.rotate(-0.45);
      ctx.scale(1, 0.42);
      ctx.strokeStyle = flat ? 'rgba(255,255,255,0.8)' : rgba(b.color, 0.65);
      ctx.lineWidth = Math.max(1.5, r * 0.42);
      ctx.beginPath(); ctx.arc(0, 0, r * 1.75, 0, TAU); ctx.stroke();
      ctx.lineWidth = Math.max(0.8, r * 0.14);
      ctx.strokeStyle = flat ? 'rgba(0,0,0,0.6)' : 'rgba(0,0,0,0.45)';
      ctx.beginPath(); ctx.arc(0, 0, r * 1.95, 0, TAU); ctx.stroke();
      ctx.restore();
      ctx.lineWidth = 1;
    }
  }

  function nearestStar(b) {
    let best = null, bestD = Infinity;
    for (const o of state.sim.bodies) {
      if (o === b || o.kind !== 'star') continue;
      const p = state.sim.bodyPos(o, state.sim.jd);
      const q = state.sim.bodyPos(b, state.sim.jd);
      const d = (p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2;
      if (d < bestD) { bestD = d; best = o; }
    }
    return best;
  }

  function render() {
    const w = canvas.clientWidth, h = canvas.clientHeight;
    const dpr = window.devicePixelRatio || 1;
    if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(h * dpr)) {
      canvas.width = Math.round(w * dpr);
      canvas.height = Math.round(h * dpr);
    }
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
    const pal = state.theme.canvas;
    const bg = ctx.createLinearGradient(0, 0, 0, h);
    bg.addColorStop(0, pal.bgTop);
    bg.addColorStop(1, pal.bgBottom);
    ctx.fillStyle = bg;
    ctx.fillRect(0, 0, w, h);
    ctx.drawImage(buildStarfield(w, h, state.theme), 0, 0);

    const sim = state.sim;

    // follow camera
    if (state.cam.followId != null) {
      const f = sim.byId.get(state.cam.followId);
      if (f) {
        const p = sim.bodyPos(f, sim.jd);
        state.cam.x += (p[0] - state.cam.x) * 0.22;
        state.cam.y += (p[1] - state.cam.y) * 0.22;
      } else state.cam.followId = null;
    }

    // orbit rails
    for (const b of sim.bodies) drawOrbitFor(b);

    // trails
    for (const b of sim.bodies) if (b.state) drawTrail(b);

    // bodies + labels
    ctx.font = '11px "Segoe UI", system-ui, sans-serif';
    let hoverChip = null;
    for (const b of sim.bodies) {
      const p = sim.bodyPos(b, sim.jd);
      const [sx, sy] = worldToScreen(p[0], p[1]);
      const r = bodyScreenRadius(b);
      if (sx < -80 || sy < -80 || sx > w + 80 || sy > h + 80) continue;
      drawBody(b, sx, sy, r);
      const showLabel = b.kind === 'moon'
        ? r > 2.5 || state.cam.ppAU / b.aAU > 300
        : true;
      if (showLabel && (b.kind !== 'moon' || state.cam.ppAU * b.aAU < 400)) {
        ctx.fillStyle = pal.label;
        ctx.fillText(b.name, sx + r + 5, sy - r - 3);
      } else if (b.kind === 'moon') {
        ctx.fillStyle = pal.faint;
        if (r > 1.5) ctx.fillText(b.name, sx + r + 4, sy - r - 3);
      }
      if (b.id === state.hoverId) {
        hoverChip = { b, sx, sy, r, dAU: Math.hypot(p[0], p[1]) };
      }
      if (b.id === state.selectedId) {
        ctx.strokeStyle = pal.select;
        ctx.lineWidth = 1.4;
        ctx.setLineDash([4, 3]);
        ctx.beginPath(); ctx.arc(sx, sy, r + 6, 0, TAU); ctx.stroke();
        ctx.setLineDash([]);
        ctx.lineWidth = 1;
      }
    }

    // spawn drag arrow
    if (state.drag) {
      const [ax, ay] = worldToScreen(state.drag.ax, state.drag.ay);
      ctx.strokeStyle = pal.select;
      ctx.beginPath(); ctx.moveTo(ax, ay); ctx.lineTo(state.mouse.x, state.mouse.y); ctx.stroke();
      ctx.fillStyle = pal.select;
      ctx.beginPath(); ctx.arc(ax, ay, 4, 0, TAU); ctx.fill();
    }

    // vignette
    if (pal.vignette > 0) {
      const g = ctx.createRadialGradient(w / 2, h / 2, Math.min(w, h) * 0.36, w / 2, h / 2, Math.max(w, h) * 0.72);
      g.addColorStop(0, 'rgba(0,0,0,0)');
      g.addColorStop(1, `rgba(0,0,0,${pal.vignette})`);
      ctx.fillStyle = g;
      ctx.fillRect(0, 0, w, h);
    }

    // scale bar
    const niceAU = [0.01, 0.02, 0.05, 0.1, 0.2, 0.5, 1, 2, 5, 10, 20, 50];
    let barAU = niceAU[niceAU.length - 1];
    for (const c of niceAU) { if (c * state.cam.ppAU >= 64) { barAU = c; break; } }
    const barPx = barAU * state.cam.ppAU;
    ctx.strokeStyle = pal.scale;
    ctx.fillStyle = pal.scale;
    ctx.beginPath();
    ctx.moveTo(18, h - 26); ctx.lineTo(18 + barPx, h - 26);
    ctx.moveTo(18, h - 30); ctx.lineTo(18, h - 22);
    ctx.moveTo(18 + barPx, h - 30); ctx.lineTo(18 + barPx, h - 22);
    ctx.stroke();
    ctx.font = '10px Consolas, ui-monospace, monospace';
    ctx.fillText(barAU + ' AU', 18 + barPx / 2 - 12, h - 32);
    ctx.font = '11px "Segoe UI", system-ui, sans-serif';

    // hover chip in DOM (crisper text than canvas)
    const chip = $('hover-chip');
    if (hoverChip) {
      chip.style.display = 'block';
      chip.style.left = Math.min(hoverChip.sx + 14, w - 160) + 'px';
      chip.style.top = (hoverChip.sy + 14) + 'px';
      $('hover-name').textContent = hoverChip.b.name;
      $('hover-info').textContent = `${fmtNumber(hoverChip.dAU, 'AU')} from origin · ${fmtMassEarth(hoverChip.b.massKg / MEARTH_KG)}`;
      canvas.style.cursor = 'pointer';
    } else {
      chip.style.display = 'none';
      canvas.style.cursor = state.placeMode ? 'crosshair' : 'default';
    }
  }

  /* ---------------------------------------------------------- inspector */

  function selected() { return state.sim.byId.get(state.selectedId) || null; }

  function renderInspector() {
    const b = selected();
    const empty = $('insp-empty'), panel = $('insp-body');
    if (!b) {
      empty.style.display = 'block';
      panel.style.display = 'none';
      return;
    }
    empty.style.display = 'none';
    panel.style.display = 'block';
    const cat = CATALOG.find(c => c.key === b.catalog);
    $('insp-dot').style.background = b.color;
    $('insp-name').textContent = b.name;
    $('insp-kind').textContent = { star: 'Star', planet: 'Planet', moon: 'Moon', dwarf: 'Dwarf planet', asteroid: 'Asteroid', comet: 'Comet', icegiant: 'Ice giant', gasgiant: 'Gas giant', terrestrial: 'Terrestrial planet', blackhole: 'Black hole' }[b.kind] || b.kind;

    const p = state.sim.bodyPos(b, state.sim.jd);
    const distAU = Math.hypot(p[0], p[1]);
    const osc = state.sim.osculating(b);
    const dayLabel = cat && cat.extra ? (cat.extra.find(([k]) => k === 'Day length') || [])[1] : null;
    $('insp-dist').textContent = fmtNumber(distAU, 'AU');
    $('insp-period').textContent = b.periodLabel || (osc ? fmtPeriod(osc.periodDays) : '—');
    $('insp-rot').textContent = dayLabel || '—';

    const descEl = $('insp-desc');
    if (cat) {
      descEl.style.display = 'block';
      descEl.textContent = cat.desc;
      const extra = $('insp-extra');
      extra.innerHTML = '';
      for (const [k, v] of (cat.extra || [])) {
        const row = document.createElement('div');
        row.className = 'fact-row';
        const kEl = document.createElement('span');
        kEl.textContent = k;
        const vEl = document.createElement('span');
        vEl.textContent = v;
        row.appendChild(kEl); row.appendChild(vEl);
        extra.appendChild(row);
      }
    } else {
      descEl.style.display = 'none';
      $('insp-extra').innerHTML = '';
    }

    $('btn-remove').style.display = canRemove(b) ? 'block' : 'none';
    $('btn-release').style.display = state.sim.mode === 'rails' ? 'block' : 'none';
    $('btn-follow').textContent = state.cam.followId === b.id ? '◉ Following' : '👁 Follow';
  }

  function canRemove(b) {
    const stars = state.sim.bodies.filter(x => x.kind === 'star');
    if (b.kind === 'star' && stars.length === 1) return false;
    return true;
  }

  /* --------------------------------------------------------- create tab */

  const createState = { type: 'terrestrial', massEarth: 1, radiusKm: 6371, distAU: 1.5, dir: 'pro', velFactor: 1 };

  function setType(t) {
    const def = BODY_TYPES.find(x => x.id === t);
    createState.type = t;
    createState.massEarth = def.massEarth;
    createState.radiusKm = def.radiusKm;
    createState.distAU = def.dfltDist;
    syncCreateForm();
  }

  function syncCreateForm() {
    const def = BODY_TYPES.find(x => x.id === createState.type);
    document.querySelectorAll('#type-grid button').forEach(el => {
      const on = el.dataset.type === createState.type;
      el.classList.toggle('on', on);
    });
    $('c-name').value = $('c-name').dataset.touched === '1' ? $('c-name').value : '';
    setSlider('c-mass', Math.log10(createState.massEarth), -7, 7, (v) => {
      createState.massEarth = Math.pow(10, v);
      $('c-mass-out').textContent = fmtMassEarth(createState.massEarth);
    });
    setSlider('c-radius', Math.log10(createState.radiusKm), 1, 6.5, (v) => {
      createState.radiusKm = Math.pow(10, v);
      $('c-radius-out').textContent = fmtNumber(createState.radiusKm, 'km');
    });
    setSlider('c-dist', Math.log10(createState.distAU), -1.3, 2, (v) => {
      createState.distAU = Math.pow(10, v);
      $('c-dist-out').textContent = fmtNumber(createState.distAU, 'AU');
    });
    $('c-color').value = def.color;
  }

  function setSlider(id, val, min, max, fn) {
    const el = $(id);
    el.min = min; el.max = max; el.step = 0.001;
    el.value = clamp(val, min, max);
    fn(parseFloat(el.value));
  }

  function syncGravityChip() {
    const live = state.sim.mode === 'nbody';
    $('gravity-chip').textContent = live ? 'Gravity: live' : 'Gravity: rails';
    $('gravity-chip').classList.toggle('on', live);
    $('btn-gravity').classList.toggle('on', live);
  }

  function loadScenarioNamed(id, label) {
    Sfx.load();
    state.sim.loadPreset(id);
    state.presetName = label;
    state.selectedId = null;
    state.cam.followId = null;
    syncGravityChip();
    renderInspector();
  }

  function addFromForm() {
    const sim = state.sim;
    const ang = Math.random() * TAU;
    const dist = createState.distAU;
    const px = dist * Math.cos(ang), py = dist * Math.sin(ang);
    // circular velocity w.r.t. the total star mass (defaults to the Sun's)
    let mCentral = 0;
    for (const b of sim.bodies) if (b.kind === 'star') mCentral += b.massKg / MSUN_KG;
    if (mCentral === 0) mCentral = 1;
    const vCirc = Math.sqrt(K2 * mCentral / dist);
    const s = createState.dir === 'retro' ? -createState.velFactor : createState.velFactor;
    const vx = -vCirc * Math.sin(ang) * s, vy = vCirc * Math.cos(ang) * s;
    const name = $('c-name').value.trim() || undefined;
    sim.addCustom({
      name, kind: createState.type, massEarth: createState.massEarth,
      radiusKm: createState.radiusKm, color: $('c-color').value,
      ring: $('c-ring').checked,
      posAU: [px, py, 0], velAUd: [vx, vy, 0],
    });
    if (!name) $('c-name').dataset.touched = '0';
    const added = sim.bodies[sim.bodies.length - 1];
    Sfx.add();
    toast(`${added.name} added — ${fmtNumber(dist, 'AU')} orbit`);
    state.selectedId = added.id;
    switchTab('inspect');
  }

  /* ---------------------------------------------------------- library tab */

  function renderLibrary() {
    const grid = $('preset-grid');
    grid.innerHTML = '';
    for (const p of PRESETS) {
      const card = document.createElement('button');
      card.className = 'preset-card';
      const icon = document.createElement('span');
      icon.className = 'preset-icon';
      icon.textContent = p.icon;
      const name = document.createElement('strong');
      name.textContent = p.name;
      const desc = document.createElement('small');
      desc.textContent = p.desc;
      card.appendChild(icon);
      const txt = document.createElement('span');
      txt.appendChild(name); txt.appendChild(desc);
      card.appendChild(txt);
      card.addEventListener('click', () => {
        loadScenarioNamed(p.id, p.name);
        toast(`${p.name} loaded`);
        renderLibraryCurrent();
      });
      grid.appendChild(card);
    }
    renderLibraryCurrent();
  }

  function renderLibraryCurrent() {
    const el = $('library-current');
    el.textContent = `${state.presetName || 'Custom scenario'} · ${state.sim.bodies.length} bodies · ` +
      (state.sim.mode === 'rails' ? 'ephemeris rails' : 'live gravity');
  }

  function exportScenario() {
    const data = JSON.stringify(state.sim.serialize(), null, 1);
    const blob = new Blob([data], { type: 'application/json' });
    const a = document.createElement('a');
    a.href = URL.createObjectURL(blob);
    a.download = 'scenario.solar.json';
    a.click();
    URL.revokeObjectURL(a.href);
    toast('Scenario exported');
  }

  function importScenario(file) {
    const fr = new FileReader();
    fr.onload = () => {
      try {
        const data = JSON.parse(fr.result);
        if (state.sim.loadScenario(data)) {
          state.selectedId = null;
          state.cam.followId = null;
          state.presetName = 'Imported scenario';
          syncGravityChip();
          Sfx.load();
          toast(`Scenario loaded — ${state.sim.bodies.length} bodies`);
          renderLibraryCurrent();
        } else toast('Not a scenario file', 'warn');
      } catch { toast('Could not read that file', 'warn'); }
    };
    fr.readAsText(file);
  }

  /* ------------------------------------------------------- volume ui */

  function volIcons() {
    const icon = Sfx.muted ? '\u{1F507}' : '\u{1F50A}';
    $('btn-volume').textContent = icon;
    $('btn-mute').textContent = icon;
    $('btn-mute').classList.toggle('off', Sfx.muted);
    $('btn-volume').title = Sfx.muted ? 'Sound: muted (M)' : 'Sound (M)';
  }

  function setVolPop(open) {
    $('vol-pop').hidden = !open;
  }

  function initVolumeUi() {
    $('vol-slider').value = Math.round(Sfx.volume * 100);
    $('vol-out').textContent = Math.round(Sfx.volume * 100) + '%';
    $('vol-ambient').checked = Sfx.ambient;
    volIcons();

    $('btn-volume').addEventListener('click', (e) => {
      e.stopPropagation();
      Sfx.unlock();
      setVolPop($('vol-pop').hidden);
    });
    $('btn-mute').addEventListener('click', () => {
      Sfx.setMuted(!Sfx.muted);
      volIcons();
      if (!Sfx.muted) Sfx.tick();
    });
    let lastPreview = 0;
    $('vol-slider').addEventListener('input', (e) => {
      const v = parseInt(e.target.value, 10) / 100;
      Sfx.setVolume(v);
      $('vol-out').textContent = e.target.value + '%';
      const now = performance.now();
      if (now - lastPreview > 90) { lastPreview = now; Sfx.tick(); }
    });
    $('vol-ambient').addEventListener('change', (e) => {
      Sfx.setAmbient(e.target.checked);
      if (e.target.checked) Sfx.tick();
    });
    // close when clicking anywhere else
    document.addEventListener('pointerdown', (e) => {
      if ($('vol-pop').hidden) return;
      if (e.target.closest && (e.target.closest('#vol-pop') || e.target.closest('#btn-volume'))) return;
      setVolPop(false);
    });
    // first gesture anywhere unlocks the audio context
    const unlock = () => { Sfx.unlock(); };
    window.addEventListener('pointerdown', unlock, { once: true, capture: true });
    window.addEventListener('keydown', unlock, { once: true, capture: true });
  }

  /* -------------------------------------------------------------- toasts */

  function toast(text, kind) {
    const wrap = $('toasts');
    const el = document.createElement('div');
    el.className = 'toast' + (kind ? ' ' + kind : '');
    el.textContent = text;
    wrap.appendChild(el);
    requestAnimationFrame(() => el.classList.add('in'));
    setTimeout(() => {
      el.classList.remove('in');
      setTimeout(() => el.remove(), 300);
    }, 2600);
  }

  /* -------------------------------------------------------------- themes */

  function applyTheme(i) {
    state.theme = THEMES[i % THEMES.length];
    document.documentElement.dataset.theme = state.theme.id;
    state.starfieldKey = '';
    try { localStorage.setItem('sss-theme', state.theme.id); } catch {}
    $('theme-label').textContent = state.theme.name;
  }
  function cycleTheme() {
    const i = THEMES.findIndex(t => t.id === state.theme.id);
    applyTheme(i + 1);
    Sfx.theme();
    toast(`Theme: ${state.theme.name}`);
  }

  /* --------------------------------------------------------------- tabs */

  function switchTab(t) {
    state.tab = t;
    for (const id of ['inspect', 'create', 'library']) {
      $('tab-' + id).classList.toggle('on', id === t);
      $('panel-' + id).style.display = id === t ? 'block' : 'none';
    }
    if (t === 'library') renderLibrary();
  }

  /* ------------------------------------------------------------- events */

  function bindEvents() {
    window.addEventListener('resize', () => { state.starfieldKey = ''; });

    canvas.addEventListener('mousedown', (e) => {
      const r = canvas.getBoundingClientRect();
      const mx = e.clientX - r.left, my = e.clientY - r.top;
      state.mouse.x = mx; state.mouse.y = my;
      if (state.placeMode && e.button === 0) {
        const [wx, wy] = screenToWorld(mx, my);
        state.drag = { ax: wx, ay: wy };
        return;
      }
      state.mouse.down = true;
      state.mouse.panning = true;
      state.mouse.lastX = e.clientX; state.mouse.lastY = e.clientY;
      state.mouse.button = e.button;
    });
    window.addEventListener('mousemove', (e) => {
      const r = canvas.getBoundingClientRect();
      state.mouse.x = e.clientX - r.left;
      state.mouse.y = e.clientY - r.top;
      if (state.mouse.panning && !state.drag) {
        state.cam.x -= (e.clientX - state.mouse.lastX) / state.cam.ppAU;
        state.cam.y += (e.clientY - state.mouse.lastY) / state.cam.ppAU;
        state.cam.followId = null;
        state.mouse.lastX = e.clientX; state.mouse.lastY = e.clientY;
      }
    });
    window.addEventListener('mouseup', (e) => {
      if (state.drag) {
        const [wx, wy] = screenToWorld(state.mouse.x, state.mouse.y);
        let dvx = (wx - state.drag.ax), dvy = (wy - state.drag.ay);
        const dLen = Math.hypot(dvx, dvy);
        const def = BODY_TYPES.find(x => x.id === createState.type);
        let mCentral = 0;
        for (const b of state.sim.bodies) if (b.kind === 'star') mCentral += b.massKg / MSUN_KG;
        if (mCentral === 0) mCentral = 1;
        const vRef = Math.sqrt(K2 * mCentral / Math.max(0.05, Math.hypot(state.drag.ax, state.drag.ay)));
        // drag across the distance to the star = circular velocity there
        const gain = dLen > 1e-6 ? 1 : 0;
        let vx = dLen > 1e-6 ? dvx * vRef : vRef;
        let vy = dLen > 1e-6 ? dvy * vRef : 0;
        if (dLen > 1e-6) { vx = dvx * vRef; vy = dvy * vRef; }
        state.sim.addCustom({
          kind: createState.type, massEarth: createState.massEarth,
          radiusKm: createState.radiusKm, color: $('c-color').value,
          name: $('c-name').value.trim() || undefined,
          posAU: [state.drag.ax, state.drag.ay, 0], velAUd: [vx, vy, 0],
        });
        Sfx.add();
        toast('Body placed by drag');
        state.drag = null;
        setPlaceMode(false);
        return;
      }
      if (state.mouse.panning && state.mouse.button === 0) {
        const moved = Math.hypot(e.clientX - state.mouse.lastX, e.clientY - state.mouse.lastY);
        if (moved < 4) {
          const r = canvas.getBoundingClientRect();
          const hit = pickBody(e.clientX - r.left, e.clientY - r.top);
          if (hit) {
            state.selectedId = hit.id;
            Sfx.select();
            switchTab('inspect');
          } else state.selectedId = null;
          renderInspector();
        }
      }
      state.mouse.panning = false;
      state.mouse.down = false;
    });
    canvas.addEventListener('dblclick', (e) => {
      const r = canvas.getBoundingClientRect();
      const hit = pickBody(e.clientX - r.left, e.clientY - r.top);
      if (hit) {
        state.selectedId = hit.id;
        state.cam.followId = hit.id;
        renderInspector();
      }
    });
    canvas.addEventListener('wheel', (e) => {
      e.preventDefault();
      const r = canvas.getBoundingClientRect();
      const mx = e.clientX - r.left, my = e.clientY - r.top;
      const [wx, wy] = screenToWorld(mx, my);
      const factor = Math.pow(1.0016, -e.deltaY);
      state.cam.ppAU = clamp(state.cam.ppAU * factor, 2, 40000);
      // keep the point under the cursor fixed
      const [wx2, wy2] = screenToWorld(mx, my);
      state.cam.x += wx - wx2;
      state.cam.y += wy - wy2;
    }, { passive: false });

    canvas.addEventListener('mousemove', () => {
      const hit = pickBody(state.mouse.x, state.mouse.y);
      state.hoverId = hit ? hit.id : null;
    });

    // transport
    $('btn-play').addEventListener('click', togglePlay);
    $('btn-now').addEventListener('click', () => { state.sim.jd = nowJD(); toast('Epoch set to now'); });
    $('speed').addEventListener('input', (e) => {
      state.sim.speed = Math.pow(10, e.target.value / 1000 * 5 - 2);
      $('speed-out').textContent = fmtSpeed(state.sim.speed);
    });
    $('scenario').addEventListener('change', (e) => {
      loadScenarioNamed(e.target.value, e.target.selectedOptions[0].text);
      toast(`${e.target.selectedOptions[0].text} loaded`);
    });
    $('btn-gravity').addEventListener('click', () => {
      state.sim.releaseToNBody();
      syncGravityChip();
      Sfx.gravity();
      toast('Gravity released');
    });
    $('btn-theme').addEventListener('click', cycleTheme);
    $('btn-help').addEventListener('click', () => $('help').classList.add('open'));
    $('help-close').addEventListener('click', () => $('help').classList.remove('open'));

    // inspector
    $('tab-inspect').addEventListener('click', () => switchTab('inspect'));
    $('tab-create').addEventListener('click', () => switchTab('create'));
    $('tab-library').addEventListener('click', () => switchTab('library'));
    $('btn-focus').addEventListener('click', () => {
      const b = selected();
      if (b) {
        const p = state.sim.bodyPos(b, state.sim.jd);
        state.cam.x = p[0]; state.cam.y = p[1];
        state.cam.followId = b.id;
      }
    });
    $('btn-follow').addEventListener('click', () => {
      const b = selected();
      if (!b) return;
      state.cam.followId = state.cam.followId === b.id ? null : b.id;
      renderInspector();
    });
    $('btn-release').addEventListener('click', () => {
      state.sim.releaseToNBody();
      syncGravityChip();
      Sfx.gravity();
      renderInspector();
    });
    $('btn-remove').addEventListener('click', () => {
      if (state.sim.removeBody(state.selectedId)) {
        Sfx.remove();
        state.selectedId = null;
        if (state.cam.followId === state.selectedId) state.cam.followId = null;
        renderInspector();
      }
    });

    // create
    $('type-grid').addEventListener('click', (e) => {
      const btn = e.target.closest('button[data-type]');
      if (btn) setType(btn.dataset.type);
    });
    $('c-mass').addEventListener('input', (e) => {
      createState.massEarth = Math.pow(10, parseFloat(e.target.value));
      $('c-mass-out').textContent = fmtMassEarth(createState.massEarth);
    });
    $('c-radius').addEventListener('input', (e) => {
      createState.radiusKm = Math.pow(10, parseFloat(e.target.value));
      $('c-radius-out').textContent = fmtNumber(createState.radiusKm, 'km');
    });
    $('c-dist').addEventListener('input', (e) => {
      createState.distAU = Math.pow(10, parseFloat(e.target.value));
      $('c-dist-out').textContent = fmtNumber(createState.distAU, 'AU');
    });
    $('c-dir').addEventListener('change', (e) => { createState.dir = e.target.value; });
    $('c-vel').addEventListener('change', (e) => { createState.velFactor = parseFloat(e.target.value); });
    $('c-name').addEventListener('input', () => { $('c-name').dataset.touched = '1'; });
    $('btn-add').addEventListener('click', addFromForm);
    $('btn-place').addEventListener('click', () => setPlaceMode(!state.placeMode));

    // parts templates
    const parts = $('parts-list');
    parts.innerHTML = '';
    for (const part of PARTS) {
      const chip = document.createElement('button');
      chip.className = 'part-chip';
      const dot = document.createElement('span');
      dot.className = 'dot';
      dot.style.background = part.color;
      const nm = document.createElement('span');
      nm.textContent = part.name;
      chip.appendChild(dot); chip.appendChild(nm);
      chip.title = part.desc;
      chip.addEventListener('click', () => {
        const t = BODY_TYPES.find(x => x.id === part.type);
        createState.type = part.type;
        createState.massEarth = part.massEarth;
        createState.radiusKm = part.radiusKm;
        createState.distAU = t.dfltDist;
        syncCreateForm();
        $('c-name').value = part.name;
        $('c-name').dataset.touched = '1';
        $('c-color').value = part.color;
        $('c-ring').checked = !!part.ring;
        toast(`${part.name} template loaded — press Add`);
      });
      parts.appendChild(chip);
    }

    // library
    $('btn-export').addEventListener('click', exportScenario);
    $('btn-import').addEventListener('click', () => $('file-import').click());
    $('file-import').addEventListener('change', (e) => {
      if (e.target.files.length) importScenario(e.target.files[0]);
      e.target.value = '';
    });

    // keyboard
    window.addEventListener('keydown', (e) => {
      if (e.target.tagName === 'INPUT' || e.target.tagName === 'SELECT' || e.target.tagName === 'TEXTAREA') return;
      switch (e.key) {
        case ' ': togglePlay(); e.preventDefault(); break;
        case 'f': case 'F': $('btn-focus').click(); break;
        case 't': case 'T': case 'F3': cycleTheme(); e.preventDefault(); break;
        case 'c': case 'C': switchTab('create'); break;
        case 'm': case 'M':
          Sfx.setMuted(!Sfx.muted);
          volIcons();
          if (!Sfx.muted) Sfx.tick();
          break;
        case 'l': case 'L': switchTab('library'); break;
        case 'Escape': $('help').classList.remove('open'); setVolPop(false); state.selectedId = null; renderInspector(); break;
        case 'Delete': case 'Backspace': if (selected() && canRemove(selected())) $('btn-remove').click(); break;
        case '+': case '=': state.cam.ppAU = clamp(state.cam.ppAU * 1.25, 2, 40000); break;
        case '-': state.cam.ppAU = clamp(state.cam.ppAU / 1.25, 2, 40000); break;
      }
    });
  }

  function setPlaceMode(on) {
    state.placeMode = on;
    $('btn-place').classList.toggle('on', on);
    $('btn-place').textContent = on ? '✕ Cancel placing' : '✛ Place by drag';
  }

  function togglePlay() {
    state.sim.paused = !state.sim.paused;
    $('btn-play').textContent = state.sim.paused ? '▶' : '❚❚';
    $('btn-play').classList.toggle('paused', state.sim.paused);
    $('status-run').textContent = state.sim.paused ? 'paused' : 'running';
  }

  /* --------------------------------------------------------------- loop */

  let lastT = performance.now();

  function frame(t) {
    const dt = Math.min(0.1, (t - lastT) / 1000);
    lastT = t;
    state.fps += ((dt > 0 ? 1 / dt : 60) - state.fps) * 0.05;

    state.sim.advance(dt);
    render();
    updateHud();

    for (const ev of state.sim.drainEvents()) {
      if (ev.kind === 'merge') Sfx.merge();
      toast(ev.text, ev.kind === 'merge' ? 'warn' : undefined);
    }
    requestAnimationFrame(frame);
  }

  let hudClock = 0;
  function updateHud() {
    hudClock++;
    if (hudClock % 6 === 0) {
      $('epoch').textContent = fmtDate(state.sim.jd);
      const dyn = state.sim.bodies.filter(b => b.state).length;
      $('status-count').textContent = `${state.sim.bodies.length} bodies`;
      $('status-mode').textContent = state.sim.mode === 'rails' ? 'ephemeris rails' : 'live gravity';
      if (state.tab === 'inspect') renderInspector();
      if (state.tab === 'library') renderLibraryCurrent();
    }
    $('status-fps').textContent = Math.round(state.fps) + ' fps';
    $('status-size').textContent = `${canvas.clientWidth} × ${canvas.clientHeight}`;
  }

  /* --------------------------------------------------------------- boot */

  function boot() {
    // restore theme
    let saved = null;
    try { saved = localStorage.getItem('sss-theme'); } catch {}
    const idx = Math.max(0, THEMES.findIndex(t => t.id === saved));
    applyTheme(idx);

    // scenario select
    const sel = $('scenario');
    sel.innerHTML = '';
    for (const p of PRESETS) {
      const o = document.createElement('option');
      o.value = p.id; o.textContent = p.name;
      sel.appendChild(o);
    }
    sel.value = 'solar';
    state.sim.loadPreset('solar');

    // type grid
    const grid = $('type-grid');
    grid.innerHTML = '';
    for (const t of BODY_TYPES) {
      const b = document.createElement('button');
      b.type = 'button';
      b.dataset.type = t.id;
      b.textContent = t.label;
      grid.appendChild(b);
    }
    setType('terrestrial');
    state.sim.speed = 1;
    $('speed').value = 600; // 10^((600/1000)*5-2) = 1 day/s
    $('speed-out').textContent = fmtSpeed(1);

    bindEvents();
    initVolumeUi();
    switchTab('inspect');
    requestAnimationFrame(frame);

    // debug/testing hook
    window.__sss = { sim: state.sim, state, addFromForm, setType, exportScenario };
  }

  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', boot);
  else boot();
})();
