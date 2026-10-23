#include "Ride/RouteFollower.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <utility>
#include <vector>

using Ride::Route;
using Ride::RouteFollower;
using Event = RouteFollower::Event;

namespace {

constexpr double kLat0 = 40.0;
constexpr double kLon0 = -105.0;
constexpr double kPi   = 3.14159265358979323846;

void at(double east, double north, double& lat, double& lon)
{
    lat = kLat0 + north / Route::kMetresPerDegree;
    lon = kLon0 + east / (Route::kMetresPerDegree * std::cos(kLat0 * kPi / 180.0));
}

/// Build a route through @p corners (east, north), a point every 10 m.
std::unique_ptr<Route> route(const std::vector<std::pair<double, double>>& corners)
{
    auto r = std::make_unique<Route>();
    r->begin();
    double lat, lon;
    for (size_t c = 0; c + 1 < corners.size(); ++c) {
        const auto [e0, n0] = corners[c];
        const auto [e1, n1] = corners[c + 1];
        const double len = std::hypot(e1 - e0, n1 - n0);
        const int    n   = static_cast<int>(len / 10.0);
        for (int i = (c == 0 ? 0 : 1); i <= n; ++i) {
            const double f = static_cast<double>(i) / n;
            at(e0 + f * (e1 - e0), n0 + f * (n1 - n0), lat, lon);
            r->add(lat, lon);
        }
    }
    r->finish();
    return r;
}

Event tickAt(RouteFollower& f, double east, double north)
{
    double lat, lon;
    at(east, north, lat, lon);
    return f.tick(lat, lon, true);
}

}  // namespace

TEST(RouteFollower, TracksProgressAlongAStraightRoute)
{
    auto r = route({{0, 0}, {2000, 0}});
    RouteFollower f(*r);
    f.reset();

    for (int e = 0; e <= 1000; e += 8) {
        EXPECT_EQ(tickAt(f, e, 5.0), Event::None);
    }
    const auto& s = f.status();
    EXPECT_TRUE(s.matched);
    EXPECT_FALSE(s.offCourse);
    EXPECT_NEAR(s.offM, 5.0f, 0.5f);
    EXPECT_NEAR(s.alongM, 1000.0f, 1.0f);
    EXPECT_NEAR(s.remainingM, 1000.0f, 1.0f);
}

TEST(RouteFollower, OffCourseAndBackWithHysteresis)
{
    auto r = route({{0, 0}, {3000, 0}});
    RouteFollower f(*r);
    f.reset();
    for (int e = 0; e <= 500; e += 8) {
        tickAt(f, e, 0);
    }

    // A detour 100 m north: off course only after kOffFixes fixes.
    std::vector<Event> evs;
    for (int i = 1; i <= 20; ++i) {
        evs.push_back(tickAt(f, 500 + i * 8, 100));
    }
    int offAt = -1;
    for (size_t i = 0; i < evs.size(); ++i) {
        if (evs[i] == Event::OffCourse) {
            EXPECT_EQ(offAt, -1);
            offAt = static_cast<int>(i);
        }
    }
    EXPECT_EQ(offAt, RouteFollower::kOffFixes - 1);
    EXPECT_TRUE(f.status().offCourse);

    // 40 m away is between the thresholds: still off.
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(tickAt(f, 700 + i * 8, 40), Event::None);
    }
    EXPECT_TRUE(f.status().offCourse);

    // Back on the road: back on course after kOnFixes.
    int backAt = -1;
    for (int i = 0; i < 10; ++i) {
        if (tickAt(f, 800 + i * 8, 2) == Event::BackOnCourse) {
            backAt = i;
            break;
        }
    }
    EXPECT_EQ(backAt, RouteFollower::kOnFixes - 1);
    EXPECT_FALSE(f.status().offCourse);
    EXPECT_NEAR(f.status().alongM, 800.0f + backAt * 8.0f, 2.0f);
}

TEST(RouteFollower, SingleBadFixDoesNotBuzz)
{
    auto r = route({{0, 0}, {3000, 0}});
    RouteFollower f(*r);
    f.reset();
    for (int i = 0; i < 300; ++i) {
        const double north = (i % 7 == 0) ? 200.0 : 3.0;  // A wild fix every 7 s.
        EXPECT_EQ(tickAt(f, i * 8.0, north), Event::None);
    }
    EXPECT_FALSE(f.status().offCourse);
}

TEST(RouteFollower, OutAndBackKeepsCountingForward)
{
    // Out 1 km east and back along the same road.
    auto r = route({{0, 0}, {1000, 0}, {0, 0.5}});
    RouteFollower f(*r);
    f.reset();
    for (int e = 0; e <= 1000; e += 8) {
        tickAt(f, e, 0);
    }
    for (int e = 1000; e >= 200; e -= 8) {
        tickAt(f, e, 0.4);
    }
    // 1000 out + 800 back, not the 200 m of the outward leg it overlaps.
    EXPECT_NEAR(f.status().alongM, 1800.0f, 15.0f);
    EXPECT_NEAR(f.status().remainingM, 200.0f, 15.0f);
}

TEST(RouteFollower, LoopDoesNotArriveAtTheStartButAtTheEnd)
{
    auto r = route({{0, 0}, {1000, 0}, {1000, 1000}, {0, 1000}, {0, 0}});
    RouteFollower f(*r);
    f.reset();

    EXPECT_EQ(tickAt(f, 0, 0), Event::None);
    EXPECT_NEAR(f.status().alongM, 0.0f, 1.0f);  // The start, not the finish.

    int arrived = 0;
    auto leg = [&](double e0, double n0, double e1, double n1) {
        const int n = static_cast<int>(std::hypot(e1 - e0, n1 - n0) / 8.0);
        for (int i = 1; i <= n; ++i) {
            const double t = static_cast<double>(i) / n;
            arrived += (tickAt(f, e0 + t * (e1 - e0), n0 + t * (n1 - n0)) == Event::Arrived) ? 1 : 0;
        }
    };
    leg(0, 0, 1000, 0);
    leg(1000, 0, 1000, 1000);
    leg(1000, 1000, 0, 1000);
    EXPECT_EQ(arrived, 0);
    leg(0, 1000, 0, 0);
    EXPECT_EQ(arrived, 1);
    EXPECT_TRUE(f.status().arrived);
    EXPECT_NEAR(f.status().remainingM, 0.0f, RouteFollower::kFinishM);
}

TEST(RouteFollower, NoOffCourseAlertsAfterArriving)
{
    // Ride a 2 km route to its end, then keep riding away from it (home).
    auto r = route({{0, 0}, {2000, 0}});
    RouteFollower f(*r);
    f.reset();
    int arrived = 0, alerts = 0;
    for (int e = 0; e <= 4000; e += 8) {
        const double n = e <= 2000 ? 0.0 : (e - 2000) * 0.5;   // turn off the end of the route
        const Event ev = tickAt(f, e <= 2000 ? e : 2000 + (e - 2000) * 0.8, n);
        arrived += ev == Event::Arrived;
        alerts  += ev == Event::OffCourse || ev == Event::BackOnCourse;
    }
    EXPECT_EQ(arrived, 1);
    EXPECT_EQ(alerts, 0);
    EXPECT_FALSE(f.status().offCourse);
}

TEST(RouteFollower, StartingPartWayAlong)
{
    auto r = route({{0, 0}, {5000, 0}});
    RouteFollower f(*r);
    f.reset();
    tickAt(f, 3200, 10);
    EXPECT_NEAR(f.status().alongM, 3200.0f, 1.0f);
}

TEST(RouteFollower, ShortcutReacquiresAhead)
{
    // A U: 2 km east, 500 m north, 2 km west. The rider cuts across the top.
    auto r = route({{0, 0}, {2000, 0}, {2000, 500}, {0, 500}});
    RouteFollower f(*r);
    f.reset();
    for (int e = 0; e <= 1000; e += 8) {
        tickAt(f, e, 0);
    }
    for (int n = 0; n <= 500; n += 8) {  // Cut north across the middle.
        tickAt(f, 1000, n);
    }
    for (int i = 0; i < 5; ++i) {
        tickAt(f, 1000 - i * 8, 500);
    }
    EXPECT_FALSE(f.status().offCourse);
    EXPECT_NEAR(f.status().alongM, 2000.0f + 500.0f + 1000.0f + 32.0f, 20.0f);
}

TEST(RouteFollower, NoFixChangesNothing)
{
    auto r = route({{0, 0}, {1000, 0}});
    RouteFollower f(*r);
    f.reset();
    tickAt(f, 100, 0);
    const float along = f.status().alongM;
    for (int i = 0; i < 50; ++i) {
        EXPECT_EQ(f.tick(0.0, 0.0, false), Event::None);
    }
    EXPECT_FLOAT_EQ(f.status().alongM, along);
}

TEST(RouteFollower, InvalidRouteDoesNothing)
{
    Route r;  // Never built.
    RouteFollower f(r);
    f.reset();
    double lat, lon;
    at(0, 0, lat, lon);
    EXPECT_EQ(f.tick(lat, lon, true), Event::None);
    EXPECT_FALSE(f.status().matched);
}

TEST(RouteFollower, LapsOfAShortCircuitAreCountedInOrder)
{
    // Three laps of a 400 m circuit: the search window reaches a whole lap
    // ahead, so every point of the road is "nearest" three times over.
    std::vector<std::pair<double, double>> corners;
    for (int lap = 0; lap < 3; ++lap) {
        corners.push_back({0, 0});
        corners.push_back({150, 0});
        corners.push_back({150, 50});
        corners.push_back({0, 50});
    }
    corners.push_back({0, 0});
    auto r = route(corners);
    RouteFollower f(*r);
    f.reset();

    int arrived = 0;
    auto lap = [&](double noise) {
        const double pts[5][2] = {{0, 0}, {150, 0}, {150, 50}, {0, 50}, {0, 0}};
        for (int c = 0; c < 4; ++c) {
            const double e0 = pts[c][0], n0 = pts[c][1], e1 = pts[c + 1][0], n1 = pts[c + 1][1];
            const int n = static_cast<int>(std::hypot(e1 - e0, n1 - n0) / 7.0);
            for (int i = 1; i <= n; ++i) {
                const double t = static_cast<double>(i) / n;
                const double jitter = ((i * 7919) % 5 - 2) * noise;  // Deterministic +-2*noise m.
                arrived += (tickAt(f, e0 + t * (e1 - e0) + jitter, n0 + t * (n1 - n0) - jitter)
                            == Event::Arrived) ? 1 : 0;
            }
        }
    };
    lap(1.5);
    EXPECT_NEAR(f.status().alongM, 400.0f, 15.0f);
    EXPECT_EQ(arrived, 0);
    lap(1.5);
    EXPECT_NEAR(f.status().alongM, 800.0f, 15.0f);
    EXPECT_EQ(arrived, 0);
    lap(1.5);
    EXPECT_EQ(arrived, 1);
}

TEST(RouteFollower, FastRiderIsNotHeldBack)
{
    // 20 m per fix (72 km/h): progress keeps up and the distance off stays true.
    auto r = route({{0, 0}, {5000, 0}});
    RouteFollower f(*r);
    f.reset();
    for (int e = 0; e <= 3000; e += 20) {
        tickAt(f, e, 3.0);
        EXPECT_NEAR(f.status().alongM, static_cast<float>(e), 1.0f);
        EXPECT_NEAR(f.status().offM, 3.0f, 0.5f);
    }
}

TEST(RouteFollower, ShortOutAndBackSpur)
{
    // A 60 m dead-end spur ridden out and back, inside the search window.
    auto r = route({{0, 0}, {500, 0}, {500, 60}, {500.5, 0}, {1000, 0}});
    RouteFollower f(*r);
    f.reset();
    for (int e = 0; e <= 500; e += 6) tickAt(f, e, 0);
    for (int n = 0; n <= 60; n += 6) tickAt(f, 500, n);
    for (int n = 60; n >= 0; n -= 6) tickAt(f, 500.3, n);
    EXPECT_NEAR(f.status().alongM, 620.0f, 12.0f);
    for (int e = 500; e <= 800; e += 6) tickAt(f, e, 0);
    EXPECT_NEAR(f.status().alongM, 920.0f, 12.0f);
}

// ---- Behaviours found by replaying real rides (Tools/replay) ----------------

TEST(RouteFollower, RidingFromTheCarParkToTheRouteIsQuiet)
{
    // Ride starts 70 m off the route and takes 40 s to reach it.
    auto r = route({{0, 0}, {3000, 0}});
    RouteFollower f(*r);
    f.reset();
    for (int i = 0; i <= 40; ++i) {
        EXPECT_EQ(tickAt(f, 200 + i * 2.0, 70.0 - i * 1.75), Event::None) << i;
    }
    for (int e = 290; e < 1000; e += 8) {
        EXPECT_EQ(tickAt(f, e, 2), Event::None);
    }
    EXPECT_TRUE(f.status().joined);
    EXPECT_FALSE(f.status().offCourse);
}

TEST(RouteFollower, FinishingBesideTheEndArrives)
{
    // The ride ends in a car park 45 m beside the route's end, never within
    // kFinishM of it along the route.
    auto r = route({{0, 0}, {5000, 0}});
    RouteFollower f(*r);
    f.reset();
    for (int e = 0; e <= 4900; e += 8) {
        tickAt(f, e, 0);
    }
    int arrived = 0;
    for (int n = 0; n <= 45; n += 5) {
        arrived += tickAt(f, 4950, n) == Event::Arrived ? 1 : 0;
    }
    EXPECT_EQ(arrived, 1);
    EXPECT_TRUE(f.status().arrived);
}

TEST(RouteFollower, NothingAfterArrival)
{
    auto r = route({{0, 0}, {2000, 0}});
    RouteFollower f(*r);
    f.reset();
    int arrived = 0;
    for (int e = 0; e <= 2000; e += 8) {
        arrived += tickAt(f, e, 0) == Event::Arrived ? 1 : 0;
    }
    ASSERT_EQ(arrived, 1);
    // Riding away from the finish to the car: no off-course alert.
    for (int n = 0; n <= 400; n += 8) {
        EXPECT_EQ(tickAt(f, 2000, n), Event::None);
    }
    EXPECT_FLOAT_EQ(f.status().remainingM, 0.0f);
}

TEST(RouteFollower, LoopJoinedPartWayFinishesWhereItJoined)
{
    // A 6 km rectangular loop, joined 2.3 km round (parked on the circuit).
    auto r = route({{0, 0}, {2000, 0}, {2000, 1000}, {0, 1000}, {0, 0}});
    RouteFollower f(*r);
    f.reset();
    ASSERT_TRUE(f.closed());
    int arrived = 0;
    double e = 2000, n = 300;
    auto go = [&](double e1, double n1) {
        const int steps = static_cast<int>(std::hypot(e1 - e, n1 - n) / 7.0);
        for (int i = 1; i <= steps; ++i) {
            const double t = static_cast<double>(i) / steps;
            arrived += tickAt(f, e + t * (e1 - e), n + t * (n1 - n)) == Event::Arrived ? 1 : 0;
        }
        e = e1;
        n = n1;
    };
    go(2000, 1000);
    go(0, 1000);
    go(0, 0);  // The route's own start/finish: not this ride's finish.
    EXPECT_EQ(arrived, 0);
    EXPECT_NEAR(f.status().remainingM, 2300.0f, 60.0f);
    go(2000, 0);
    go(2000, 300);
    EXPECT_EQ(arrived, 1);
}

TEST(RouteFollower, TurningBackEarlyOnAnOutAndBackFollowsTheReturn)
{
    // Out 3 km east and back on the same road. The rider turns back after
    // 1 km: they are on the route's return leg, outside the search window,
    // going its way. Not off course, and the return leg's distance to go.
    auto r = route({{0, 0}, {3000, 0}, {0, 0.5}});
    RouteFollower f(*r);
    f.reset();
    for (int e = 0; e <= 1000; e += 7) {
        EXPECT_EQ(tickAt(f, e, 0), Event::None);
    }
    int off = 0;
    for (int e = 1000; e >= 500; e -= 7) {
        off += tickAt(f, e, 0.4) == Event::OffCourse ? 1 : 0;
    }
    EXPECT_EQ(off, 0);
    EXPECT_NEAR(f.status().alongM, 5500.0f, 30.0f);
    EXPECT_NEAR(f.status().remainingM, 500.0f, 30.0f);
}

TEST(RouteFollower, JumpingToAnotherPassIsNotProgress)
{
    // A loop that uses one road twice, in opposite directions. Re-identifying
    // which pass the rider is on must not add the gap between the passes to
    // the distance ridden.
    auto r = route({{0, 0}, {1000, 0}, {1000, 1000}, {2000, 1000}, {2000, 2000},
                    {1000.5, 2000}, {1000.5, 0.5}, {0.5, 0.5}, {0, 0}});
    RouteFollower f(*r);
    f.reset();
    for (int e = 0; e <= 1000; e += 7) {
        tickAt(f, e, 0);
    }
    for (int n = 0; n <= 600; n += 7) {
        tickAt(f, 1000, n);
    }
    EXPECT_NEAR(f.status().riddenM, 1600.0f, 40.0f);
}

TEST(RouteFollower, WobbleJustAfterRejoiningBuzzesOnce)
{
    // Off course, back on, then off again 17 s later for 15 s: no second
    // alert. Off again for good after that: alerted once the minute is up.
    auto r = route({{0, 0}, {5000, 0}});
    RouteFollower f(*r);
    f.reset();
    double e = 0;
    std::vector<Event> evs;
    auto ride = [&](int secs, double north) {
        for (int i = 0; i < secs; ++i, e += 6) {
            const Event ev = tickAt(f, e, north);
            if (ev != Event::None) evs.push_back(ev);
        }
    };
    ride(100, 0);
    ride(20, 100);   // off: alerted
    ride(20, 0);     // back on
    ride(15, 70);    // wobble inside the hold: quiet
    ride(10, 0);
    ride(40, 100);   // still inside the hold (to 87 s since back on): quiet
    ride(30, 100);   // still off when the hold ends: alerted
    ASSERT_EQ(evs.size(), 3u);
    EXPECT_EQ(evs[0], Event::OffCourse);
    EXPECT_EQ(evs[1], Event::BackOnCourse);
    EXPECT_EQ(evs[2], Event::OffCourse);
}
