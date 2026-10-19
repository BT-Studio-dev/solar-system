# Solar System Sandbox

**[Open the simulator](index.html)**
**[Windows installer — one file for every Windows 7+ PC](windows/SolarSystemSandbox-1.0.0-Setup.exe)** ·
[all Windows builds](windows/) — the original native app (auto-selects 32/64-bit at install time; portable exes too), built from the same project's C sources · a real-time solar system and N-body gravity sandbox that runs entirely in your browser — plain HTML, CSS and JavaScript, no build step and no dependencies.

Planets follow **JPL's approximate ephemerides** (J2000 Keplerian elements with secular rates, accurate to minutes of arc across 1800–2050). The moment you add, remove or release a body, the whole system switches to a symplectic **velocity-Verlet N-body integrator** and everything starts pulling on everything else.

## Features

- **Real sky** — the Sun, eight planets, Pluto and seven major moons on their actual orbits for any date; press *NOW* to jump to today.
- **Gravity sandbox** — create bodies (asteroid to black hole), aim them by dragging in the sky, and watch perturbations, slingshots and collisions (momentum-conserving merges).
- **Hybrid engine** — planets ride their ephemeris "rails" while your bodies feel the full moving gravitational field; release gravity and the rails come off.
- **Five colour themes** — Aero Night, Aero Day, Luna Blue, Aurora and High Contrast, restyling the shell *and* the viewport (<kbd>T</kbd> or <kbd>F3</kbd>).
- **Inspector** — click any body for live distance, period, speed, mass and facts; focus or follow it around its orbit.
- **Simulation panel** — set the start date (1800–2050), toggle Labels / Orbits / Moons / Trails, auto-track any body, read the live separation between any two objects, and one-click *Spawn asteroid*, *Spawn comet*, *Clear trails* and *Empty sky*.
- **Texture packs** — *Deep Blue* (cold starlight · 45%) and *Warm Dusk* (amber light · 35%) recolour the viewport over any theme, just like the desktop app's `pack.ini` files.
- **Satellites** — select a planet to see its moons with orbit radii; click one to select it.
- **Scenario files** — the Library tab exports and imports plain `.json` scenarios you can share; five presets from *Inner System* to a *Binary Star* run.
- **Procedural sound** — every effect (select blips, add/remove sweeps, collision thuds, the gravity-release whoosh, theme switches) is synthesized live with the Web Audio API — no audio files. Master volume slider, mute, and an optional ambient space hum under the 🔊 button (<kbd>M</kbd> to mute).

## Run it

It is a static site. Either open `index.html` directly, or serve the folder:

```bash
python3 -m http.server 8000
# → http://localhost:8000
```

Pushes to `main` deploy automatically to **GitHub Pages** via the workflow in `.github/`.

## Controls

| Input | Action |
| --- | --- |
| <kbd>Space</kbd> | play / pause |
| drag / wheel | pan / zoom to cursor |
| click / double-click | select / select + follow |
| <kbd>F</kbd> | focus the selected body |
| <kbd>T</kbd>, <kbd>F3</kbd> | cycle theme |
| <kbd>M</kbd> | mute / unmute sound |
| <kbd>C</kbd> / <kbd>L</kbd> | Create / Library tab |
| <kbd>Delete</kbd> | remove the selected body |
| Create → *Place by drag* | press to set position, drag to aim velocity, release to launch |

## Project structure

```
index.html        app shell: transport bar, viewport, inspector, status bar
css/style.css     the five themes ([data-theme]) and all chrome styling
js/data.js        JPL elements, body catalogue, themes, presets, part templates
js/sim.js         ephemeris math + velocity-Verlet N-body engine
js/audio.js       Web Audio sound synthesis (SFX, volume, ambient hum)
js/main.js        canvas renderer and interface
assets/           favicon
404.html          themed not-found page for GitHub Pages
```

## Tests

`node --check js/*.js` plus a headless smoke harness (stubbed DOM/canvas) that
verifies ephemeris accuracy (Earth's J2000 perihelion distance and longitude),
moon stability after gravity release, integrator energy drift on the binary
preset, collision merges and scenario save/load round-trips.

## License

MIT — see [LICENSE](LICENSE).
