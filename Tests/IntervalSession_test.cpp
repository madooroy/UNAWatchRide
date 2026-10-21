#include "Ride/IntervalSession.hpp"

#include <gtest/gtest.h>

#include <cstring>

using Ride::IntervalSession;
using Phase  = IntervalSession::Phase;
using Change = IntervalSession::Change;

namespace {

/// Drives a session at 1 Hz with a steady speed, as the service does.
struct Clock {
    IntervalSession& s;
    uint32_t ms = 0;
    uint32_t cm = 0;
    int      beeps = 0;

    IntervalSession::Tick step(uint32_t seconds = 1)
    {
        IntervalSession::Tick last;
        for (uint32_t i = 0; i < seconds; ++i) {
            ms += 1000;
            cm += 800;  // 8 m/s.
            last = s.update(ms, cm);
            beeps += last.beep ? 1 : 0;
            if (last.change != Change::None) {
                return last;
            }
        }
        return last;
    }

    /// Run until a step changes; returns the seconds taken.
    uint32_t untilChange(uint32_t limit = 10000)
    {
        for (uint32_t i = 1; i <= limit; ++i) {
            if (step().change != Change::None) {
                return i;
            }
        }
        return 0;
    }
};

}  // namespace

TEST(IntervalSession, PresetsAreSane)
{
    for (uint8_t i = 0; i < IntervalSession::kPresetCount; ++i) {
        const auto& p = IntervalSession::kPresets[i];
        EXPECT_GT(p.repeats, 0);
        EXPECT_GT(p.workSec, 0);
        EXPECT_GT(p.restSec, 0);
        ASSERT_NE(p.label, nullptr);
        EXPECT_LE(std::strlen(p.label), 14u);
    }
}

TEST(IntervalSession, InactiveByDefault)
{
    IntervalSession s;
    EXPECT_FALSE(s.active());
    EXPECT_EQ(s.phase(), Phase::None);
    EXPECT_EQ(s.update(1000, 100).change, Change::None);
    EXPECT_EQ(s.next(1000, 100), Change::None);
}

TEST(IntervalSession, FullSessionRunsWarmUpEffortsAndCoolDown)
{
    IntervalSession s;
    s.start(2, 0, 0);  // 8x 1:00/1:00
    Clock c{s};

    ASSERT_TRUE(s.active());
    EXPECT_EQ(s.phase(), Phase::WarmUp);
    EXPECT_TRUE(s.openStep());
    EXPECT_EQ(s.rep(), 0u);
    EXPECT_EQ(s.reps(), 8u);

    // The warm-up is open: it waits for the lap button however long it is.
    c.step(600);
    EXPECT_EQ(s.phase(), Phase::WarmUp);
    EXPECT_EQ(s.next(c.ms, c.cm), Change::Manual);
    EXPECT_EQ(s.endedPhase(), Phase::WarmUp);
    EXPECT_EQ(s.ended().timeMs, 600000u);

    for (uint32_t rep = 1; rep <= 8; ++rep) {
        EXPECT_EQ(s.phase(), Phase::Work) << "rep " << rep;
        EXPECT_EQ(s.rep(), rep);
        EXPECT_EQ(s.remainingSec(), 60u);
        c.beeps = 0;
        EXPECT_EQ(c.untilChange(), 60u);
        EXPECT_EQ(s.endedPhase(), Phase::Work);
        EXPECT_EQ(s.ended().how, Change::Auto);
        EXPECT_GE(c.beeps, 4);  // Countdown in the last seconds.
        EXPECT_LE(c.beeps, 7);

        if (rep < 8) {
            EXPECT_EQ(s.phase(), Phase::Rest);
            EXPECT_EQ(s.rep(), rep);
            EXPECT_EQ(c.untilChange(), 60u);
            EXPECT_EQ(s.endedPhase(), Phase::Rest);
        }
    }

    // No rest after the last effort: straight into the open cool-down.
    EXPECT_EQ(s.phase(), Phase::CoolDown);
    EXPECT_TRUE(s.openStep());
    c.step(300);
    EXPECT_EQ(s.phase(), Phase::CoolDown);
    EXPECT_EQ(s.next(c.ms, c.cm), Change::Manual);
    EXPECT_EQ(s.endedPhase(), Phase::CoolDown);
    EXPECT_EQ(s.phase(), Phase::Done);

    // Finished: nothing more happens.
    EXPECT_EQ(s.next(c.ms, c.cm), Change::None);
    EXPECT_EQ(c.step(100).change, Change::None);
}

TEST(IntervalSession, LapButtonCutsAnEffortShort)
{
    IntervalSession s;
    s.start(0, 0, 0);  // 5x 3:00/2:00
    Clock c{s};
    s.next(c.ms, c.cm);  // End warm-up.
    c.step(100);
    EXPECT_EQ(s.remainingSec(), 80u);
    EXPECT_EQ(s.stepSec(), 100u);
    EXPECT_EQ(s.next(c.ms, c.cm), Change::Manual);
    EXPECT_EQ(s.endedPhase(), Phase::Work);
    EXPECT_EQ(s.ended().timeMs, 100000u);
    EXPECT_EQ(s.phase(), Phase::Rest);
    EXPECT_EQ(s.remainingSec(), 120u);
}

TEST(IntervalSession, StartingFromNonZeroTotals)
{
    // A session started part-way into a ride counts from where the ride is.
    IntervalSession s;
    s.start(3, 3600000, 3000000);  // 10x 0:30/0:30, an hour in.
    Clock c{s, 3600000, 3000000};
    s.next(c.ms, c.cm);
    EXPECT_EQ(s.phase(), Phase::Work);
    EXPECT_EQ(c.untilChange(), 30u);
    EXPECT_EQ(s.phase(), Phase::Rest);
}

TEST(IntervalSession, PresetIndexIsClamped)
{
    IntervalSession s;
    s.start(200, 0, 0);
    EXPECT_EQ(s.preset(), IntervalSession::kPresetCount - 1);
}

TEST(IntervalSession, StopEndsIt)
{
    IntervalSession s;
    s.start(1, 0, 0);
    s.stop();
    EXPECT_FALSE(s.active());
    EXPECT_EQ(s.phase(), Phase::None);
}

TEST(IntervalSession, PhaseNames)
{
    EXPECT_STREQ(IntervalSession::phaseName(Phase::Work), "WORK");
    EXPECT_STREQ(IntervalSession::phaseName(Phase::CoolDown), "COOL DOWN");
    EXPECT_STREQ(IntervalSession::phaseName(Phase::None), "");
}
