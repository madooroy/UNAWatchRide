#!/usr/bin/env bash
# Plays a ride in the simulator on a virtual display and saves a screenshot of
# every screen, each taken when the matching event shows up in the log:
# the menus, every ride face, going off course and back and finishing a demo
# route (one lap whose second half takes a road up to 120 m away), a 4 % climb
# with its start and summit, and the summary. Optionally records it all as a video for
# Tools/make_video.py. Takes about 5 minutes.
#
# Build the simulator first (see README). Needs Xvfb, xdotool, ImageMagick
# (and ffmpeg for --video). Usage: Tools/sim_tour.sh OUTDIR [--video]
set -euo pipefail
OUT=${1:-/tmp/ride-tour}; mkdir -p "$OUT"
VIDEO=${2:-}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
GCC="$ROOT/Software/Apps/TouchGFX-GUI/simulator/gcc"
LOG="$OUT/sim.log"
export DISPLAY=${DISPLAY_NUM:-:95} UNA_SIM_HILL=4 UNA_SIM_HILL_SEC=120 UNA_SIM_SPEED=20
Xvfb "$DISPLAY" -screen 0 640x480x24 >/dev/null 2>&1 & XPID=$!
sleep 1
# The demo route; restore any route.gpx of your own afterwards.
[ -f "$ROOT/Output/route.gpx" ] && mv "$ROOT/Output/route.gpx" "$OUT/route.gpx.yours"
cp "$ROOT/docs/simulator-route-detour.gpx" "$ROOT/Output/route.gpx"
cd "$GCC"; stdbuf -oL -eL ../../build/bin/simulator.out > "$LOG" 2>&1 & SPID=$!
VPID=""
cleanup() {
    kill $SPID $XPID $VPID 2>/dev/null || true
    rm -f "$ROOT/Output/route.gpx"
    [ -f "$OUT/route.gpx.yours" ] && mv "$OUT/route.gpx.yours" "$ROOT/Output/route.gpx"
}
trap cleanup EXIT INT TERM
sleep 4
W=$(xdotool search --name "AppGUI" | head -1); xdotool windowmove "$W" 0 0; sleep 0.5

if [ "$VIDEO" = "--video" ]; then
    ffmpeg -loglevel error -y -f x11grab -draw_mouse 0 -framerate 25 -video_size 240x240 -i "$DISPLAY+0,0" \
        -c:v libx264 -pix_fmt yuv420p -crf 16 "$OUT/demo.mp4" & VPID=$!
    sleep 0.5
fi

snap() { import -window root -crop 240x240+0+0 "$OUT/$1.png"; echo "  $1"; }
key()  { xdotool key --window "$W" "$1"; sleep "${2:-0.8}"; }
waitlog() {
    local pat=$1 settle=${2:-0.3}
    for _ in $(seq 1 2400); do
        grep -q -- "$pat" "$LOG" && { sleep "$settle"; return 0; }
        sleep 0.25
    done
    echo "timeout waiting for '$pat'" >&2; return 1
}

sleep 1;  snap 01_menu                       # Start: Route 1.2 km
key 2;    snap 02_menu_intervals
key 2;    snap 03_menu_settings
key 3 1;  snap 04_settings
key 2; key 2; key 2; snap 05_settings_more
key 4 1;  key 2 1                            # back, then round to Start
key 3 1;  snap 06_riding                     # start the ride
waitlog "Track started" 8;   snap 07_speed
key 2; key 2; key 2 1                        # Route face
waitlog "Off course" 1.2;    snap 08_off_course
waitlog "Back on course" 1.2; snap 09_back_on_course
waitlog "Route complete" 1.2; snap 10_route_complete
sleep 2;   key 2 1.6; snap 11_lap
key 2 1.6; snap 12_clock
key 2 1.6; snap 13_speed
key 2 1.6; snap 14_overview
key 2 1.6; snap 15_climb_flat                   # stay on the Climb face
waitlog "Climb started" 1.2; snap 16_climb_start
sleep 8;   snap 17_climbing
sleep 13;  snap 18_climbing_more
waitlog "Climb finished" 1.2; snap 19_summit
sleep 6
key 3 1;   snap 20_action_menu               # Resume / Summary / Save & End / Discard
key 2; key 2 1; snap 21_save_end
# Save & End is hold-to-confirm: hold R1 until the ring fills. In the
# simulator, "e" is the R1 press and "d" the release.
key e 0.6; snap 22_hold_to_finish; sleep 1.4
key d 2
snap 23_summary
key 2 1.5; snap 24_summary_2
key 2 1.5; snap 25_summary_3
key 2 1.5; snap 26_summary_4

[ -n "$VPID" ] && { kill -INT $VPID; wait $VPID 2>/dev/null || true; VPID=""; }
echo "done: $OUT"
