'use strict';
/* Solar System Sandbox — procedural sound effects (Web Audio API).
 *
 * Every sound is synthesized from oscillators and shaped noise: there are no
 * audio files to load, the whole soundscape is a few KB of code.  A master
 * gain node carries the volume slider and mute switch; an optional ambient
 * drone adds a barely-there space hum.  Browsers only allow audio after a
 * user gesture, so the context is created/resumed lazily on first
 * pointerdown/keydown — before that, every call is a safe no-op.
 */

const Sfx = (() => {
  const VOL_KEY = 'sss-volume', MUTE_KEY = 'sss-muted', AMB_KEY = 'sss-ambient';
  const store = {
    get(k) { try { return localStorage.getItem(k); } catch { return null; } },
    set(k, v) { try { localStorage.setItem(k, v); } catch {} },
  };

  let ctx = null;
  let master = null;
  let ambient = null;          // { oscs: [...], lfo } while playing
  let volume = Math.min(1, Math.max(0, parseFloat(store.get(VOL_KEY) ?? '0.7') || 0.7));
  let muted = store.get(MUTE_KEY) === '1';
  let ambientOn = store.get(AMB_KEY) === '1';
  const lastPlayed = new Map(); // sound name -> time of last play (throttle)

  function ensure() {
    if (!ctx) {
      const AC = window.AudioContext || window.webkitAudioContext;
      if (!AC) return null;
      try { ctx = new AC(); } catch { return null; }
      master = ctx.createGain();
      master.gain.value = muted ? 0 : volume;
      master.connect(ctx.destination);
      if (ambientOn) startAmbient();
    }
    if (ctx.state === 'suspended') ctx.resume();
    return ctx;
  }

  function throttled(name, ms) {
    const now = performance.now();
    if (now - (lastPlayed.get(name) || 0) < ms) return true;
    lastPlayed.set(name, now);
    return false;
  }

  /* One enveloped oscillator sweep. */
  function tone({ type = 'sine', from = 440, to = 0, dur = 0.15, peak = 0.4, delay = 0 }) {
    const c = ensure();
    if (!c) return;
    const t0 = c.currentTime + delay;
    const osc = c.createOscillator();
    const g = c.createGain();
    osc.type = type;
    osc.frequency.setValueAtTime(Math.max(from, 1), t0);
    if (to) osc.frequency.exponentialRampToValueAtTime(Math.max(to, 1), t0 + dur);
    g.gain.setValueAtTime(0.0001, t0);
    g.gain.exponentialRampToValueAtTime(Math.max(peak * 0.5, 0.0001), t0 + 0.012);
    g.gain.exponentialRampToValueAtTime(0.0001, t0 + dur);
    osc.connect(g);
    g.connect(master);
    osc.start(t0);
    osc.stop(t0 + dur + 0.05);
  }

  /* A burst of shaped white noise through a sweeping filter. */
  function noise({ dur = 0.3, peak = 0.35, from = 1000, to = 200, type = 'lowpass', delay = 0 }) {
    const c = ensure();
    if (!c) return;
    const t0 = c.currentTime + delay;
    const len = Math.max(1, Math.floor((dur + 0.05) * c.sampleRate));
    const buf = c.createBuffer(1, len, c.sampleRate);
    const d = buf.getChannelData(0);
    for (let i = 0; i < len; i++) d[i] = Math.random() * 2 - 1;
    const src = c.createBufferSource();
    src.buffer = buf;
    const filt = c.createBiquadFilter();
    filt.type = type;
    filt.frequency.setValueAtTime(Math.max(from, 10), t0);
    filt.frequency.exponentialRampToValueAtTime(Math.max(to, 10), t0 + dur);
    const g = c.createGain();
    g.gain.setValueAtTime(0.0001, t0);
    g.gain.exponentialRampToValueAtTime(Math.max(peak * 0.4, 0.0001), t0 + 0.02);
    g.gain.exponentialRampToValueAtTime(0.0001, t0 + dur);
    src.connect(filt);
    filt.connect(g);
    g.connect(master);
    src.start(t0);
    src.stop(t0 + dur + 0.1);
  }

  /* ------------------------------------------------------------- ambient */

  function startAmbient() {
    const c = ensure();
    if (!c || ambient) return;
    const g = c.createGain();
    g.gain.value = 0.05;
    const filt = c.createBiquadFilter();
    filt.type = 'lowpass';
    filt.frequency.value = 220;
    const oscs = [55, 82.41, 110].map((f, i) => {
      const o = c.createOscillator();
      o.type = 'sine';
      o.frequency.value = f;
      o.detune.value = i * 4 - 4;
      o.connect(filt);
      o.start();
      return o;
    });
    // slow swell so the hum breathes instead of sitting flat
    const lfo = c.createOscillator();
    const lfoGain = c.createGain();
    lfo.frequency.value = 0.05;
    lfoGain.gain.value = 0.02;
    lfo.connect(lfoGain);
    lfoGain.connect(g.gain);
    lfo.start();
    filt.connect(g);
    g.connect(master);
    ambient = { oscs, lfo, g };
  }

  function stopAmbient() {
    if (!ambient) return;
    const t = ctx.currentTime;
    ambient.g.gain.setTargetAtTime(0.0001, t, 0.2);
    const dying = ambient;
    setTimeout(() => {
      for (const o of dying.oscs) { try { o.stop(); } catch {} }
      try { dying.lfo.stop(); } catch {}
    }, 800);
    ambient = null;
  }

  /* ---------------------------------------------------------------- api */

  return {
    /* individual effects */
    select() { if (!throttled('select', 60)) tone({ from: 660, to: 920, dur: 0.06, peak: 0.22 }); },
    add() {
      tone({ from: 300, to: 620, dur: 0.13, peak: 0.5 });
      tone({ type: 'triangle', from: 620, to: 980, dur: 0.1, peak: 0.18, delay: 0.06 });
    },
    remove() { tone({ from: 520, to: 170, dur: 0.22, peak: 0.4 }); },
    merge() {
      if (throttled('merge', 120)) return;
      tone({ from: 140, to: 42, dur: 0.5, peak: 0.9 });
      noise({ dur: 0.32, from: 900, to: 110, peak: 0.45 });
    },
    gravity() {
      noise({ dur: 0.7, from: 260, to: 2600, type: 'bandpass', peak: 0.4 });
      tone({ from: 170, to: 430, dur: 0.6, peak: 0.22 });
    },
    load() {
      tone({ from: 392, dur: 0.16, peak: 0.34 });
      tone({ from: 523.25, dur: 0.18, peak: 0.34, delay: 0.09 });
      tone({ from: 659.25, dur: 0.24, peak: 0.28, delay: 0.18 });
    },
    theme() { tone({ type: 'triangle', from: 520, to: 800, dur: 0.09, peak: 0.26 }); },
    tick() { if (!throttled('tick', 45)) tone({ from: 620, dur: 0.035, peak: 0.14 }); },

    /* lifecycle */
    unlock() { ensure(); },

    setVolume(v) {
      volume = Math.min(1, Math.max(0, +v || 0));
      store.set(VOL_KEY, String(volume));
      if (master && ctx) master.gain.setTargetAtTime(muted ? 0 : volume, ctx.currentTime, 0.02);
    },
    setMuted(m) {
      muted = !!m;
      store.set(MUTE_KEY, muted ? '1' : '0');
      if (master && ctx) master.gain.setTargetAtTime(muted ? 0 : volume, ctx.currentTime, 0.02);
    },
    setAmbient(on) {
      ambientOn = !!on;
      store.set(AMB_KEY, ambientOn ? '1' : '0');
      if (ambientOn) startAmbient();
      else stopAmbient();
    },

    get volume() { return volume; },
    get muted() { return muted; },
    get ambient() { return ambientOn; },
  };
})();
