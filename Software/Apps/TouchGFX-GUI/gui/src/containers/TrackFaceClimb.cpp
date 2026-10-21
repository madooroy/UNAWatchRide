#include <gui/containers/TrackFaceClimb.hpp>
#include <SDK/GUI/Color.hpp>

TrackFaceClimb::TrackFaceClimb()
{
    setWidth(240);
    setHeight(240);

    // Same grid as the Speed/Distance/Timer face.
    mGradeLabel.init(*this, 50, 10, 140, 23, T_TMP_ITALIC_18);
    mGradeValue.init(*this, 60, 30, 110, 42, T_TMP_SEMIBOLD_40);
    mGradeUnit.init(*this, 172, 51, 40, 19, T_TMP_REGULAR_18_L);
    RideUi::divider(*this, mLine1, mLine1Painter, 79);

    mMidLabel.init(*this, 50, 88, 140, 23, T_TMP_ITALIC_18);
    mMidValue.init(*this, 45, 111, 134, 42, T_TMP_SEMIBOLD_35);
    mMidUnit.init(*this, 180, 129, 45, 19, T_TMP_REGULAR_18_L);
    RideUi::divider(*this, mLine2, mLine2Painter, 159);

    mLowValue.init(*this, 45, 162, 134, 42, T_TMP_SEMIBOLD_35);
    mLowUnit.init(*this, 180, 180, 45, 19, T_TMP_REGULAR_18_L);
    mLowLabel.init(*this, 30, 204, 180, 23, T_TMP_ITALIC_18);

    mGradeUnit.set("%");
}

void TrackFaceClimb::setData(const Ride::Data& d, bool isImperial)
{
    char s[24];
    const char* hUnit = isImperial ? "ft" : "m";

    // Live grade.
    if (d.gradeValid) {
        RideUi::fixed1(s, sizeof(s), d.gradePct);
        mGradeValue.set(s);
    } else {
        mGradeValue.set("---");
    }

    if (d.inClimb) {
        mGradeLabel.set("Climbing");
        mGradeLabel.setColor(RideUi::rgb(SDK::GUI::Color::YELLOW_DARK));

        mMidLabel.set("Climb gain");
        RideUi::height(s, sizeof(s), d.climbGainM, isImperial, true);
        mMidValue.set(s);
        mMidUnit.set(hUnit);

        RideUi::distance(s, sizeof(s), d.climbDistM, isImperial);
        mLowValue.set(s);
        mLowUnit.set(isImperial ? "mi" : "km");

        // "Avg 6.1%  VAM 820" (VAM appears once there is enough climbing).
        char avg[8];
        RideUi::fixed1(avg, sizeof(avg), d.climbGradePct);
        if (d.climbVamMph > 0.0f) {
            std::snprintf(s, sizeof(s), "Avg %s%%  VAM %ld", avg,
                          static_cast<long>(d.climbVamMph + 0.5f));
        } else {
            std::snprintf(s, sizeof(s), "Avg %s%%", avg);
        }
        mLowLabel.set(s);
    } else {
        mGradeLabel.set("Grade");
        mGradeLabel.setColor(RideUi::textColor());

        mMidLabel.set("Climbed");
        RideUi::height(s, sizeof(s), d.climbedGainM, isImperial, true);
        mMidValue.set(s);
        mMidUnit.set(hUnit);

        std::snprintf(s, sizeof(s), "%u", static_cast<unsigned>(d.climbCount));
        mLowValue.set(s);
        mLowUnit.set("");
        mLowLabel.set(d.climbCount == 1 ? "Climb" : "Climbs");
    }
}
