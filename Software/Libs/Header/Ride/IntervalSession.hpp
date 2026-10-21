/**
 ******************************************************************************
 * @file    IntervalSession.hpp
 * @brief   Timed on-bike interval sessions, from a short list of presets.
 *
 * A preset becomes an SDK::Workout::IntervalsSpec -- an open warm-up, N work
 * and rest blocks, and an open cool-down -- which the SDK builds into a
 * program and runs on SDK::Workout::Engine. The warm-up and cool-down are
 * open: the rider presses the lap button when ready to start the first effort,
 * and again to end the cool-down portion. Every step that ends becomes a lap
 * in the FIT file (the app does that from ended()).
 *
 * Intervals are time-based on a bike: distance per effort depends too much on
 * terrain and wind to be a useful target.
 ******************************************************************************
 */

#ifndef RIDE_INTERVAL_SESSION_HPP
#define RIDE_INTERVAL_SESSION_HPP

#include "SDK/Workout/Intervals.hpp"
#include "SDK/Workout/WorkoutAlerts.hpp"
#include "SDK/Workout/WorkoutEngine.hpp"
#include "SDK/Workout/WorkoutProgram.hpp"

#include <cstdint>

namespace Ride {

class IntervalSession {
public:
    using Change = SDK::Workout::Engine::Change;

    enum class Phase : uint8_t { None, WarmUp, Work, Rest, CoolDown, Done };

    struct Preset {
        const char* label;    ///< ASCII, fits a menu tip ("5x 3:00/2:00").
        uint8_t     repeats;
        uint16_t    workSec;
        uint16_t    restSec;
    };

    // Common bike interval formats: VO2max, threshold, short and very short
    // efforts, and sweet spot. Labels are ASCII so every font renders them.
    // Inline (C++17) so the GUI can show them without linking the session.
    static constexpr uint8_t kPresetCount = 5;
    static constexpr Preset  kPresets[kPresetCount] = {
        {"5x 3:00/2:00",  5, 180, 120},
        {"4x 8:00/4:00",  4, 480, 240},
        {"8x 1:00/1:00",  8,  60,  60},
        {"10x 0:30/0:30", 10, 30,  30},
        {"3x12:00/5:00",  3, 720, 300},
    };

    /// What one update() produced.
    struct Tick {
        Change change = Change::None;  ///< A step ended; see ended() and endedPhase().
        bool   beep   = false;         ///< Countdown beep for the step's last seconds.
    };

    /**
     * Start preset @p preset (clamped to the list) with the activity's active
     * totals now, normally 0.
     */
    void start(uint8_t preset, uint32_t timeMs, uint32_t distanceCm);

    /// Stop following the session (the ride ended or was discarded).
    void stop();

    bool active() const { return mActive; }

    /// One active tick with the activity's active totals.
    Tick update(uint32_t timeMs, uint32_t distanceCm);

    /// The lap button: end the current step now. Returns the change.
    Change next(uint32_t timeMs, uint32_t distanceCm);

    // -- What to show ---------------------------------------------------------

    Phase    phase() const;
    uint8_t  preset() const { return mPreset; }
    /// Effort number to show (1..repeats), 0 during the warm-up.
    uint32_t rep() const;
    uint32_t reps() const { return mSpec.repeats; }
    /// Seconds left in a timed step, rounded up; 0 for an open step.
    uint32_t remainingSec() const;
    /// Active seconds in the current step.
    uint32_t stepSec() const { return mEngine.status().stepTimeMs / 1000u; }
    bool     openStep() const;

    // -- The step that just ended ---------------------------------------------

    const SDK::Workout::Engine::Ended& ended() const { return mEngine.ended(); }
    Phase endedPhase() const { return phaseOf(mEngine.ended().step); }

    /// Upper-case name of a phase, for screens. Inline: the GUI process uses
    /// it without linking the rest of the session.
    static const char* phaseName(Phase p)
    {
        switch (p) {
        case Phase::WarmUp:   return "WARM UP";
        case Phase::Work:     return "WORK";
        case Phase::Rest:     return "REST";
        case Phase::CoolDown: return "COOL DOWN";
        case Phase::Done:     return "DONE";
        case Phase::None:
        default:              return "";
        }
    }

private:
    Phase phaseOf(uint16_t step) const;

    bool    mActive = false;
    uint8_t mPreset = 0;

    SDK::Workout::IntervalsSpec   mSpec;
    SDK::Workout::IntervalsLayout mLayout;
    SDK::Workout::Program         mProgram;  ///< ~15 KB: keep the session long-lived.
    SDK::Workout::Engine          mEngine;
    SDK::Workout::LeadInAlert     mLeadIn;
};

}  // namespace Ride

#endif  // RIDE_INTERVAL_SESSION_HPP
