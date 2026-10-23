"""FIT records -> CSV, one row per record (the Karoo records once per active second).
Columns: active_s,elapsed_s,dist_m,alt_m,lat,lon,speed_mps,hr,karoo_grade"""
import sys, fitdecode
SC = 180.0 / 2**31
src, dst = sys.argv[1], sys.argv[2]
t0 = None
with fitdecode.FitReader(src) as fr, open(dst, "w") as out:
    out.write("active_s,elapsed_s,dist_m,alt_m,lat,lon,speed_mps,hr,karoo_grade\n")
    n = 0
    for fm in fr:
        if not isinstance(fm, fitdecode.FitDataMessage) or fm.name != "record":
            continue
        g = lambda k: fm.get_value(k, fallback=None)
        ts = g("timestamp")
        if t0 is None: t0 = ts
        n += 1
        lat, lon = g("position_lat"), g("position_long")
        alt = g("enhanced_altitude") if g("enhanced_altitude") is not None else g("altitude")
        f = lambda v, d=6: "" if v is None else f"{v:.{d}f}"
        out.write(",".join([str(n), str(int((ts - t0).total_seconds())), f(g("distance"), 2), f(alt, 2),
                            f(lat * SC if lat is not None else None, 7), f(lon * SC if lon is not None else None, 7),
                            f(g("enhanced_speed"), 3), "" if g("heart_rate") is None else str(g("heart_rate")),
                            f(g("grade"), 2)]) + "\n")
print(dst, n)
