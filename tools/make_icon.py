#!/usr/bin/env python3
"""Generate the Solar System Sandbox application icon.

Artwork is drawn at 4x and downsampled for clean edges.  Outputs a
multi-resolution Windows .ico (16..256 px) plus PNGs for docs/packaging.
Only Pillow + numpy are required.
"""
from __future__ import annotations

import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ASSETS = os.path.join(ROOT, "assets")
DOCS = os.path.join(ROOT, "docs")
S = 1024  # supersampled canvas


def radial_layer(cx, cy, radius, inner, outer, power=2.0, size=S):
    """RGBA radial gradient centred on (cx, cy)."""
    y, x = np.mgrid[0:size, 0:size].astype(np.float32)
    d = np.sqrt((x - cx) ** 2 + (y - cy) ** 2) / max(radius, 1e-6)
    t = np.clip(d, 0.0, 1.0) ** power
    a = np.zeros((size, size, 4), np.float32)
    for ch in range(4):
        a[..., ch] = inner[ch] + (outer[ch] - inner[ch]) * t
    return a


def paste_radial(base, layer):
    alpha = layer[..., 3:4] / 255.0
    base[..., :3] = layer[..., :3] * alpha + base[..., :3] * (1 - alpha)
    base[..., 3:4] = np.maximum(base[..., 3:4], layer[..., 3:4])


def rounded_mask(size, radius, inset=0):
    m = Image.new("L", (size, size), 0)
    ImageDraw.Draw(m).rounded_rectangle(
        [inset, inset, size - 1 - inset, size - 1 - inset], radius=radius, fill=255
    )
    return np.asarray(m, np.float32) / 255.0


def main() -> int:
    os.makedirs(ASSETS, exist_ok=True)
    os.makedirs(DOCS, exist_ok=True)

    # ------------------------------------------------------------------ background
    bg = np.zeros((S, S, 4), np.float32)
    bg[..., 3] = 255
    yy = np.linspace(0, 1, S, dtype=np.float32)[:, None]
    top = np.array([9, 14, 27], np.float32)
    bot = np.array([17, 29, 53], np.float32)
    for c in range(3):
        bg[..., c] = top[c] + (bot[c] - top[c]) * yy

    paste_radial(bg, radial_layer(S * 0.84, S * 0.10, S * 0.58, (44, 86, 158, 52), (44, 86, 158, 0), 2.0))
    paste_radial(bg, radial_layer(S * 0.10, S * 0.90, S * 0.60, (34, 104, 138, 24), (34, 104, 138, 0), 2.0))
    # the sun, rising in the upper-left corner
    paste_radial(bg, radial_layer(S * 0.175, S * 0.170, S * 0.30, (255, 190, 96, 52), (255, 150, 60, 0), 1.6))
    paste_radial(bg, radial_layer(S * 0.175, S * 0.170, S * 0.105, (255, 238, 190, 130), (255, 186, 90, 0), 1.4))
    paste_radial(bg, radial_layer(S * 0.175, S * 0.170, S * 0.050, (255, 252, 236, 255), (255, 214, 140, 0), 1.25))

    img = Image.fromarray(np.clip(bg, 0, 255).astype(np.uint8), "RGBA")
    draw = ImageDraw.Draw(img, "RGBA")

    # ---------------------------------------------------------------------- stars
    rng = np.random.default_rng(20261018)
    for _ in range(120):
        x, y = rng.uniform(0, S, 2)
        r = float(rng.uniform(0.8, 2.4))
        a = int(rng.uniform(30, 170))
        draw.ellipse([x - r, y - r, x + r, y + r], fill=(216, 228, 255, a))
    for x, y, r in [(S * 0.795, S * 0.245, 3.0), (S * 0.155, S * 0.715, 2.4), (S * 0.63, S * 0.845, 2.0)]:
        draw.ellipse([x - r * 3.4, y - r * 3.4, x + r * 3.4, y + r * 3.4], fill=(170, 210, 255, 26))
        draw.ellipse([x - r, y - r, x + r, y + r], fill=(246, 250, 255, 230))
        draw.line([x - r * 7, y, x + r * 7, y], fill=(200, 226, 255, 56), width=2)
        draw.line([x, y - r * 7, x, y + r * 7], fill=(200, 226, 255, 56), width=2)

    # --------------------------------------------------------------------- planet
    pcx, pcy, pr = S * 0.495, S * 0.525, S * 0.243

    def ellipse_box(rx, ry):
        return [pcx - rx, pcy - ry, pcx + rx, pcy + ry]

    ring_rx = pr * 2.06
    ring_ry = pr * 0.50
    RINGS = [
        (4.15, 0.0060, 55, (150, 172, 208)),   # faint outer shepherd ring
        (3.62, 0.0190, 150, (247, 226, 182)),  # B ring (bright, wide)
        (3.16, 0.0080, 90, (222, 192, 140)),
        (2.78, 0.0150, 135, (240, 214, 168)),  # A ring
        (2.42, 0.0090, 85, (214, 186, 138)),
        (2.18, 0.0040, 45, (188, 162, 118)),   # C ring
    ]

    def ring_layer(front_only):
        layer = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        d = ImageDraw.Draw(layer, "RGBA")
        for ratio, halfw, alpha, col in RINGS:
            rx = ring_rx * ratio / 2.06
            ry = ring_ry * ratio / 2.06
            w = max(2, int(S * halfw))
            if front_only:
                d.arc(ellipse_box(rx, ry), 0, 180, fill=col + (alpha,), width=w)
            else:
                d.ellipse(ellipse_box(rx, ry), outline=col + (alpha,), width=w)
        return layer

    RING_TILT = -10.0

    def tilt(layer):
        return layer.rotate(RING_TILT, resample=Image.BICUBIC, center=(pcx, pcy))

    img = Image.alpha_composite(img, tilt(ring_layer(False)))
    draw = ImageDraw.Draw(img, "RGBA")

    # planet sphere: light from upper-left
    lx, ly = pcx - pr * 0.40, pcy - pr * 0.42
    y, x = np.mgrid[0:S, 0:S].astype(np.float32)
    ndist = np.sqrt((x - lx) ** 2 + (y - ly) ** 2) / (pr * 1.85)
    t = np.clip(ndist, 0, 1) ** 1.25
    stops = [
        (0.00, (196, 233, 255)),
        (0.28, (110, 178, 240)),
        (0.58, (52, 112, 178)),
        (0.82, (23, 58, 104)),
        (1.00, (11, 26, 52)),
    ]
    sphere = np.zeros((S, S, 4), np.float32)
    for ch in range(3):
        grid = np.zeros_like(t)
        for i in range(len(stops) - 1):
            p0, c0 = stops[i]
            p1, c1 = stops[i + 1]
            m = (t >= p0) & (t <= p1)
            grid[m] = c0[ch] + (c1[ch] - c0[ch]) * ((t[m] - p0) / (p1 - p0))
        sphere[..., ch] = grid
    sphere[..., 3] = 255

    disc = Image.new("L", (S, S), 0)
    ImageDraw.Draw(disc).ellipse(ellipse_box(pr, pr), fill=255)
    sphere[..., 3] *= np.asarray(disc, np.float32) / 255.0
    planet = Image.fromarray(np.clip(sphere, 0, 255).astype(np.uint8), "RGBA")

    # atmosphere halo
    halo = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(halo).ellipse(ellipse_box(pr + 10, pr + 10), fill=(120, 190, 255, 46))
    halo = halo.filter(ImageFilter.GaussianBlur(18))
    img = Image.alpha_composite(img, halo)
    img = Image.alpha_composite(img, planet)
    draw = ImageDraw.Draw(img, "RGBA")

    # subtle latitude bands, clipped to the disc
    bands = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    bd = ImageDraw.Draw(bands, "RGBA")
    for i in range(7):
        by = pcy - pr * 0.68 + i * pr * 0.225
        half = math.sqrt(max(0.0, 1 - ((by - pcy) / pr) ** 2)) * pr
        col = (230, 244, 255, 22) if i % 2 == 0 else (10, 30, 60, 26)
        bd.ellipse([pcx - half, by - pr * 0.062, pcx + half, by + pr * 0.062], fill=col)
    bmask = Image.new("L", (S, S), 0)
    ImageDraw.Draw(bmask).ellipse(ellipse_box(pr, pr), fill=255)
    bands.putalpha(Image.composite(bands.getchannel("A"), Image.new("L", (S, S), 0), bmask))
    img = Image.alpha_composite(img, bands)
    draw = ImageDraw.Draw(img, "RGBA")

    # warm rim light where the sun grazes the limb
    rim = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(rim, "RGBA").arc(
        ellipse_box(pr - 5, pr - 5), 150, 275, fill=(255, 226, 174, 205), width=int(S * 0.0075)
    )
    img = Image.alpha_composite(img, rim.filter(ImageFilter.GaussianBlur(2.4)))
    draw = ImageDraw.Draw(img, "RGBA")

    # specular glint
    glint = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(glint, "RGBA").ellipse(
        [pcx - pr * 0.62, pcy - pr * 0.72, pcx - pr * 0.05, pcy - pr * 0.22],
        fill=(255, 255, 255, 70),
    )
    img = Image.alpha_composite(img, glint.filter(ImageFilter.GaussianBlur(16)))
    draw = ImageDraw.Draw(img, "RGBA")

    # rings in front of the planet
    img = Image.alpha_composite(img, tilt(ring_layer(True)))
    draw = ImageDraw.Draw(img, "RGBA")

    # moon + orbit arc
    orbit = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(orbit, "RGBA").arc(
        [pcx - pr * 2.9, pcy - pr * 1.05, pcx + pr * 2.9, pcy + pr * 1.55],
        200, 340, fill=(196, 220, 255, 34), width=3,
    )
    img = Image.alpha_composite(img, orbit)

    mx, my, mr = S * 0.822, S * 0.705, pr * 0.088
    moon_glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(moon_glow).ellipse([mx - mr * 2.6, my - mr * 2.6, mx + mr * 2.6, my + mr * 2.6],
                                      fill=(140, 195, 255, 30))
    img = Image.alpha_composite(img, moon_glow.filter(ImageFilter.GaussianBlur(10)))
    draw = ImageDraw.Draw(img, "RGBA")
    draw.ellipse([mx - mr, my - mr, mx + mr, my + mr], fill=(206, 220, 236, 255))
    draw.ellipse([mx - mr * 0.80, my - mr * 0.88, mx + mr * 0.28, my + mr * 0.10], fill=(246, 250, 255, 255))
    draw.ellipse([mx + mr * 0.20, my + mr * 0.18, mx + mr * 0.78, my + mr * 0.72], fill=(150, 168, 190, 255))

    # ---------------------------------------------------------------- glass frame
    mask = rounded_mask(S, int(S * 0.215), 8)
    arr = np.asarray(img, np.float32)
    arr[..., 3] *= mask
    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), "RGBA")

    border = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    bd = ImageDraw.Draw(border, "RGBA")
    bd.rounded_rectangle([6, 6, S - 7, S - 7], radius=int(S * 0.215), outline=(4, 8, 18, 210), width=8)
    bd.rounded_rectangle([14, 14, S - 15, S - 15], radius=int(S * 0.208), outline=(122, 178, 255, 120), width=4)
    bd.rounded_rectangle([26, 26, S - 27, S - 27], radius=int(S * 0.196), outline=(255, 255, 255, 26), width=2)
    # glossy top highlight
    gloss = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    ImageDraw.Draw(gloss, "RGBA").rounded_rectangle(
        [14, 14, S - 15, int(S * 0.46)], radius=int(S * 0.20), fill=(255, 255, 255, 16)
    )
    gloss = gloss.filter(ImageFilter.GaussianBlur(9))
    border = Image.alpha_composite(border, gloss)
    img = Image.alpha_composite(img, border)

    # ----------------------------------------------------------------------- output
    icon_path = os.path.join(ASSETS, "icon.ico")
    img.save(icon_path, format="ICO", sizes=[(16, 16), (24, 24), (32, 32), (48, 48), (64, 64), (128, 128), (256, 256)])
    img.resize((512, 512), Image.LANCZOS).save(os.path.join(ASSETS, "icon.png"))
    img.resize((256, 256), Image.LANCZOS).save(os.path.join(DOCS, "icon.png"))
    print("wrote", icon_path, os.path.getsize(icon_path), "bytes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
