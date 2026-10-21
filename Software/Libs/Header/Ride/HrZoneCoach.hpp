/**
 ******************************************************************************
 * @file    HrZoneCoach.hpp
 * @brief   Keeps the rider in a chosen heart-rate zone for the whole ride.
 *
 * The zones come from the kernel's heart-rate thresholds (the same ones the
 * HR bar on the track face uses): six boundaries th[0..5] make five zones,
 * zone z (1..5) being [th[z-1], th[z]].
 *
 * The decisions are the SDK's, built for workout steps:
 *   - SDK::Workout::HeartRateGauge turns the live heart rate into a zone with
 *     a deadband and a dwell, and holds off after a start or resume;
 *   - SDK::Workout::ZoneAlerts decides when leaving or re-entering the band is
 *     worth an alert.
 * ZoneAlerts repeats an out-of-band alert every 30 s, which suits a 5-minute
 * interval but nags on a three-hour ride, so repeats in the same direction are
 * held to kRepeatSec here. Crossing to the other side, and coming back in,
 * still alert at once.
 *
 * Tick only while the ride is active; paused seconds are not ticks.
 ******************************************************************************
 */

#ifndef RIDE_HR_ZONE_COACH_HPP
#define RIDE_HR_ZONE_COACH_HPP

#include "SDK/Workout/TargetGauge.hpp"
#include "SDK/Workout/WorkoutAlerts.hpp"

#include <cstdint>

namespace Ride {

class HrZoneCoach {
public:
    using Alert = SDK::Workout::ZoneAlerts::Alert;
    using Zone  = SDK::Workout::Zone;

    static constexpr uint8_t  kZones     = 5;
    static constexpr uint32_t kRepeatSec = 120;  ///< Same-direction repeat interval.

    /**
     * Band [lo, hi] of zone @p zone (1..5) from @p count boundaries.
     * False if the zone is out of range or the thresholds cannot describe it
     * (too few, or not increasing).
     */
    static bool zoneBand(uint8_t zone, const uint8_t* thresholds, uint8_t count,
                         float& lo, float& hi);

    /**
     * The zone @p bpm falls in: 0 below zone 1, 1..5 inside, 5 above the top
     * boundary (an all-out effort is still "zone 5" on a five-zone bar).
     */
    static uint8_t zoneOf(float bpm, const uint8_t* thresholds, uint8_t count);

    /// Choose the target zone (0 = off). Takes effect at the next start().
    void configure(uint8_t targetZone, const uint8_t* thresholds, uint8_t count);

    /// The activity started: start the gauge and the alert timing.
    void start();

    /// The activity resumed after a pause: hold the zone off again.
    void resume();

    /// One active second. @p valid false without a trustworthy reading.
    Alert tick(float bpm, bool valid);

    bool    enabled() const { return mEnabled; }
    uint8_t targetZone() const { return mEnabled ? mTarget : 0; }
    Zone    zone() const { return mGauge.zone(); }

private:
    bool     mEnabled = false;
    uint8_t  mTarget  = 0;
    float    mLo = 0.0f, mHi = 0.0f;

    SDK::Workout::HeartRateGauge mGauge;
    SDK::Workout::ZoneAlerts     mAlerts;

    Alert    mLastDir   = Alert::None;  ///< Above or Below of the last played alert.
    uint32_t mSinceLast = 0;            ///< Ticks since that alert.
};

}  // namespace Ride

#endif  // RIDE_HR_ZONE_COACH_HPP
