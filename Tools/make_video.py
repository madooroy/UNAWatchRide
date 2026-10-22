#!/usr/bin/env python3
"""Cut the ride recording from `Tools/sim_tour.sh OUTDIR --video` into a
~75 s captioned video: the watch in a round bezel, a caption underneath.
Quiet stretches are sped up; screens shown only briefly in the recording
are held as stills.

usage: Tools/make_video.py OUTDIR      (writes OUTDIR/Ride_demo.mp4)
Needs ffmpeg and Pillow.
"""
import os, subprocess, sys
from PIL import Image, ImageDraw

OUT = sys.argv[1]
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = os.path.join(ROOT, 'Software/Apps/TouchGFX-GUI/assets/fonts/Poppins-SemiBold.ttf')
VIDEO = os.path.join(OUT, 'demo.mp4')
W, H, S = 720, 800, 600
X, Y = (W - S) // 2, 40
BG = (18, 18, 18)

# When each screenshot was taken, relative to the start of the recording.
# The recording ends just after the last screenshot.
def mtime(p):
    return os.stat(p).st_mtime
dur = float(subprocess.run(['ffprobe', '-v', 'error', '-show_entries', 'format=duration', '-of', 'csv=p=0', VIDEO],
                           capture_output=True, text=True, check=True).stdout)
start = mtime(VIDEO) - dur
shots = {f[:-4]: mtime(os.path.join(OUT, f)) - start for f in os.listdir(OUT) if f[:2].isdigit() and f.endswith('.png')}
def at(name, d=0.0):
    return shots[name] + d

# (kind, from, to-or-None, speed-or-seconds, caption)
SEGS = [
    ('clip', at('01_menu', -1.0), at('06_riding'), 1, 'Start, intervals or settings: buttons only'),
    ('clip', at('06_riding'), at('07_speed', 3.0), 3, 'Following a route from route.gpx'),
    ('clip', at('07_speed', 3.0), at('08_off_course', -1.5), 6, 'Riding the route'),
    ('png', '08_off_course', None, 3.0, 'Off course: buzz and distance to the route'),
    ('png', '09_back_on_course', None, 2.4, 'Back on route'),
    ('png', '10_route_complete', None, 2.6, 'Route complete'),
    ('png', '11_lap', None, 1.8, 'Lap face'),
    ('png', '12_clock', None, 1.8, 'Clock and battery'),
    ('png', '14_overview', None, 2.2, 'Heart rate zone, average speed, elevation'),
    ('clip', at('15_climb_flat', -0.3), at('16_climb_start', -1.5), 8, 'Climb face: live grade'),
    ('png', '16_climb_start', None, 2.6, 'A climb starts: buzz'),
    ('clip', at('17_climbing', -0.5), at('19_summit', -1.5), 10, 'Grade, gain, distance and VAM on the climb'),
    ('png', '19_summit', None, 3.0, 'Summit: length, gain, average grade'),
    ('png', '20_action_menu', None, 1.8, 'Pause menu'),
    ('png', '22_hold_to_finish', None, 2.0, 'Hold to finish, so a bump never ends a ride'),
    ('png', '24_summary_2', None, 2.4, 'Summary'),
    ('png', '23_summary', None, 2.4, 'The ride on a map'),
    ('png', '25_summary_3', None, 2.0, 'Heart rate'),
    ('png', '26_summary_4', None, 2.0, 'Laps, saved as a FIT file'),
]

mask = Image.new('RGBA', (W, H), BG + (255,))
d = ImageDraw.Draw(mask)
d.ellipse([X - 22, Y - 22, X + S + 21, Y + S + 21], fill=(44, 44, 44, 255))
d.ellipse([X, Y, X + S - 1, Y + S - 1], fill=(0, 0, 0, 0))
mask_png = os.path.join(OUT, 'mask.png')
mask.save(mask_png)

def esc(c):
    return c.replace(':', '\\:').replace("'", '’').replace(',', '\\,')

parts = []
for i, (kind, a, b, sp, cap) in enumerate(SEGS):
    seg = os.path.join(OUT, f'seg{i:02d}.mp4')
    parts.append(seg)
    if kind == 'clip':
        inp = ['-ss', f'{a:.2f}', '-t', f'{b - a:.2f}', '-i', VIDEO]
        pre = f'setpts=PTS/{sp},fps=25,'
    else:
        inp = ['-loop', '1', '-t', f'{sp}', '-i', os.path.join(OUT, a + '.png')]
        pre = 'fps=25,'
    fc = (f'[0:v]{pre}scale={S}:{S}[v];color=c=0x121212:s={W}x{H}:r=25[bg];'
          f'[bg][v]overlay={X}:{Y}:shortest=1[o];[o][1:v]overlay=0:0,'
          f"drawtext=fontfile={FONT}:text='{esc(cap)}':fontsize=30:fontcolor=white:"
          f'x=(w-tw)/2:y={Y + S + 60},format=yuv420p')
    subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', *inp, '-i', mask_png, '-filter_complex', fc,
                    '-an', '-c:v', 'libx264', '-crf', '20', '-r', '25', seg], check=True)

lst = os.path.join(OUT, 'segments.txt')
with open(lst, 'w') as f:
    f.writelines(f"file '{p}'\n" for p in parts)
final = os.path.join(OUT, 'Ride_demo.mp4')
subprocess.run(['ffmpeg', '-loglevel', 'error', '-y', '-f', 'concat', '-safe', '0', '-i', lst,
                '-c', 'copy', '-movflags', '+faststart', final], check=True)
for p in parts + [lst, mask_png]:
    os.remove(p)
print(final)
