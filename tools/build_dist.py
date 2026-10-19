#!/usr/bin/env python3
"""Package the site for distribution.

  python3 tools/build_dist.py

Writes into dist/:
  SolarSystemSandbox-<version>-game.html  single-file standalone build
                                          (all CSS/JS inlined, inline SVG favicon)
  SolarSystemSandbox-<version>-site.zip   full multi-file site copy

Run the smoke test against the extracted inline scripts afterwards:
  node tests/smoke.cjs js/data.js js/sim.js js/audio.js js/main.js
"""
from __future__ import annotations

import os
import re
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(ROOT)

version = open('VERSION').read().strip()
html = open('index.html').read()
css = open('css/style.css').read()

FAVICON = ("data:image/svg+xml,"
           "%3Csvg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 32 32'%3E"
           "%3Ccircle cx='16' cy='16' r='8' fill='%235f9fe8'/%3E"
           "%3Cellipse cx='16' cy='16' rx='14' ry='4' fill='none' "
           "stroke='%23e0cb96' stroke-width='1.6' transform='rotate(-18 16 16)'/%3E%3C/svg%3E")

single = html.replace('<link rel="icon" href="assets/icon.ico" sizes="any">',
                      f'<link rel="icon" href="{FAVICON}">')
# tolerate an optional ?v= cache-bust query on the stylesheet link
single = re.sub(r'<link rel="stylesheet" href="css/style\.css(?:\?[^"]*)?">',
                '<style>\n' + css + '\n</style>', single, count=1)

# inline every local script tag (with optional ?v= cache-bust query), in order
script_tags = re.findall(r'<script src="(js/[^"?]+)(?:\?[^"]*)?"></script>', single)
assert script_tags, 'no local scripts found'
for tag_path in script_tags:
    code = open(tag_path).read()
    tag_match = re.search(r'<script src="' + re.escape(tag_path) + r'(?:\?[^"]*)?"></script>', single)
    assert tag_match, tag_path
    single = single[:tag_match.start()] + '<script>\n' + code + '\n</script>' + single[tag_match.end():]

banner = (f'<!--\n  Solar System Sandbox v{version} — STANDALONE SINGLE-FILE BUILD\n'
          '  The complete simulator in one file: open anywhere, works offline.\n'
          '  Regenerate with: python3 tools/build_dist.py\n-->\n')
single = single.replace('<!doctype html>', '<!doctype html>\n' + banner, 1)

game_out = f'dist/SolarSystemSandbox-{version}-game.html'
with open(game_out, 'w') as f:
    f.write(single)
print(f'wrote {game_out} ({len(single) // 1024} KB)')

# sanity: nothing references the external files any more
assert 'rel="stylesheet"' not in single
assert '<script src="js/' not in single
assert css in single
# the boot-error watcher script must survive inlining (5 inline scripts total)
assert single.count('<script>') == 5, single.count('<script>')

zip_out = f'dist/SolarSystemSandbox-{version}-site.zip'
members = ['index.html', 'css/style.css', 'js/data.js', 'js/sim.js', 'js/audio.js',
           'js/main.js', 'assets/icon.ico', 'assets/icon.png', '404.html',
           'README.md', 'LICENSE', 'VERSION', '.nojekyll']
with zipfile.ZipFile(zip_out, 'w', zipfile.ZIP_DEFLATED) as z:
    for m in members:
        assert os.path.isfile(m), m
        z.write(m, m)
print(f'wrote {zip_out} ({os.path.getsize(zip_out) // 1024} KB, {len(members)} files)')
