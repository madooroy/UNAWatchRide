/**
 ******************************************************************************
 * @file    ClimbDetector.hpp
 * @brief   Detects climbs from distance and barometric altitude, and keeps the
 *          numbers a rider wants while on one: distance, gain, average grade
 *          and VAM.
 *
 * The detector is fed once per *active* second (never while paused) with the
 * activity's active distance and filtered altitude. It knows nothing of
 * sensors, screens or files, so it can be unit-tested on the host.
 *
 * How a climb is found
 * --------------------
 * Samples of (distance, altitude, time) are kept every kSampleSpacingM of
 * distance in a small ring. The window grade is the altitude change over the
 * last kWindowM of distance. Working on distance rather than time keeps the
 * estimate the same whether the rider is crawling or flying, and the window is
 * long enough that the barometer's noise (a metre or so) moves the grade by
 * about 1 %.
 *
 *   IDLE      -> CANDIDATE   window grade >= kStartGradePct over a full window.
 *                            The climb is backdated to the last low point in
 *                            that window, so the first metres are not lost.
 *   CANDIDATE -> CLIMBING    the summit so far is kConfirmGainM above the start
 *                            and kMinDistanceM from it. This is the "climb
 *                            started" event: by then it is a real climb, not a
 *                            bridge or a ramp, and it will be reported.
 *   any       -> IDLE        altitude falls kEndDropM below the highest point,
 *                            or kEndFlatM is ridden since the road last climbed
 *                            at kKeepGradePct. A confirmed climb is reported as
 *                            finished, measured start -> summit.
 *
 * The summit is the highest point up to where the road was last climbing at
 * kKeepGradePct or more. The dip, false flat or gentle drag after it is not
 * part of the climb: replayed on real rides, measuring to the highest point
 * overall let a long 1 % drag stretch short ramps into "climbs" averaging
 * 1-2 %.
 ******************************************************************************
 */

#ifndef RIDE_CLIMB_DETECTOR_HPP
#define RIDE_CLIMB_DETECTOR_HPP

#include <cstdint>

namespace Ride {

/// A finished (or in-progress) climb, start to highest point.
struct ClimbStats {
    float    distanceM   = 0.0f;  ///< Distance from the start to the summit.
    float    gainM       = 0.0f;  ///< Summit altitude minus start altitude.
    float    avgGradePct = 0.0f;  ///< gain / distance, percent.
    uint32_t durationS   = 0;     ///< Active seconds from the start to the summit.
    float    vamMph      = 0.0f;  ///< Vertical ascent rate, metres per hour.
};

class ClimbDetector {
public:
    // -- Tuning -------------------------------------------------------------

    static constexpr float kSampleSpacingM = 10.0f;  ///< Distance between window samples.
    static constexpr float kWindowM        = 100.0f; ///< Grade window length.
    static constexpr float kStartGradePct  = 3.0f;   ///< Window grade that opens a candidate.
    static constexpr float kConfirmGainM   = 15.0f;  ///< Gain that confirms a candidate.
    static constexpr float kMinDistanceM   = 300.0f; ///< Start-to-summit length needed to confirm.
    static constexpr float kEndDropM       = 10.0f;  ///< Drop below the summit that ends a climb.
    static constexpr float kEndFlatM       = 400.0f; ///< Distance below kKeepGradePct that ends a climb.
    static constexpr float kKeepGradePct   = 2.0f;   ///< Window grade that keeps a climb going.
    /// Samples within this of the window's lowest altitude count as its low
    /// point when backdating a climb's start (allows for barometer jitter).
    static constexpr float kLowPointM      = 0.5f;
    /// A VAM needs some climbing time behind it to mean anything.
    static constexpr uint32_t kMinVamSec   = 30;

    enum class Event : uint8_t {
        None,
        Started,   ///< A candidate became a confirmed climb.
        Finished,  ///< A confirmed climb ended; see lastClimb().
    };

    enum class State : uint8_t { Idle, Candidate, Climbing };

    /// Forget everything, including the totals (new activity).
    void reset();

    /**
     * One active second.
     * @param distanceM  Active distance of the activity, metres. Must not go
     *                   backwards; it may stand still.
     * @param altitudeM  Filtered barometric altitude, metres.
     * @param altValid   False when there is no altitude yet; the tick is ignored.
     * @param activeSec  Active seconds of the activity (monotonic).
     */
    Event tick(float distanceM, float altitudeM, bool altValid, uint32_t activeSec);

    State state() const { return mState; }

    /// True while on a confirmed climb.
    bool inClimb() const { return mState == State::Climbing; }

    /// Window grade, percent (positive uphill). Valid once a full window of
    /// distance has been ridden with altitude.
    float gradePct() const { return mGradePct; }
    bool  gradeValid() const { return mGradeValid; }

    /// The climb in progress, start to its highest point so far: the same
    /// span lastClimb() reports when it ends. All zero when not climbing.
    ClimbStats current() const;

    /// The last finished climb.
    const ClimbStats& lastClimb() const { return mLast; }

    uint16_t climbCount() const { return mCount; }      ///< Finished climbs.
    float    climbedGainM() const { return mTotalGain; } ///< Sum of their gains.

private:
    struct Sample {
        float    distanceM;
        float    altitudeM;
        uint32_t sec;
    };

    static constexpr uint8_t kRing = 16;  ///< > kWindowM / kSampleSpacingM + 2.

    void        push(const Sample& s);
    const Sample& at(uint8_t ageFromNewest) const;
    void        updateGrade();
    Event       endClimb();
    static ClimbStats stats(const Sample& start, const Sample& end);

    Sample   mRing[kRing] = {};
    uint8_t  mHead  = 0;      ///< Index of the newest sample.
    uint8_t  mCount8 = 0;     ///< Samples held (<= kRing).

    float    mGradePct   = 0.0f;
    bool     mGradeValid = false;

    State    mState = State::Idle;
    Sample   mStart = {};     ///< Climb start (backdated window start).
    Sample   mPeak  = {};     ///< Highest point since the start.
    Sample   mTop   = {};     ///< The summit: highest point up to mSteep.
    Sample   mSteep = {};     ///< Last point where the window grade was >= kKeepGradePct.
    Sample   mNow   = {};     ///< Latest tick.

    ClimbStats mLast = {};
    uint16_t   mCount = 0;
    float      mTotalGain = 0.0f;
};

}  // namespace Ride

#endif  // RIDE_CLIMB_DETECTOR_HPP
