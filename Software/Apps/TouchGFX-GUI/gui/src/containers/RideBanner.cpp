#include <gui/containers/RideBanner.hpp>
#include <SDK/GUI/Color.hpp>
#include "Ride/IntervalSession.hpp"

RideBanner::RideBanner()
{
    setPosition(0, 82, 240, 76);
    mBox.setPosition(0, 0, 240, 76);
    mBox.setColor(RideUi::rgb(SDK::GUI::Color::TEAL_DARK));
    add(mBox);
    mTitle.init(*this, 10, 6, 220, 34, T_TMP_SEMIBOLD_25);
    mDetail.init(*this, 10, 42, 220, 26, T_TMP_REGULAR_18);
    setVisible(false);
}

void RideBanner::show(const RideAlertInfo& alert, bool isImperial)
{
    using Kind = Ride::AlertKind;
    char title[24] = "";
    char detail[64] = "";
    uint32_t bg = SDK::GUI::Color::TEAL_DARK;

    char a[16], b[16], c[16];
    switch (alert.kind) {
    case Kind::ClimbStarted:
        std::snprintf(title, sizeof(title), "Climb");
        std::snprintf(detail, sizeof(detail), "Climb face for details");
        bg = SDK::GUI::Color::SIENNA_DARK;
        break;
    case Kind::ClimbFinished:
        RideUi::distance(a, sizeof(a), alert.a, isImperial);
        RideUi::height(b, sizeof(b), alert.b, isImperial, true);
        RideUi::fixed1(c, sizeof(c), alert.c);
        std::snprintf(title, sizeof(title), "Summit");
        std::snprintf(detail, sizeof(detail), "%s %s  %s %s  %s%%", a, isImperial ? "mi" : "km",
                      b, isImperial ? "ft" : "m", c);
        bg = SDK::GUI::Color::SIENNA_DARK;
        break;
    case Kind::OffCourse:
        RideUi::height(a, sizeof(a), alert.a, isImperial, false);
        std::snprintf(title, sizeof(title), "Off course");
        std::snprintf(detail, sizeof(detail), "%s %s from route", a, isImperial ? "ft" : "m");
        bg = SDK::GUI::Color::MAROON;
        break;
    case Kind::BackOnCourse:
        std::snprintf(title, sizeof(title), "Back on route");
        bg = SDK::GUI::Color::GREEN_DARK;
        break;
    case Kind::RouteArrived:
        std::snprintf(title, sizeof(title), "Route complete");
        bg = SDK::GUI::Color::GREEN_DARK;
        break;
    case Kind::HrAbove:
        std::snprintf(title, sizeof(title), "HR too high");
        std::snprintf(detail, sizeof(detail), "Target zone %u - ease off", static_cast<unsigned>(alert.a));
        bg = SDK::GUI::Color::MAROON;
        break;
    case Kind::HrBelow:
        std::snprintf(title, sizeof(title), "HR too low");
        std::snprintf(detail, sizeof(detail), "Target zone %u - push on", static_cast<unsigned>(alert.a));
        bg = SDK::GUI::Color::TEAL_BLUE;
        break;
    case Kind::HrBackIn:
        std::snprintf(title, sizeof(title), "HR in zone %u", static_cast<unsigned>(alert.a));
        bg = SDK::GUI::Color::GREEN_DARK;
        break;
    case Kind::IntervalStep: {
        using Phase = Ride::IntervalSession::Phase;
        const auto phase = static_cast<Phase>(static_cast<int>(alert.a));
        if (phase == Phase::Work || phase == Phase::Rest) {
            std::snprintf(title, sizeof(title), "%s %u/%u", Ride::IntervalSession::phaseName(phase),
                          static_cast<unsigned>(alert.b), static_cast<unsigned>(alert.c));
        } else {
            std::snprintf(title, sizeof(title), "%s", Ride::IntervalSession::phaseName(phase));
        }
        if (phase == Phase::Work) {
            bg = SDK::GUI::Color::SIENNA_DARK;
            std::snprintf(detail, sizeof(detail), "Go!");
        } else if (phase == Phase::Rest) {
            bg = SDK::GUI::Color::TEAL_DARK;
            std::snprintf(detail, sizeof(detail), "Recover");
        } else if (phase == Phase::CoolDown) {
            bg = SDK::GUI::Color::TEAL_BLUE;
            std::snprintf(detail, sizeof(detail), "Efforts done");
        } else if (phase == Phase::Done) {
            bg = SDK::GUI::Color::GREEN_DARK;
            std::snprintf(detail, sizeof(detail), "Session complete");
        }
    } break;
    case Kind::None:
    default:
        return;
    }

    mBox.setColor(RideUi::rgb(bg));
    mTitle.set(title);
    mDetail.set(detail);
    // A one-line banner centres its title.
    mTitle.area.setY(detail[0] ? 6 : 20);
    mFramesLeft = kArmed;  // tick() knows the frame rate and starts the count.
    setVisible(true);
    invalidate();
}

void RideBanner::tick(uint32_t frameRate)
{
    if (!isVisible()) {
        return;
    }
    if (mFramesLeft == kArmed) {
        mFramesLeft = kShowSeconds * frameRate;
    }
    if (mFramesLeft > 0 && --mFramesLeft == 0) {
        invalidate();   // While still visible, so the area redraws.
        setVisible(false);
    }
}
