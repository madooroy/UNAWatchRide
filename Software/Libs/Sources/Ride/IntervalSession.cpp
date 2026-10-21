/**
 ******************************************************************************
 * @file    IntervalSession.cpp
 * @brief   See IntervalSession.hpp.
 ******************************************************************************
 */

#include "Ride/IntervalSession.hpp"

namespace Ride {

// Common bike interval formats: VO2max, threshold, short and very short
// efforts, and sweet spot. Labels are ASCII so every font can render them.
const IntervalSession::Preset IntervalSession::kPresets[kPresetCount] = {
    {"5x 3:00/2:00",  5, 180, 120},
    {"4x 8:00/4:00",  4, 480, 240},
    {"8x 1:00/1:00",  8,  60,  60},
    {"10x 0:30/0:30", 10, 30,  30},
    {"3x12:00/5:00",  3, 720, 300},
};

void IntervalSession::start(uint8_t preset, uint32_t timeMs, uint32_t distanceCm)
{
    if (preset >= kPresetCount) {
        preset = kPresetCount - 1;
    }
    mPreset = preset;
    const Preset& p = kPresets[preset];

    mSpec = SDK::Workout::IntervalsSpec{};
    mSpec.repeats      = p.repeats;
    mSpec.run.end      = SDK::Workout::IntervalsSpec::Phase::End::Time;
    mSpec.run.timeMs   = static_cast<uint32_t>(p.workSec) * 1000u;
    mSpec.rest.end     = SDK::Workout::IntervalsSpec::Phase::End::Time;
    mSpec.rest.timeMs  = static_cast<uint32_t>(p.restSec) * 1000u;
    mSpec.warmUp       = true;
    mSpec.coolDown     = true;
    mSpec.lastRest     = false;  // Last effort leads straight into the cool-down.

    mLayout = SDK::Workout::buildIntervals(mSpec, mProgram);
    mEngine.start(mProgram, timeMs, distanceCm);
    mLeadIn.stepStarted();
    mActive = true;
}

void IntervalSession::stop()
{
    if (mActive) {
        mEngine.stop();
    }
    mActive = false;
}

IntervalSession::Tick IntervalSession::update(uint32_t timeMs, uint32_t distanceCm)
{
    Tick t;
    if (!mActive || mEngine.status().finished) {
        return t;
    }
    t.change = mEngine.update(timeMs, distanceCm);
    if (t.change != Change::None) {
        // The step change has its own alert; the countdown restarts.
        mLeadIn.stepStarted();
    } else {
        t.beep = mLeadIn.tick(mEngine.status());
    }
    return t;
}

IntervalSession::Change IntervalSession::next(uint32_t timeMs, uint32_t distanceCm)
{
    if (!mActive || mEngine.status().finished) {
        return Change::None;
    }
    const Change c = mEngine.next(timeMs, distanceCm);
    if (c != Change::None) {
        mLeadIn.stepStarted();
    }
    return c;
}

IntervalSession::Phase IntervalSession::phaseOf(uint16_t step) const
{
    if (step >= mProgram.stepCount) {
        return Phase::None;
    }
    switch (mProgram.steps[step].intensity) {
    case SDK::Workout::Intensity::Warmup:   return Phase::WarmUp;
    case SDK::Workout::Intensity::Rest:     return Phase::Rest;
    case SDK::Workout::Intensity::Cooldown: return Phase::CoolDown;
    case SDK::Workout::Intensity::Active:
    default:                                return Phase::Work;
    }
}

IntervalSession::Phase IntervalSession::phase() const
{
    if (!mActive) {
        return Phase::None;
    }
    if (mEngine.status().finished) {
        return Phase::Done;
    }
    return phaseOf(mEngine.status().step);
}

uint32_t IntervalSession::rep() const
{
    if (!mActive) {
        return 0;
    }
    return SDK::Workout::intervalsRepeat(mSpec, mLayout, mEngine.status());
}

bool IntervalSession::openStep() const
{
    const auto& st = mEngine.status();
    if (!mActive || st.finished || st.step >= mProgram.stepCount) {
        return false;
    }
    return mProgram.steps[st.step].end == SDK::Workout::StepEnd::Open;
}

uint32_t IntervalSession::remainingSec() const
{
    const auto& st = mEngine.status();
    if (!mActive || st.finished || st.step >= mProgram.stepCount ||
        mProgram.steps[st.step].end != SDK::Workout::StepEnd::Time) {
        return 0;
    }
    return (st.remainingMs + 999u) / 1000u;
}

const char* IntervalSession::phaseName(Phase p)
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

}  // namespace Ride
