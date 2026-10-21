#include <gui/containers/TrackFaceIntervals.hpp>
#include <SDK/GUI/Color.hpp>
#include "Ride/IntervalSession.hpp"

TrackFaceIntervals::TrackFaceIntervals()
{
    setWidth(240);
    setHeight(240);

    mPhase.init(*this, 30, 14, 180, 32, T_TMP_SEMIBOLD_25);
    // Poppins SemiBold 60 is narrowed to digits, ':' and space: fine for times.
    mTime.init(*this, 20, 46, 200, 78, T_TMP_SEMIBOLD_60);
    mHint.init(*this, 30, 124, 180, 23, T_TMP_ITALIC_18);
    RideUi::divider(*this, mLine, mLinePainter, 152);

    mSpeed.init(*this, 30, 158, 95, 36, T_TMP_SEMIBOLD_30);
    mHr.init(*this, 125, 158, 85, 36, T_TMP_SEMIBOLD_30);
    mSpeedUnit.init(*this, 30, 193, 95, 23, T_TMP_ITALIC_18);
    mHrUnit.init(*this, 125, 193, 85, 23, T_TMP_ITALIC_18);
}

void TrackFaceIntervals::setData(const Ride::Data& ride, const Track::Data& track, bool isImperial)
{
    using Phase = Ride::IntervalSession::Phase;
    const auto phase = static_cast<Phase>(ride.ivPhase);
    char s[24];

    // Phase and effort number.
    switch (phase) {
    case Phase::Work:
    case Phase::Rest:
        std::snprintf(s, sizeof(s), "%s %u/%u", Ride::IntervalSession::phaseName(phase),
                      static_cast<unsigned>(ride.ivRep), static_cast<unsigned>(ride.ivReps));
        break;
    default:
        std::snprintf(s, sizeof(s), "%s", Ride::IntervalSession::phaseName(phase));
        break;
    }
    mPhase.set(s);
    uint32_t color = SDK::GUI::Color::WHITE;
    switch (phase) {
    case Phase::Work:     color = SDK::GUI::Color::YELLOW_DARK; break;
    case Phase::Rest:     color = SDK::GUI::Color::CYAN;        break;
    case Phase::WarmUp:
    case Phase::CoolDown: color = SDK::GUI::Color::CORNFLOWER;  break;
    case Phase::Done:     color = SDK::GUI::Color::GREEN;       break;
    default: break;
    }
    mPhase.setColor(RideUi::rgb(color));

    // Time: what is left of a timed step, what has passed of an open one.
    const bool open = (phase == Phase::WarmUp || phase == Phase::CoolDown || phase == Phase::Done);
    RideUi::duration(s, sizeof(s), open ? ride.ivStepSec : ride.ivRemainingSec);
    mTime.set(s);

    switch (phase) {
    case Phase::WarmUp:   mHint.set("Lap: start efforts"); break;
    case Phase::CoolDown: mHint.set("Lap: end cool-down"); break;
    case Phase::Done:     mHint.set("Session complete");   break;
    case Phase::Work:
    case Phase::Rest:     mHint.set("Lap: skip step");     break;
    default:              mHint.set("");                   break;
    }

    // Speed.
    const float kmh = track.speed * 3.6f;
    const float v   = isImperial ? kmh / 1.609344f : kmh;
    if (v <= 0.0f) {
        mSpeed.set("---");
    } else {
        RideUi::fixed1(s, sizeof(s), v);
        mSpeed.set(s);
    }
    mSpeedUnit.set(isImperial ? "mi/h" : "km/h");

    // Heart rate and its zone.
    if (track.hr < 20.0f) {
        mHr.set("---");
        mHrUnit.set("HR");
    } else {
        std::snprintf(s, sizeof(s), "%ld", static_cast<long>(track.hr + 0.5f));
        mHr.set(s);
        if (ride.hrZone > 0) {
            std::snprintf(s, sizeof(s), "HR Z%u", static_cast<unsigned>(ride.hrZone));
            mHrUnit.set(s);
        } else {
            mHrUnit.set("HR");
        }
    }
}
