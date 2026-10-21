#include "Ride/HrZoneCoach.hpp"

#include <gtest/gtest.h>

using Ride::HrZoneCoach;
using Alert = HrZoneCoach::Alert;

namespace {
const uint8_t kTh[6] = {95, 114, 133, 152, 171, 190};
}

TEST(HrZoneCoach, ZoneBands)
{
    float lo = 0, hi = 0;
    ASSERT_TRUE(HrZoneCoach::zoneBand(1, kTh, 6, lo, hi));
    EXPECT_FLOAT_EQ(lo, 95.0f);
    EXPECT_FLOAT_EQ(hi, 114.0f);
    ASSERT_TRUE(HrZoneCoach::zoneBand(5, kTh, 6, lo, hi));
    EXPECT_FLOAT_EQ(lo, 171.0f);
    EXPECT_FLOAT_EQ(hi, 190.0f);

    EXPECT_FALSE(HrZoneCoach::zoneBand(0, kTh, 6, lo, hi));
    EXPECT_FALSE(HrZoneCoach::zoneBand(6, kTh, 6, lo, hi));
    EXPECT_FALSE(HrZoneCoach::zoneBand(3, kTh, 3, lo, hi));    // Too few thresholds.
    const uint8_t flat[6] = {100, 100, 100, 100, 100, 100};
    EXPECT_FALSE(HrZoneCoach::zoneBand(2, flat, 6, lo, hi));   // Not increasing.
    EXPECT_FALSE(HrZoneCoach::zoneBand(2, nullptr, 6, lo, hi));
}

TEST(HrZoneCoach, ZoneOf)
{
    EXPECT_EQ(HrZoneCoach::zoneOf(80.0f, kTh, 6), 0);
    EXPECT_EQ(HrZoneCoach::zoneOf(95.0f, kTh, 6), 1);
    EXPECT_EQ(HrZoneCoach::zoneOf(120.0f, kTh, 6), 2);
    EXPECT_EQ(HrZoneCoach::zoneOf(150.0f, kTh, 6), 3);
    EXPECT_EQ(HrZoneCoach::zoneOf(160.0f, kTh, 6), 4);
    EXPECT_EQ(HrZoneCoach::zoneOf(185.0f, kTh, 6), 5);
    EXPECT_EQ(HrZoneCoach::zoneOf(205.0f, kTh, 6), 5);  // Above the top: still zone 5.
    EXPECT_EQ(HrZoneCoach::zoneOf(150.0f, kTh, 1), 0);
}

TEST(HrZoneCoach, OffMeansNoAlerts)
{
    HrZoneCoach c;
    c.configure(0, kTh, 6);
    c.start();
    EXPECT_FALSE(c.enabled());
    for (int i = 0; i < 600; ++i) {
        EXPECT_EQ(c.tick(185.0f, true), Alert::None);
    }
}

TEST(HrZoneCoach, InvalidZoneDisables)
{
    HrZoneCoach c;
    c.configure(4, kTh, 3);
    EXPECT_FALSE(c.enabled());
    EXPECT_EQ(c.targetZone(), 0);
}

TEST(HrZoneCoach, InBandStaysQuiet)
{
    HrZoneCoach c;
    c.configure(2, kTh, 6);
    c.start();
    ASSERT_TRUE(c.enabled());
    EXPECT_EQ(c.targetZone(), 2);
    for (int i = 0; i < 600; ++i) {
        EXPECT_EQ(c.tick(124.0f, true), Alert::None);
    }
    EXPECT_EQ(c.zone(), HrZoneCoach::Zone::In);
}

TEST(HrZoneCoach, AboveThenRepeatThrottledThenBackIn)
{
    HrZoneCoach c;
    c.configure(2, kTh, 6);
    c.start();

    int firstAbove = -1;
    int secondAbove = -1;
    for (int i = 0; i < 400; ++i) {
        const Alert a = c.tick(165.0f, true);
        if (a == Alert::Above) {
            if (firstAbove < 0) {
                firstAbove = i;
            } else if (secondAbove < 0) {
                secondAbove = i;
            }
        }
        EXPECT_NE(a, Alert::Below);
        EXPECT_NE(a, Alert::BackIn);
    }
    ASSERT_GE(firstAbove, 0);
    // Not before the SDK's quiet start.
    EXPECT_GE(firstAbove, static_cast<int>(SDK::Workout::ZoneAlerts::kQuietStartSec) - 1);
    ASSERT_GE(secondAbove, 0);
    EXPECT_GE(secondAbove - firstAbove, static_cast<int>(HrZoneCoach::kRepeatSec));

    // Ease off: one BackIn.
    int backIn = 0;
    for (int i = 0; i < 120; ++i) {
        backIn += (c.tick(124.0f, true) == Alert::BackIn) ? 1 : 0;
    }
    EXPECT_EQ(backIn, 1);
}

TEST(HrZoneCoach, CrossingToTheOtherSideAlertsAtOnce)
{
    HrZoneCoach c;
    c.configure(3, kTh, 6);
    c.start();
    int i = 0;
    for (; i < 200; ++i) {
        if (c.tick(165.0f, true) == Alert::Above) {
            break;
        }
    }
    ASSERT_LT(i, 200);
    // Straight down below the band: Below must not wait out kRepeatSec.
    int below = -1;
    for (int j = 0; j < 30; ++j) {
        if (c.tick(110.0f, true) == Alert::Below) {
            below = j;
            break;
        }
    }
    EXPECT_GE(below, 0);
}

TEST(HrZoneCoach, NoSignalHoldsBack)
{
    HrZoneCoach c;
    c.configure(2, kTh, 6);
    c.start();
    for (int i = 0; i < 300; ++i) {
        EXPECT_EQ(c.tick(0.0f, false), Alert::None);
    }
    EXPECT_EQ(c.zone(), HrZoneCoach::Zone::NoSignal);
}
