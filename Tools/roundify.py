#!/usr/bin/env python3
"""Mask simulator screenshots to the watch's round face and tile them.
usage: roundify.py out.png in1.png [in2.png ...]  (single input -> single round PNG)"""
import sys
from PIL import Image, ImageDraw

def round_face(path, bezel=(28, 28, 28)):
    im = Image.open(path).convert("RGB")
    s = im.size[0]
    pad = s // 16
    out = Image.new("RGBA", (s + 2 * pad, s + 2 * pad), (0, 0, 0, 0))
    d = ImageDraw.Draw(out)
    d.ellipse([0, 0, s + 2 * pad - 1, s + 2 * pad - 1], fill=bezel + (255,))
    mask = Image.new("L", im.size, 0)
    ImageDraw.Draw(mask).ellipse([0, 0, s - 1, s - 1], fill=255)
    out.paste(im, (pad, pad), mask)
    return out

out, ins = sys.argv[1], sys.argv[2:]
faces = [round_face(p) for p in ins]
if len(faces) == 1:
    faces[0].save(out)
else:
    w, h = faces[0].size
    gap = 16
    sheet = Image.new("RGBA", (len(faces) * (w + gap) - gap, h), (0, 0, 0, 0))
    for i, f in enumerate(faces):
        sheet.paste(f, (i * (w + gap), 0), f)
    sheet.save(out)
