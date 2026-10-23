/**
 * Replay a recorded ride through Ride's climb detection and route following,
 * exactly as the watch's service feeds them, and report what the watch would
 * have shown and buzzed.
 *
 *   replay --ride ride.csv [--route route.gpx] [--events out.csv] [--trace out.csv]
 *
 * ride.csv has one row per active second (Tools/replay/fit2csv.py writes it
 * from a .fit file):
 *   active_s,elapsed_s,dist_m,alt_m,lat,lon,speed_mps,hr,karoo_grade
 * Empty lat/lon means no GPS fix that second.
 *
 * As in Service.cpp: the route is streamed through GpxReader in 1 KB chunks
 * into Route; altitude goes through the SDK's SimpleLPF (alpha 0.8) into the
 * SDK's DeltaCounter (2 m) for total ascent, and into ClimbDetector with the
 * active distance and active seconds; every active second with a fix is a
 * RouteFollower tick.
 */

#include "Ride/ClimbDetector.hpp"
#include "Ride/GpxReader.hpp"
#include "Ride/Route.hpp"
#include "Ride/RouteFollower.hpp"

#include "SDK/Filters/SimpleLPF.hpp"
#include "SDK/Metrics/DeltaCounter.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

struct Row {
    uint32_t active = 0;
    uint32_t elapsed = 0;
    float    dist = 0.0f;
    float    alt = 0.0f;
    bool     fix = false;
    double   lat = 0.0, lon = 0.0;
};

bool readRide(const char* path, std::vector<Row>& rows)
{
    FILE* f = std::fopen(path, "r");
    if (!f) {
        return false;
    }
    char line[512];
    if (!std::fgets(line, sizeof(line), f)) {  // header
        std::fclose(f);
        return false;
    }
    while (std::fgets(line, sizeof(line), f)) {
        // Split on commas, keeping empty fields.
        std::vector<std::string> col;
        std::string cur;
        for (const char* p = line; *p && *p != '\n' && *p != '\r'; ++p) {
            if (*p == ',') {
                col.push_back(cur);
                cur.clear();
            } else {
                cur += *p;
            }
        }
        col.push_back(cur);
        if (col.size() < 6 || col[2].empty() || col[3].empty()) {
            continue;
        }
        Row r;
        r.active  = static_cast<uint32_t>(std::strtoul(col[0].c_str(), nullptr, 10));
        r.elapsed = static_cast<uint32_t>(std::strtoul(col[1].c_str(), nullptr, 10));
        r.dist    = std::strtof(col[2].c_str(), nullptr);
        r.alt     = std::strtof(col[3].c_str(), nullptr);
        r.fix     = !col[4].empty() && !col[5].empty();
        if (r.fix) {
            r.lat = std::strtod(col[4].c_str(), nullptr);
            r.lon = std::strtod(col[5].c_str(), nullptr);
        }
        rows.push_back(r);
    }
    std::fclose(f);
    return true;
}

bool addPoint(void* ctx, double lat, double lon)
{
    static_cast<Ride::Route*>(ctx)->add(lat, lon);
    return true;
}

bool loadRoute(const char* path, Ride::Route& route, uint32_t& read)
{
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        return false;
    }
    route.begin();
    Ride::GpxReader reader(addPoint, &route);
    char   chunk[1024];  // Service::skRouteChunk
    size_t n;
    while ((n = std::fread(chunk, 1, sizeof(chunk), f)) > 0) {
        reader.feed(chunk, n);
    }
    std::fclose(f);
    route.finish();
    read = reader.pointsRead();
    return route.valid();
}

void hms(char* out, size_t n, uint32_t s)
{
    std::snprintf(out, n, "%u:%02u:%02u", s / 3600, (s / 60) % 60, s % 60);
}

}  // namespace

int main(int argc, char** argv)
{
    const char* ridePath   = nullptr;
    const char* routePath  = nullptr;
    const char* eventsPath = nullptr;
    const char* tracePath  = nullptr;
    for (int i = 1; i + 1 < argc; i += 2) {
        if (!std::strcmp(argv[i], "--ride"))        ridePath   = argv[i + 1];
        else if (!std::strcmp(argv[i], "--route"))  routePath  = argv[i + 1];
        else if (!std::strcmp(argv[i], "--events")) eventsPath = argv[i + 1];
        else if (!std::strcmp(argv[i], "--trace"))  tracePath  = argv[i + 1];
    }
    if (!ridePath) {
        std::fprintf(stderr, "usage: replay --ride ride.csv [--route route.gpx] [--events e.csv] [--trace t.csv]\n");
        return 2;
    }

    std::vector<Row> rows;
    if (!readRide(ridePath, rows) || rows.empty()) {
        std::fprintf(stderr, "cannot read ride %s\n", ridePath);
        return 1;
    }

    auto route    = std::make_unique<Ride::Route>();
    auto follower = std::make_unique<Ride::RouteFollower>(*route);
    bool haveRoute = false;
    if (routePath) {
        uint32_t read = 0;
        haveRoute = loadRoute(routePath, *route, read);
        if (haveRoute) {
            std::printf("Route: %u points read, %u kept (tolerance %.1f m), %.2f km\n", read,
                        route->size(), route->toleranceM(), route->lengthM() / 1000.0f);
        } else {
            std::printf("Route: %s unusable (%u points read)\n", routePath, read);
        }
    }
    follower->reset();

    FILE* ev = eventsPath ? std::fopen(eventsPath, "w") : nullptr;
    FILE* tr = tracePath ? std::fopen(tracePath, "w") : nullptr;
    if (ev) std::fprintf(ev, "active_s,elapsed_s,dist_m,event,a,b,c,d\n");
    if (tr) std::fprintf(tr, "active_s,dist_m,alt_f,grade,grade_valid,in_climb,off_m,along_m,off_course,ridden_m,remaining_m,joined\n");

    SDK::Filter::SimpleLPF    lpf(0.8f);
    SDK::Metric::DeltaCounter ascent;
    ascent.init(2.0f);
    Ride::ClimbDetector climb;

    uint32_t offEvents = 0, offSeconds = 0, worstOffStart = 0;
    float    worstOff = 0.0f;
    char     t[16];

    std::printf("\n  %-8s %-8s %8s  %s\n", "active", "clock", "km", "event");
    for (const Row& r : rows) {
        const float alt = lpf.execute(r.alt);
        ascent.add(alt);

        const auto ce = climb.tick(r.dist, alt, true, r.active);
        if (ce == Ride::ClimbDetector::Event::Started) {
            hms(t, sizeof(t), r.active);
            std::printf("  %-8s %-8u %8.2f  climb started\n", t, r.elapsed, r.dist / 1000.0f);
            if (ev) std::fprintf(ev, "%u,%u,%.1f,climb_start,,,,\n", r.active, r.elapsed, r.dist);
        } else if (ce == Ride::ClimbDetector::Event::Finished) {
            const auto& c = climb.lastClimb();
            hms(t, sizeof(t), r.active);
            std::printf("  %-8s %-8u %8.2f  SUMMIT  %.2f km  +%.0f m  %.1f %%  VAM %.0f  (%u s)\n", t,
                        r.elapsed, r.dist / 1000.0f, c.distanceM / 1000.0f, c.gainM, c.avgGradePct,
                        c.vamMph, c.durationS);
            if (ev) std::fprintf(ev, "%u,%u,%.1f,climb_end,%.1f,%.1f,%.2f,%.0f\n", r.active, r.elapsed,
                                 r.dist, c.distanceM, c.gainM, c.avgGradePct, c.vamMph);
        }

        if (haveRoute) {
            const auto re = follower->tick(r.lat, r.lon, r.fix);
            const auto& s = follower->status();
            if (s.offCourse) {
                ++offSeconds;
            }
            using E = Ride::RouteFollower::Event;
            if (re != E::None) {
                hms(t, sizeof(t), r.active);
                const char* name = re == E::OffCourse ? "OFF COURSE" : re == E::BackOnCourse ? "back on course" : "ARRIVED";
                std::printf("  %-8s %-8u %8.2f  %s  (%.0f m off, %.2f km along, %.2f km to go)\n", t,
                            r.elapsed, r.dist / 1000.0f, name, s.offM, s.alongM / 1000.0f,
                            s.remainingM / 1000.0f);
                if (ev) std::fprintf(ev, "%u,%u,%.1f,%s,%.1f,%.1f,%.1f,\n", r.active, r.elapsed, r.dist,
                                     re == E::OffCourse ? "off_course" : re == E::BackOnCourse ? "back_on" : "arrived",
                                     s.offM, s.alongM, s.remainingM);
                if (re == E::OffCourse) {
                    ++offEvents;
                }
            }
            if (s.offM > worstOff) {
                worstOff      = s.offM;
                worstOffStart = r.active;
            }
        }

        if (tr) {
            const auto& s = follower->status();
            std::fprintf(tr, "%u,%.1f,%.2f,%.2f,%d,%d,%.1f,%.1f,%d,%.1f,%.1f,%d\n", r.active, r.dist, alt,
                         climb.gradePct(), climb.gradeValid() ? 1 : 0, climb.inClimb() ? 1 : 0,
                         haveRoute ? s.offM : 0.0f, haveRoute ? s.alongM : 0.0f,
                         haveRoute && s.offCourse ? 1 : 0, s.riddenM, s.remainingM, s.joined ? 1 : 0);
        }
    }

    const Row& last = rows.back();
    hms(t, sizeof(t), last.active);
    std::printf("\nRide: %s active, %.2f km\n", t, last.dist / 1000.0f);
    std::printf("Ascent (watch's DeltaCounter, 2 m): %.0f m   descent %.0f m\n", ascent.getAscent(),
                ascent.getDescent());
    std::printf("Climbs: %u, climbed %.0f m\n", climb.climbCount(), climb.climbedGainM());
    if (haveRoute) {
        const auto& s = follower->status();
        std::printf("Route: %s, %.2f km ridden of it, %.2f km along of %.2f km, arrived %s, off-course alerts %u (%u s off course), "
                    "worst %.0f m at %u s\n",
                    follower->closed() ? "loop" : "open", s.riddenM / 1000.0f,
                    s.alongM / 1000.0f, route->lengthM() / 1000.0f, s.arrived ? "yes" : "no", offEvents,
                    offSeconds, worstOff, worstOffStart);
    }
    if (ev) std::fclose(ev);
    if (tr) std::fclose(tr);
    return 0;
}
