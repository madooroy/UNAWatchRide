/**
 ******************************************************************************
 * @file    HrZoneCoach.cpp
 * @brief   See HrZoneCoach.hpp.
 ******************************************************************************
 */

#include "Ride/HrZoneCoach.hpp"

namespace Ride {

bool HrZoneCoach::zoneBand(uint8_t zone, const uint8_t* thresholds, uint8_t count,
                           float& lo, float& hi)
{
    if (thresholds == nullptr || zone < 1 || zone > kZones || count < zone + 1u) {
        return false;
    }
    if (thresholds[zone] <= thresholds[zone - 1]) {
        return false;
    }
    lo = static_cast<float>(thresholds[zone - 1]);
    hi = static_cast<float>(thresholds[zone]);
    return true;
}

uint8_t HrZoneCoach::zoneOf(float bpm, const uint8_t* thresholds, uint8_t count)
{
    if (thresholds == nullptr || count < 2 || bpm < static_cast<float>(thresholds[0])) {
        return 0;
    }
    uint8_t zones = static_cast<uint8_t>(count - 1u);
    if (zones > kZones) {
        zones = kZones;
    }
    for (uint8_t z = 1; z <= zones; ++z) {
        if (bpm < static_cast<float>(thresholds[z])) {
            return z;
        }
    }
    return zones;
}

void HrZoneCoach::configure(uint8_t targetZone, const uint8_t* thresholds, uint8_t count)
{
    mTarget  = targetZone;
    mEnabled = zoneBand(targetZone, thresholds, count, mLo, mHi);
}

void HrZoneCoach::start()
{
    if (mEnabled) {
        mGauge.startStep(mLo, mHi);
    } else {
        mGauge.startStep();
    }
    mAlerts.stepStarted();
    mLastDir   = Alert::None;
    mSinceLast = 0;
}

void HrZoneCoach::resume()
{
    mGauge.resume();
}

HrZoneCoach::Alert HrZoneCoach::tick(float bpm, bool valid)
{
    if (!mEnabled) {
        return Alert::None;
    }

    mGauge.tick(bpm, valid);
    const Alert a = mAlerts.tick(mGauge.zone());

    if (mSinceLast < UINT32_MAX) {
        ++mSinceLast;
    }

    switch (a) {
    case Alert::Above:
    case Alert::Below:
        if (a == mLastDir && mSinceLast < kRepeatSec) {
            return Alert::None;
        }
        mLastDir   = a;
        mSinceLast = 0;
        return a;
    case Alert::BackIn:
        mLastDir = Alert::None;
        return a;
    case Alert::None:
    default:
        return Alert::None;
    }
}

}  // namespace Ride
