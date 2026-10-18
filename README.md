# Solar System Sandbox

**[Open the simulator](index.html)** · a real-time solar system and N-body gravity sandbox that runs entirely in your browser — plain HTML, CSS and JavaScript, no build step and no dependencies.

Planets follow **JPL's approximate ephemerides** (J2000 Keplerian elements with secular rates, accurate to minutes of arc across 1800–2050). The moment you add, remove or release a body, the whole system switches to a symplectic **velocity-Verlet N-body integrator** and everything starts pulling on everything else.

## Features

- **Real sky** — the Sun, eight planets, Pluto and seven major moons on their actual orbits for any date; press *NOW* to jump to today.
- **Gravity sandbox** — create bodies (asteroid to black hole), aim them by dragging in the sky, and watch perturbations, slingshots and collisions (momentum-conserving merges).
- **Hybrid engine** — planets ride their ephemeris "rails" while your bodies feel the full moving gravitational field; release gravity and the rails come off.
- **Five colour themes** — Aero Night, Aero Day, Luna Blue, Aurora and High Contrast, restyling the shell *and* the viewport (<kbd>T</kbd> or <kbd>F3</kbd>).
- **Inspector** — click any body for live distance, period, speed, mass and facts; focus or follow it around its orbit.
- **Scenario files** — the Library tab exports and imports plain `.json` scenarios you can share; five presets from *Inner System* to a *Binary Star* run.

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
| <kbd>C</kbd> / <kbd>L</kbd> | Create / Library tab |
| <kbd>Delete</kbd> | remove the selected body |
| Create → *Place by drag* | press to set position, drag to aim velocity, release to launch |

## Project structure

```
index.html        app shell: transport bar, viewport, inspector, status bar
css/style.css     the five themes ([data-theme]) and all chrome styling
js/data.js        JPL elements, body catalogue, themes, presets, part templates
js/sim.js         ephemeris math + velocity-Verlet N-body engine
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
