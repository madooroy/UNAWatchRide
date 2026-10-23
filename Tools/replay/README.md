# Ride replay

Runs a recorded ride through the app's climb detection and route following on a PC, fed the same way the watch's service feeds them, and prints what the watch would have shown and buzzed.

- Route: streamed through `GpxReader` in 1 KB chunks into `Route`.
- Altitude: through the SDK's `SimpleLPF` (alpha 0.8), then into the SDK's `DeltaCounter` (2 m) for total ascent and into `ClimbDetector`.
- Position: every active second with a fix is one `RouteFollower` tick.

## Build

Needs a checkout of [una-sdk](https://github.com/UNAWatch/una-sdk) for the filter and ascent-counter headers.

```sh
UNA_SDK=/path/to/una-sdk cmake -S Tools/replay -B Tools/replay/build
cmake --build Tools/replay/build
```

## Run

Convert a `.fit` ride to CSV (`pip install fitdecode`), then replay it, with or without its planned route:

```sh
python3 Tools/replay/fit2csv.py ride.fit ride.csv
Tools/replay/build/replay --ride ride.csv --route route.gpx --events events.csv --trace trace.csv
```

The printed log lists climb starts and summits (distance, gain, average grade, VAM), off-course and back-on-course alerts, and arrival. At the end it prints a summary: ascent, climbs, and route progress.

The `--events` CSV has one row per alert. Its columns `a`, `b`, `c` and `d` carry the numbers for that event:

| Event | `a` | `b` | `c` | `d` |
|---|---|---|---|---|
| `climb_end` | distance (m) | gain (m) | average grade (%) | VAM (m/h) |
| `off_course`, `back_on`, `arrived` | metres off route | metres along it | metres to go | |

`--trace` writes one row per second with grade, climb state, distance from the route, progress, ridden and remaining distance. Use it for plotting.

The CSV needs these columns, one row per active second (`fit2csv.py` writes them):

`active_s,elapsed_s,dist_m,alt_m,lat,lon,speed_mps,hr,karoo_grade`

Leave `lat` and `lon` empty for a second with no GPS fix.

## Privacy

Ride files show where people live and ride. Keep them out of this repository. The unit tests use synthetic routes only.
