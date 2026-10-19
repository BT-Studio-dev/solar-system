'use strict';
/* Solar System Sandbox — static data.
 *
 * Planetary positions come from JPL's "Approximate Positions of the Major
 * Planets" — Keplerian elements referenced to the J2000 ecliptic with linear
 * rates per Julian century (valid 1800–2050 AD).  Moons use simple circular
 * orbits around their parent; they are decoration at solar-system zoom and
 * honest enough when you follow a planet.
 */

const J2000_JD = 2451545.0;
const DAY_MS = 86400000;
const MSUN_KG = 1.98892e30;
const MEARTH_KG = 5.9722e24;
const MSUN_IN_MEARTH = MSUN_KG / MEARTH_KG;
const AU_KM = 1.495978707e8;
const K2 = 2.959122082855911e-4;   // (Gaussian gravitational constant)^2, AU^3/day^2/Msun

/* ------------------------------------------------- JPL Keplerian elements -- */
/* a [AU, AU/cy], e, I [deg], L [deg], varpi (longitude of perihelion), Omega
 * (longitude of ascending node). */
const PLANET_ELEMENTS = {
  mercury: { a: [0.38709927, 0.00000037], e: [0.20563593, 0.00001906], I: [7.00497902, -0.00594749], L: [252.25032350, 149472.67411175], w: [77.45779628, 0.16047689], O: [48.33076593, -0.12534081] },
  venus:   { a: [0.72333566, 0.00000390], e: [0.00677672, -0.00004107], I: [3.39467605, -0.00078890], L: [181.97909950, 58517.81538729], w: [131.60246718, 0.00268329], O: [76.67984255, -0.27769418] },
  earth:   { a: [1.00000261, 0.00000562], e: [0.01671123, -0.00004392], I: [-0.00001531, -0.01294668], L: [100.46457166, 35999.37244981], w: [102.93768193, 0.32327364], O: [0.0, 0.0] },
  mars:    { a: [1.52371034, 0.00001847], e: [0.09339410, 0.00007882], I: [1.84969142, -0.00813131], L: [-4.55343205, 19140.30268499], w: [-23.94362959, 0.44441088], O: [49.55953891, -0.29257343] },
  jupiter: { a: [5.20288700, -0.00011607], e: [0.04838624, -0.00013253], I: [1.30439695, -0.00183714], L: [34.39644051, 3034.74612775], w: [14.72847983, 0.21252668], O: [100.47390909, 0.20469106] },
  saturn:  { a: [9.53667594, -0.00125060], e: [0.05386179, -0.00050991], I: [2.48599187, 0.00193609], L: [49.95424423, 1222.49362201], w: [92.59887831, -0.41897216], O: [113.66242448, -0.28867794] },
  uranus:  { a: [19.18916464, -0.00196176], e: [0.04725744, -0.00004397], I: [0.77263783, -0.00242939], L: [313.23810451, 428.48202785], w: [170.95427630, 0.40805281], O: [74.01692503, 0.04240589] },
  neptune: { a: [30.06992276, 0.00026291], e: [0.00859048, 0.00005105], I: [1.77004347, 0.00035372], L: [-55.12002969, 218.45945325], w: [44.96476227, -0.32241464], O: [131.78422574, -0.00508664] },
  pluto:   { a: [39.48211675, -0.00031596], e: [0.24882730, 0.00005170], I: [17.14001206, 0.00004818], L: [238.92903833, 145.20780515], w: [224.06891629, -0.04062942], O: [110.30393684, -0.01183482] },
};

/* ------------------------------------------------------------ body facts -- */

const CATALOG = [
  {
    key: 'sun', name: 'Sun', kind: 'star',
    radiusKm: 696000, massKg: MSUN_KG, color: '#ffcf6e', glow: '#ffe9b8',
    desc: 'A G2V main-sequence star holding 99.86% of the mass of the system. Every orbit below is a compromise with its gravity.',
    extra: [['Spectral type', 'G2V'], ['Surface', '5,772 K'], ['Age', '4.6 billion years']],
  },
  {
    key: 'mercury', name: 'Mercury', kind: 'planet', elem: 'mercury',
    radiusKm: 2439.7, massKg: 3.3011e23, color: '#a89e93', period: '88.0 d',
    desc: 'A cratered iron ball on the fastest orbit of all — the Roman messenger god, appropriately.',
    extra: [['Day length', '58.6 d'], ['Moons', '0'], ['Atmosphere', 'trace']],
  },
  {
    key: 'venus', name: 'Venus', kind: 'planet', elem: 'venus',
    radiusKm: 6051.8, massKg: 4.8675e24, color: '#e6c68a', period: '224.7 d',
    desc: 'Earth\'s evil twin: same size, runaway greenhouse, 92-bar CO2 air and sulphuric-acid clouds.',
    extra: [['Day length', '243 d (retro)'], ['Moons', '0'], ['Surface', '464 °C']],
  },
  {
    key: 'earth', name: 'Earth', kind: 'planet', elem: 'earth',
    radiusKm: 6371, massKg: 5.9722e24, color: '#5f9fe8', period: '1.00 y',
    desc: 'The only world known to carry liquid-water oceans — and the people reading this page.',
    extra: [['Day length', '23 h 56 m'], ['Moons', '1'], ['Atmosphere', 'N2 / O2']],
  },
  {
    key: 'moon', name: 'Moon', kind: 'moon', parent: 'earth',
    aAU: 0.002570, periodDays: 27.321661, e: 0.0549, phase0: 1.35,
    radiusKm: 1737.4, massKg: 7.342e22, color: '#c9d2dc', period: '27.3 d',
    desc: 'Earth\'s companion, locked with one face turned home. Its tugs raise the ocean tides.',
    extra: [['Diameter', '3,475 km'], ['Orbit', '384,400 km']],
  },
  {
    key: 'mars', name: 'Mars', kind: 'planet', elem: 'mars',
    radiusKm: 3389.5, massKg: 6.4171e23, color: '#d97e56', period: '1.88 y',
    desc: 'A cold desert world with the solar system\'s tallest volcano and a buried ocean\'s worth of ice.',
    extra: [['Day length', '24 h 37 m'], ['Moons', '2'], ['Gravity', '0.38 g']],
  },
  {
    key: 'jupiter', name: 'Jupiter', kind: 'planet', elem: 'jupiter',
    radiusKm: 69911, massKg: 1.8982e27, color: '#d9b088', period: '11.86 y',
    desc: 'More massive than every other planet combined; its moons are a miniature planetary system.',
    extra: [['Day length', '9 h 56 m'], ['Moons', '95 known'], ['Great Red Spot', 'rages 350+ yrs']],
  },
  {
    key: 'io', name: 'Io', kind: 'moon', parent: 'jupiter',
    aAU: 0.002819, periodDays: 1.769138, e: 0.004, phase0: 0.6,
    radiusKm: 1821.6, massKg: 8.9319e22, color: '#ddc86a', period: '1.77 d',
    desc: 'The most volcanic body known, kneaded by Jupiter\'s tides.',
  },
  {
    key: 'europa', name: 'Europa', kind: 'moon', parent: 'jupiter',
    aAU: 0.004486, periodDays: 3.551181, e: 0.009, phase0: 2.4,
    radiusKm: 1560.8, massKg: 4.7998e22, color: '#cfd8d6', period: '3.55 d',
    desc: 'An ice shell over a global salt-water ocean — a prime candidate for life.',
  },
  {
    key: 'ganymede', name: 'Ganymede', kind: 'moon', parent: 'jupiter',
    aAU: 0.007155, periodDays: 7.154553, e: 0.001, phase0: 4.1,
    radiusKm: 2634.1, massKg: 1.4819e23, color: '#b7a88f', period: '7.15 d',
    desc: 'The largest moon in the solar system — bigger than Mercury, with its own magnetic field.',
  },
  {
    key: 'callisto', name: 'Callisto', kind: 'moon', parent: 'jupiter',
    aAU: 0.012586, periodDays: 16.689017, e: 0.007, phase0: 5.5,
    radiusKm: 2410.3, massKg: 1.0759e23, color: '#8f8674', period: '16.69 d',
    desc: 'The most heavily cratered surface known; a quiet witness to four billion years.',
  },
  {
    key: 'saturn', name: 'Saturn', kind: 'planet', elem: 'saturn', ring: true,
    radiusKm: 58232, massKg: 5.6834e26, color: '#e0cb96', period: '29.45 y',
    desc: 'The ringed giant: a hydrogen world lighter than water, wearing a halo of ice.',
    extra: [['Day length', '10 h 33 m'], ['Moons', '146 known'], ['Rings', '280,000 km wide']],
  },
  {
    key: 'titan', name: 'Titan', kind: 'moon', parent: 'saturn',
    aAU: 0.008168, periodDays: 15.945421, e: 0.029, phase0: 1.9,
    radiusKm: 2574.7, massKg: 1.3452e23, color: '#d8b268', period: '15.95 d',
    desc: 'A moon with rivers and seas — of liquid methane under an orange nitrogen sky.',
  },
  {
    key: 'uranus', name: 'Uranus', kind: 'planet', elem: 'uranus',
    radiusKm: 25362, massKg: 8.6810e25, color: '#9ed6da', period: '84.02 y',
    desc: 'The tipped-over ice giant: it rolls around the Sun on its side, seasons forty years long.',
    extra: [['Axial tilt', '97.8°'], ['Moons', '28 known']],
  },
  {
    key: 'neptune', name: 'Neptune', kind: 'planet', elem: 'neptune',
    radiusKm: 24622, massKg: 1.02413e26, color: '#6f8fe8', period: '164.8 y',
    desc: 'The windiest world — supersonic storms in a deep blue methane haze, found by mathematics before telescopes.',
    extra: [['Winds', '2,100 km/h'], ['Moons', '16 known']],
  },
  {
    key: 'triton', name: 'Triton', kind: 'moon', parent: 'neptune',
    aAU: 0.002371, periodDays: -5.876854, e: 0.000, phase0: 3.0,
    radiusKm: 1353.4, massKg: 2.1390e22, color: '#ccd8e6', period: '5.88 d (retro)',
    desc: 'Neptune\'s great moon orbits backwards — a captured Kuiper Belt world with nitrogen geysers.',
  },
  {
    key: 'pluto', name: 'Pluto', kind: 'dwarf', elem: 'pluto',
    radiusKm: 1188.3, massKg: 1.303e22, color: '#cfc9bd', period: '248.0 y',
    desc: 'A dwarf planet with a heart of nitrogen ice. Planet, memorialised by a state legislature.',
    extra: [['Discovered', '1930'], ['Moons', '5']],
  },
];

/* --------------------------------------------------------------- themes -- */
/* Each theme drives both the CSS (via [data-theme]) and the canvas palette. */

const THEMES = [
  {
    id: 'aero-night', name: 'Aero Night', desc: 'Windows 7 glass over deep space',
    canvas: {
      bgTop: '#060911', bgBottom: '#0d1626', vignette: 0.55,
      nebula: ['rgba(52,32,96,0.30)', 'rgba(24,52,110,0.26)'],
      star: '207,226,255', starCount: 240,
      orbit: 'rgba(132,178,255,0.30)', rail: 'rgba(132,178,255,0.16)',
      moonOrbit: 'rgba(150,190,240,0.35)', label: '#cfe0f8', faint: 'rgba(207,224,248,0.62)',
      trail: 'rgba(140,196,255,', trailAlpha: 0.55, hover: '#8cc4ff', select: '#8cc4ff',
      scale: 'rgba(180,205,240,0.8)',
    },
  },
  {
    id: 'aero-day', name: 'Aero Day', desc: 'light glass, blueprint chart',
    canvas: {
      bgTop: '#eef4fc', bgBottom: '#ccd9ee', vignette: 0,
      nebula: [], star: '60,90,150', starCount: 0,
      orbit: 'rgba(28,64,128,0.40)', rail: 'rgba(28,64,128,0.18)',
      moonOrbit: 'rgba(28,64,128,0.35)', label: '#1c2c48', faint: 'rgba(28,44,72,0.65)',
      trail: 'rgba(34,86,164,', trailAlpha: 0.6, hover: '#2256a4', select: '#2256a4',
      scale: 'rgba(20,40,80,0.75)', flat: true,
    },
  },
  {
    id: 'luna', name: 'Luna Blue', desc: 'XP-era blue chrome, cream panels',
    canvas: {
      bgTop: '#0a1c3c', bgBottom: '#123a6e', vignette: 0.4,
      nebula: ['rgba(40,80,160,0.28)'],
      star: '235,240,250', starCount: 200,
      orbit: 'rgba(190,214,255,0.32)', rail: 'rgba(190,214,255,0.15)',
      moonOrbit: 'rgba(200,220,255,0.35)', label: '#f4f0e2', faint: 'rgba(244,240,226,0.6)',
      trail: 'rgba(255,220,140,', trailAlpha: 0.5, hover: '#ffd982', select: '#ffd982',
      scale: 'rgba(240,236,220,0.85)',
    },
  },
  {
    id: 'aurora', name: 'Aurora', desc: 'teal and violet over polar night',
    canvas: {
      bgTop: '#03151a', bgBottom: '#0a2430', vignette: 0.5,
      nebula: ['rgba(28,120,110,0.30)', 'rgba(90,48,140,0.26)'],
      star: '200,255,240', starCount: 260,
      orbit: 'rgba(110,240,210,0.28)', rail: 'rgba(110,240,210,0.13)',
      moonOrbit: 'rgba(140,250,220,0.35)', label: '#dcf8f0', faint: 'rgba(220,248,240,0.6)',
      trail: 'rgba(62,230,192,', trailAlpha: 0.55, hover: '#3ee6c0', select: '#3ee6c0',
      scale: 'rgba(190,245,230,0.8)',
    },
  },
  {
    id: 'high-contrast', name: 'High Contrast', desc: 'black, white and yellow',
    canvas: {
      bgTop: '#000000', bgBottom: '#000000', vignette: 0,
      nebula: [], star: '255,255,255', starCount: 160,
      orbit: 'rgba(255,255,255,0.55)', rail: 'rgba(255,255,255,0.30)',
      moonOrbit: 'rgba(255,255,255,0.5)', label: '#ffffff', faint: 'rgba(255,255,255,0.75)',
      trail: 'rgba(255,230,0,', trailAlpha: 0.8, hover: '#ffe600', select: '#ffe600',
      scale: 'rgba(255,255,255,0.9)', flat: true,
    },
  },
];

/* ------------------------------------------------- texture pack recolours -- */
/* Tint + strength, mirroring the desktop app's pack.ini format. */

const PACKS = [
  { id: 'theme', name: 'Theme colours', desc: 'no recolour', tint: null, strength: 0 },
  { id: 'deep-blue', name: 'Deep Blue', desc: 'cold starlight · 45%', tint: '#4a7ab8', strength: 0.45, blend: 'soft-light' },
  { id: 'warm-dusk', name: 'Warm Dusk', desc: 'amber light · 35%', tint: '#e8a25c', strength: 0.35, blend: 'soft-light' },
];

/* ------------------------------------------------------- scenario presets -- */

const PRESETS = [
  {
    id: 'solar', name: 'Solar System', icon: '☀',
    desc: 'The Sun, eight planets, Pluto and seven major moons on their real ephemerides.',
  },
  {
    id: 'inner', name: 'Inner System', icon: '☾',
    desc: 'Mercury to Mars with the Moon — fast orbits, close-up action.',
  },
  {
    id: 'giants', name: 'Jupiter & Saturn', icon: '♃',
    desc: 'The two gas giants with their Galilean moons and Titan.',
  },
  {
    id: 'binary', name: 'Binary Star', icon: '☀☀',
    desc: 'Two suns waltzing around each other with three circumbinary worlds — live gravity.',
  },
  {
    id: 'empty', name: 'Empty Sandbox', icon: '✦',
    desc: 'A lone star. Build your own system with the Create tab.',
  },
];

/* --------------------------------------------------- create-tab templates -- */

const BODY_TYPES = [
  { id: 'asteroid', label: 'Asteroid', massEarth: 1e-4, radiusKm: 120, color: '#b0aa9e', dfltDist: 1.8 },
  { id: 'comet', label: 'Comet', massEarth: 1e-7, radiusKm: 12, color: '#bcd8e8', dfltDist: 4.5 },
  { id: 'terrestrial', label: 'Terrestrial', massEarth: 1, radiusKm: 6371, color: '#5f9fe8', dfltDist: 1.5 },
  { id: 'icegiant', label: 'Ice giant', massEarth: 14.5, radiusKm: 25362, color: '#9ed6da', dfltDist: 12 },
  { id: 'gasgiant', label: 'Gas giant', massEarth: 318, radiusKm: 69911, color: '#d9b088', dfltDist: 6 },
  { id: 'dwarf', label: 'Dwarf', massEarth: 0.0022, radiusKm: 1188, color: '#cfc9bd', dfltDist: 20 },
  { id: 'star', label: 'Star', massEarth: 332946, radiusKm: 696000, color: '#ffb84e', dfltDist: 25 },
  { id: 'blackhole', label: 'Black hole', massEarth: 3.6e6, radiusKm: 21, color: '#111318', dfltDist: 40 },
];

const PARTS = [
  { name: 'Ocean World', type: 'terrestrial', massEarth: 1.3, radiusKm: 6900, color: '#3f9bd8',
    desc: 'A temperate waterworld wrapped in white clouds.' },
  { name: 'Ember', type: 'terrestrial', massEarth: 0.06, radiusKm: 2900, color: '#e06a4a',
    desc: 'A molten rock hugging its star; rivers of lava on the day side.' },
  { name: 'Ringed Ice Giant', type: 'icegiant', massEarth: 14.5, radiusKm: 25362, color: '#9ed6da', ring: true,
    desc: 'A cold methane atmosphere girdled by narrow dark rings.' },
  { name: 'Iron Heart', type: 'asteroid', massEarth: 8e-4, radiusKm: 260, color: '#9aa4ae',
    desc: 'The exposed core of a shattered protoplanet.' },
  { name: 'Wanderer', type: 'comet', massEarth: 2e-7, radiusKm: 14, color: '#bcd8e8',
    desc: 'A long-period visitor that will grow a tail near the star.' },
];

/* ------------------------------------------------------------- utilities -- */

function fmtNumber(v, unit, digits) {
  const abs = Math.abs(v);
  let s;
  if (abs >= 1e6) s = (v / 1e6).toFixed(1) + 'M';
  else if (abs >= 1e3) s = (v / 1e3).toFixed(abs >= 1e4 ? 0 : 1) + 'k';
  else if (abs >= 100) s = v.toFixed(0);
  else if (abs >= 10) s = v.toFixed(1);
  else if (abs >= 1) s = v.toFixed(digits === undefined ? 2 : digits);
  else if (abs >= 0.01) s = v.toFixed(3);
  else if (abs === 0) s = '0';
  else s = v.toExponential(1);
  return unit ? s + ' ' + unit : s;
}

function fmtMassEarth(mEarth) {
  if (mEarth >= 0.05) return fmtNumber(mEarth, 'Earth', 2);
  return (mEarth >= 1e-4 ? mEarth.toFixed(4) : mEarth.toExponential(1)) + ' Earth';
}
