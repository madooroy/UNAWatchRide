#ifndef TRACKFACEINTERVALS_HPP
#define TRACKFACEINTERVALS_HPP

#include <gui/containers/RideUi.hpp>
#include "RideData.hpp"
#include "Track.hpp"

/**
 * @brief Track face for an interval session.
 *
 *        WORK 3/8          <- phase, coloured: work amber, rest teal
 *         2:41             <- time left in the step (elapsed for an open step)
 *   ------------------
 *    32.4        152       <- speed, heart rate
 *    km/h        HR Z4
 *
 * The warm-up and cool-down are open: the face says to press the lap button.
 */
class TrackFaceIntervals : public touchgfx::Container
{
public:
    TrackFaceIntervals();

    void setData(const Ride::Data& ride, const Track::Data& track, bool isImperial);

private:
    RideUi::Text<20> mPhase;
    RideUi::Text<12> mTime;
    RideUi::Text<24> mHint;
    RideUi::Text<10> mSpeed, mHr;
    RideUi::Text<10> mSpeedUnit, mHrUnit;

    touchgfx::Line            mLine;
    touchgfx::PainterABGR2222 mLinePainter;
};

#endif // TRACKFACEINTERVALS_HPP
