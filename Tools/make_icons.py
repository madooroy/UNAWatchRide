#!/usr/bin/env python3
"""Draw Ride's launcher and store icons in the UNA launcher style: a flat
glyph on transparency, teal with a grey accent (like the stock icons).

The glyph is the Route face in miniature: a road that winds up a climb from a
grey start dot to the rider's filled dot.

Writes Resources/icon_60x60.png, icon_30x30.png (launcher) and icon.png
(256 px, store listing).
"""
import math
import os

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TEAL = (0, 128, 128, 255)
GREY = (192, 192, 192, 255)


def bezier(p0, p1, p2, p3, n=64):
    pts = []
    for i in range(n + 1):
        t = i / n
        u = 1 - t
        x = u**3 * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t**3 * p3[0]
        y = u**3 * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t**3 * p3[1]
        pts.append((x, y))
    return pts


def thick_line(d, pts, w, fill):
    for a, b in zip(pts, pts[1:]):
        d.line([a, b], fill=fill, width=int(w))
    for x, y in pts:  # round joins and caps
        d.ellipse([x - w / 2, y - w / 2, x + w / 2, y + w / 2], fill=fill)


def icon(size):
    s = 16                       # supersample, then downscale for clean edges
    N = size * s
    im = Image.new("RGBA", (N, N), (0, 0, 0, 0))
    d = ImageDraw.Draw(im)

    def P(x, y):                 # unit square -> pixels
        return (x * N, y * N)

    # The road: from bottom-left, a switchback, up to the top-right.
    road = bezier(P(0.14, 0.86), P(0.70, 0.92), P(0.78, 0.58), P(0.42, 0.52)) + \
           bezier(P(0.42, 0.52), P(0.10, 0.46), P(0.30, 0.14), P(0.80, 0.18))[1:]
    w = N * 0.12
    thick_line(d, road, w, TEAL)

    # Start (grey ring) and rider (filled teal dot with a halo gap).
    sx, sy = road[0]
    r = N * 0.10
    d.ellipse([sx - r, sy - r, sx + r, sy + r], fill=GREY)
    ex, ey = road[-1]
    gap = N * 0.16
    d.ellipse([ex - gap, ey - gap, ex + gap, ey + gap], fill=(0, 0, 0, 0))
    r = N * 0.12
    d.ellipse([ex - r, ey - r, ex + r, ey + r], fill=TEAL)
    return im.resize((size, size), Image.LANCZOS)


out = os.path.join(ROOT, "Resources")
for sz in (60, 30):
    icon(sz).save(os.path.join(out, f"icon_{sz}x{sz}.png"))
icon(256).save(os.path.join(out, "icon.png"))
print("icons written to", out)
