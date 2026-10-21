#include "Ride/ClimbDetector.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <functional>
#include <vector>

using Ride::ClimbDetector;
using Event = ClimbDetector::Event;

namespace {

struct Ride1Hz {
    ClimbDetector det;
    float         d   = 0.0f;
    uint32_t      sec = 0;
    std::vector<std::pair<Event, float>> events;  ///< Event and the distance it fired at.

    /// Ride @p metres at @p mps with altitude given by @p alt(distance).
    void ride(float metres, float mps, const std::function<float(float)>& alt,
              const std::function<float(uint32_t)>& noise = nullptr)
    {
        const float end = d + metres;
        while (d < end) {
            d += mps;
            ++sec;
            float a = alt(d);
            if (noise) {
                a += noise(sec);
            }
            const Event e = det.tick(d, a, true, sec);
            if (e != Event::None) {
                events.emplace_back(e, d);
            }
        }
    }

    int count(Event e) const
    {
        int n = 0;
        for (const auto& ev : events) {
            n += (ev.first == e) ? 1 : 0;
        }
        return n;
    }
};

/// Piecewise-linear profile: segments of (length m, grade %).
std::function<float(float)> profile(std::vector<std::pair<float, float>> segs, float base = 100.0f)
{
    return [segs, base](float d) {
        float alt = base;
        float at  = 0.0f;
        for (const auto& s : segs) {
            const float len = std::min(s.first, std::max(0.0f, d - at));
            alt += len * s.second / 100.0f;
            at += s.first;
            if (d <= at) {
                break;
            }
        }
        return alt;
    };
}

/// Deterministic barometer-like noise, +-amp metres.
std::function<float(uint32_t)> noise(float amp)
{
    return [amp](uint32_t s) {
        uint32_t x = s * 2654435761u;
        x ^= x >> 13;
        return amp * (static_cast<float>(x % 2001u) / 1000.0f - 1.0f);
    };
}

}  // namespace

TEST(ClimbDetector, FlatRoadIsNotAClimb)
{
    Ride1Hz r;
    r.ride(5000.0f, 8.0f, profile({{10000.0f, 0.0f}}));
    EXPECT_TRUE(r.events.empty());
    EXPECT_TRUE(r.det.gradeValid());
    EXPECT_NEAR(r.det.gradePct(), 0.0f, 0.01f);
    EXPECT_EQ(r.det.climbCount(), 0);
}

TEST(ClimbDetector, BarometerNoiseOnTheFlatDoesNotTrigger)
{
    Ride1Hz r;
    r.ride(10000.0f, 7.0f, profile({{20000.0f, 0.0f}}), noise(0.8f));
    EXPECT_EQ(r.count(Event::Started), 0);
    EXPECT_EQ(r.det.climbCount(), 0);
}

TEST(ClimbDetector, SteadyClimbIsMeasuredStartToSummit)
{
    Ride1Hz r;
    // 1 km flat, 1 km at 6 %, then a descent.
    auto alt = profile({{1000.0f, 0.0f}, {1000.0f, 6.0f}, {2000.0f, -5.0f}});
    r.ride(4000.0f, 4.0f, alt);

    ASSERT_EQ(r.count(Event::Started), 1);
    ASSERT_EQ(r.count(Event::Finished), 1);

    // Confirmed once 15 m are gained: 250 m into the climb.
    EXPECT_NEAR(r.events[0].second, 1250.0f, 20.0f);
    // Ends 10 m below the summit: 200 m into the 5 % descent.
    EXPECT_NEAR(r.events[1].second, 2200.0f, 20.0f);

    const auto& c = r.det.lastClimb();
    EXPECT_NEAR(c.distanceM, 1000.0f, 60.0f);  // Backdated start, sample spacing.
    EXPECT_NEAR(c.gainM, 60.0f, 3.0f);
    EXPECT_NEAR(c.avgGradePct, 6.0f, 0.5f);
    EXPECT_NEAR(static_cast<float>(c.durationS), 250.0f, 20.0f);  // 1000 m at 4 m/s.
    EXPECT_NEAR(c.vamMph, 60.0f * 3600.0f / 250.0f, 70.0f);
    EXPECT_EQ(r.det.climbCount(), 1);
    EXPECT_FALSE(r.det.inClimb());
}

TEST(ClimbDetector, CurrentClimbStatsWhileClimbing)
{
    Ride1Hz r;
    r.ride(1500.0f, 4.0f, profile({{1000.0f, 0.0f}, {2000.0f, 8.0f}}));
    ASSERT_TRUE(r.det.inClimb());
    const auto c = r.det.current();
    EXPECT_NEAR(c.distanceM, 500.0f, 20.0f);
    EXPECT_NEAR(c.gainM, 40.0f, 2.0f);
    EXPECT_NEAR(c.avgGradePct, 8.0f, 0.5f);
    EXPECT_GT(c.vamMph, 0.0f);
    EXPECT_NEAR(r.det.gradePct(), 8.0f, 0.2f);
}

TEST(ClimbDetector, LiveStatsStopAtTheSummitWhileDescending)
{
    // Over the top and 150 m down at 5 % (7.5 m): still "climbing", because
    // the climb only ends 10 m below the summit. The live numbers must be the
    // climb's, not diluted by the descent.
    Ride1Hz r;
    r.ride(2150.0f, 4.0f, profile({{1000.0f, 0.0f}, {1000.0f, 6.0f}, {2000.0f, -5.0f}}));
    ASSERT_TRUE(r.det.inClimb());
    const auto c = r.det.current();
    EXPECT_NEAR(c.distanceM, 1000.0f, 60.0f);
    EXPECT_NEAR(c.gainM, 60.0f, 3.0f);
    EXPECT_NEAR(c.avgGradePct, 6.0f, 0.5f);
    EXPECT_LT(r.det.gradePct(), 0.0f);  // The live grade does show the descent.
}

TEST(ClimbDetector, ShortRampIsIgnored)
{
    Ride1Hz r;
    // A bridge: 150 m at 5 % (7.5 m), then back down.
    r.ride(2000.0f, 6.0f, profile({{500.0f, 0.0f}, {150.0f, 5.0f}, {150.0f, -5.0f}, {2000.0f, 0.0f}}));
    EXPECT_EQ(r.count(Event::Started), 0);
    EXPECT_EQ(r.count(Event::Finished), 0);
}

TEST(ClimbDetector, ConfirmedButShortClimbIsNotReported)
{
    Ride1Hz r;
    // 250 m at 8 % = 20 m: confirmed, but shorter than kMinDistanceM.
    r.ride(2000.0f, 5.0f, profile({{500.0f, 0.0f}, {250.0f, 8.0f}, {300.0f, -8.0f}, {2000.0f, 0.0f}}));
    EXPECT_EQ(r.count(Event::Started), 1);
    EXPECT_EQ(r.count(Event::Finished), 0);
    EXPECT_EQ(r.det.climbCount(), 0);
}

TEST(ClimbDetector, ShortFalseFlatDoesNotSplitAClimb)
{
    Ride1Hz r;
    r.ride(3000.0f, 4.0f,
           profile({{200.0f, 0.0f}, {500.0f, 5.0f}, {200.0f, 0.0f}, {500.0f, 5.0f}, {1000.0f, -6.0f}}));
    EXPECT_EQ(r.count(Event::Started), 1);
    ASSERT_EQ(r.count(Event::Finished), 1);
    EXPECT_NEAR(r.det.lastClimb().gainM, 50.0f, 3.0f);
}

TEST(ClimbDetector, PlateauEndsTheClimbAtTheSummit)
{
    Ride1Hz r;
    r.ride(2000.0f, 5.0f, profile({{100.0f, 0.0f}, {600.0f, 5.0f}, {3000.0f, 0.0f}}));
    ASSERT_EQ(r.count(Event::Finished), 1);
    // Fires kEndFlatM after the summit; the climb itself stops at the summit.
    EXPECT_NEAR(r.events.back().second, 700.0f + ClimbDetector::kEndFlatM, 20.0f);
    EXPECT_NEAR(r.det.lastClimb().distanceM, 600.0f, 60.0f);
}

TEST(ClimbDetector, TotalsAcrossClimbs)
{
    Ride1Hz r;
    auto alt = profile({{300.0f, 0.0f}, {800.0f, 5.0f}, {800.0f, -5.0f}, {300.0f, 0.0f},
                        {600.0f, 7.0f}, {800.0f, -6.0f}});
    r.ride(3600.0f, 5.0f, alt);
    EXPECT_EQ(r.det.climbCount(), 2);
    EXPECT_NEAR(r.det.climbedGainM(), 40.0f + 42.0f, 5.0f);
}

TEST(ClimbDetector, NoAltitudeIsIgnored)
{
    ClimbDetector det;
    for (uint32_t s = 1; s < 300; ++s) {
        EXPECT_EQ(det.tick(s * 5.0f, s * 1.0f, false, s), Event::None);
    }
    EXPECT_FALSE(det.gradeValid());
    EXPECT_EQ(det.state(), ClimbDetector::State::Idle);
}

TEST(ClimbDetector, StandingStillHoldsState)
{
    Ride1Hz r;
    r.ride(1400.0f, 4.0f, profile({{1000.0f, 0.0f}, {2000.0f, 6.0f}}));
    ASSERT_TRUE(r.det.inClimb());
    // Stopped mid-climb (not paused): distance stands still, altitude too.
    for (int i = 0; i < 600; ++i) {
        ++r.sec;
        EXPECT_EQ(r.det.tick(r.d, 124.0f, true, r.sec), Event::None);
    }
    EXPECT_TRUE(r.det.inClimb());
}

TEST(ClimbDetector, ResetClearsTotals)
{
    Ride1Hz r;
    r.ride(4000.0f, 4.0f, profile({{1000.0f, 0.0f}, {1000.0f, 6.0f}, {2000.0f, -5.0f}}));
    ASSERT_EQ(r.det.climbCount(), 1);
    r.det.reset();
    EXPECT_EQ(r.det.climbCount(), 0);
    EXPECT_FLOAT_EQ(r.det.climbedGainM(), 0.0f);
    EXPECT_FALSE(r.det.gradeValid());
}
