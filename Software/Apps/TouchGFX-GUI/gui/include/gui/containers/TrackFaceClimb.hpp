#ifndef TRACKFACECLIMB_HPP
#define TRACKFACECLIMB_HPP

#include <gui/containers/RideUi.hpp>
#include "RideData.hpp"

/**
 * @brief Track face for climbing: live grade, and the climb in progress.
 *
 *   not climbing          on a climb
 *   ------------          ----------
 *   Grade   4.2 %         Climbing  7.8 %   (label turns amber)
 *   Climbed +186 m        Climb gain +42 m
 *   Climbs  3             0.85 km   "Avg 6.1%" <-> "VAM 820 m/h" (alternating)
 */
class TrackFaceClimb : public touchgfx::Container
{
public:
    TrackFaceClimb();

    void setData(const Ride::Data& d, bool isImperial);

private:
    RideUi::Text<16> mGradeLabel, mGradeValue, mGradeUnit;
    RideUi::Text<16> mMidLabel, mMidValue, mMidUnit;
    RideUi::Text<32> mLowLabel;
    RideUi::Text<12> mLowValue, mLowUnit;

    /// setData() calls (one a second) between the two bottom-label texts.
    static constexpr uint32_t kLabelSwapTicks = 3;
    uint32_t mLabelTicks = 0;

    touchgfx::Line             mLine1, mLine2;
    touchgfx::PainterABGR2222  mLine1Painter, mLine2Painter;
};

#endif // TRACKFACECLIMB_HPP
