#include <gui/containers/TrackFaceOverview.hpp>

#include <SDK/GUI/Color.hpp>
#include <SDK/Workout/TargetGauge.hpp>
#include <cstdio>

TrackFaceOverview::TrackFaceOverview()
{
    // Left of the HR value, mirroring the "HR" label on its right.
    mHrTarget.init(*this, 28, 50, 47, 23, T_TMP_MEDIUM_18_R);
}

void TrackFaceOverview::initialize()
{
    TrackFaceOverviewBase::initialize();
}

void TrackFaceOverview::setHR(float hr, const uint8_t* thresholds, uint8_t thresholdCount)
{
    if (hr < App::Display::kMinHR) {
        Unicode::snprintf(hrValueBuffer, HRVALUE_SIZE, "---");
        hrValue.invalidate();
        hrZone.setHR(0, thresholds, thresholdCount);
        return;
    }

    Unicode::snprintfFloat(hrValueBuffer, HRVALUE_SIZE, "%.0f", hr);
    hrValue.invalidate();

    hrZone.setHR(static_cast<uint8_t>(hr), thresholds, thresholdCount);
}

void TrackFaceOverview::setAvgSpeed(float speed, bool isImperial)
{
    if (speed < App::Display::kMinSpeed) {
        Unicode::snprintf(avgSpeedValueBuffer, AVGSPEEDVALUE_SIZE, "---");
    } else if (speed < 10.0f) {
        Unicode::snprintfFloat(avgSpeedValueBuffer, AVGSPEEDVALUE_SIZE, "%.02f", speed);
    } else if (speed < 100.0f) {
        Unicode::snprintfFloat(avgSpeedValueBuffer, AVGSPEEDVALUE_SIZE, "%.01f", speed);
    } else {
        Unicode::snprintfFloat(avgSpeedValueBuffer, AVGSPEEDVALUE_SIZE, "%.0f", speed);
    }

    Unicode::snprintf(avgSpeedUnitsBuffer, AVGSPEEDUNITS_SIZE, "%s",
        touchgfx::TypedText(isImperial ? T_TEXT_MI_PER_H : T_TEXT_KM_PER_H).getText());

    avgSpeedValue.invalidate();
    avgSpeedUnits.invalidate();
}

void TrackFaceOverview::setElevation(float elevation, bool isImperial)
{
    Unicode::snprintf(elevationValueBuffer, ELEVATIONVALUE_SIZE, "%d", static_cast<int32_t>(elevation));

    Unicode::snprintf(elevationUnitsBuffer, ELEVATIONUNITS_SIZE, "%s",
        touchgfx::TypedText(isImperial ? T_TEXT_FT : T_TEXT_M).getText());

    elevationValue.invalidate();
    elevationUnits.invalidate();
}


void TrackFaceOverview::setHrTarget(uint8_t zone, uint8_t state)
{
    if (zone == 0) {
        mHrTarget.set("");
        return;
    }
    char s[8];
    std::snprintf(s, sizeof(s), "Z%u", static_cast<unsigned>(zone));
    mHrTarget.set(s);

    using Zone = SDK::Workout::Zone;
    uint32_t c = SDK::GUI::Color::GRAY;
    switch (static_cast<Zone>(state)) {
    case Zone::In:    c = SDK::GUI::Color::GREEN; break;
    case Zone::Above: c = SDK::GUI::Color::RED;   break;
    case Zone::Below: c = SDK::GUI::Color::SKY;   break;
    default: break;
    }
    mHrTarget.setColor(RideUi::rgb(c));
}
