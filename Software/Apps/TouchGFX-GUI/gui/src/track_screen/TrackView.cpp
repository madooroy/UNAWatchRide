#include <gui/track_screen/TrackView.hpp>
#include <SDK/Utils/Utils.hpp>
#include <cstring>

using FaceId = App::MenuNav::TrackView::Id;

TrackView::TrackView()
{
    setRideFaces(false, false);
}

void TrackView::setupScreen()
{
    TrackViewBase::setupScreen();

    buttons.setL1(Buttons::NONE);
    buttons.setL2(Buttons::NONE);
    buttons.setR1(Buttons::NONE);
    buttons.setR2(Buttons::AMBER);

    // The Ride faces sit with the Designer faces, under the pause banner.
    mFaceClimb.setVisible(false);
    mFaceRoute.setVisible(false);
    mFaceIntervals.setVisible(false);
    insert(&trackFaceTotal, mFaceIntervals);
    insert(&trackFaceTotal, mFaceRoute);
    insert(&trackFaceTotal, mFaceClimb);
    add(mBanner);  // Over everything.

    scrollIndicator.setConfig(ScrollIndicator::kSmall);
    scrollIndicator.setCount(mFaceCount);
    mScreenReady = true;

    // The banner is an overlay on whichever face is showing, so its visibility
    // survives face changes and is driven only by the track state.
    pauseIndicator.setVisible(mPaused);
}

void TrackView::tearDownScreen()
{
    TrackViewBase::tearDownScreen();
}

void TrackView::setRideFaces(bool intervals, bool route)
{
    // Order: the interval countdown first when there is a session, then the
    // ride numbers, the climb face, the route, the lap, and the clock.
    mFaceCount = 0;
    if (intervals) {
        mFaces[mFaceCount++] = FaceId::ID_INTERVALS;
    }
    mFaces[mFaceCount++] = FaceId::ID_TRACK1;
    mFaces[mFaceCount++] = FaceId::ID_TRACK2;
    mFaces[mFaceCount++] = FaceId::ID_CLIMB;
    if (route) {
        mFaces[mFaceCount++] = FaceId::ID_ROUTE;
    }
    mFaces[mFaceCount++] = FaceId::ID_TRACK3;
    mFaces[mFaceCount++] = FaceId::ID_TRACK4;

    // The constructor builds the default list before setupScreen() has set
    // the indicator up; it takes the count there.
    if (mScreenReady) {
        scrollIndicator.setCount(mFaceCount);
        scrollIndicator.invalidate();
    }
}

void TrackView::showFace(uint16_t id)
{
    touchgfx::Drawable* faces[] = {
        &trackFaceTotal, &trackFaceOverview, &trackFaceLap, &trackFaceStatus,
        &mFaceClimb, &mFaceRoute, &mFaceIntervals,
    };
    static_assert(sizeof(faces) / sizeof(faces[0]) == FaceId::ID_COUNT, "one face per id");
    for (uint16_t i = 0; i < FaceId::ID_COUNT; ++i) {
        const bool show = (i == id);
        if (faces[i]->isVisible() != show) {
            faces[i]->setVisible(show);
            faces[i]->invalidate();
        }
    }
}

void TrackView::setPositionId(uint16_t id)
{
    // id is a face (FaceId). A face this ride does not have (the route face
    // after the route was switched off, say) falls back to the first one.
    uint16_t index = 0;
    for (uint16_t i = 0; i < mFaceCount; ++i) {
        if (mFaces[i] == id) {
            index = i;
            break;
        }
    }
    mCurrentFaceId = mFaces[index];

    scrollIndicator.setActiveId(index);
    showFace(mCurrentFaceId);

    // A face shows its latest data straight away rather than on the next tick.
    setRideData(mRide);
}

uint16_t TrackView::getPositionId()
{
    return mCurrentFaceId;
}

void TrackView::setConfig(bool isImperial, const uint8_t* thresholds, uint8_t thresholdCount)
{
    mIsImperial = isImperial;
    mHrThresholdCount = thresholdCount < App::Config::kHrThresholdsCount ?
        thresholdCount : static_cast<uint8_t>(App::Config::kHrThresholdsCount);
    memcpy(mHrThresholds, thresholds, mHrThresholdCount);
}

void TrackView::setTimeFormat(bool is12Hour)
{
    mIs12Hour = is12Hour;
}

void TrackView::setTrackData(const Track::Data& data)
{
    auto distConv = [this](float metres) -> float {
        const float km = metres / 1000.0f;
        return mIsImperial ? SDK::Utils::kmToMiles(km) : km;
    };

    auto speedConv = [this](float mPerSec) -> float {
        const float kmPerHour = mPerSec * 3.6f;
        return mIsImperial ? SDK::Utils::kmToMiles(kmPerHour) : kmPerHour;
    };

    auto elevationConv = [this](float metres) -> float {
        return mIsImperial ? SDK::Utils::metersToFeet(metres) : metres;
    };

    trackFaceTotal.setSpeed(speedConv(data.speed), mIsImperial);
    trackFaceTotal.setDistance(distConv(data.distance), mIsImperial);
    trackFaceTotal.setTimer(data.totalTime);

    trackFaceOverview.setHR(data.hr, mHrThresholds, mHrThresholdCount);
    trackFaceOverview.setAvgSpeed(speedConv(data.avgSpeed), mIsImperial);
    trackFaceOverview.setElevation(elevationConv(data.elevation), mIsImperial);

    trackFaceLap.setSpeed(speedConv(data.avgLapSpeed), mIsImperial);
    trackFaceLap.setDistance(distConv(data.lapDistance), mIsImperial);
    trackFaceLap.setTimer(data.lapTime);

    mTrack = data;
    if (mCurrentFaceId == FaceId::ID_INTERVALS) {
        mFaceIntervals.setData(mRide, mTrack, mIsImperial);
    }

    mHrSource = data.hrSource;
    updateHrIcon();
}

void TrackView::setTime(uint8_t h, uint8_t m)
{
    trackFaceStatus.setTime(h, m, mIs12Hour);
}

void TrackView::setBatteryLevel(uint8_t level)
{
    trackFaceStatus.setBatteryLevel(level);
}

void TrackView::handleKeyEvent(uint8_t key)
{
    uint16_t index = 0;
    for (uint16_t i = 0; i < mFaceCount; ++i) {
        if (mFaces[i] == mCurrentFaceId) {
            index = i;
            break;
        }
    }

    if (key == SDK::GUI::Button::L1) {
        index = (index == 0) ? static_cast<uint16_t>(mFaceCount - 1) : static_cast<uint16_t>(index - 1);
        setPositionId(mFaces[index]);
    }

    if (key == SDK::GUI::Button::L2) {
        index = static_cast<uint16_t>((index + 1u) % mFaceCount);
        setPositionId(mFaces[index]);
    }

    if (key == SDK::GUI::Button::R1) {
        application().gotoTrackActionScreenNoTransition();
    }

    if (key == SDK::GUI::Button::R2) {
        presenter->saveLap();
        application().gotoTrackLapScreenNoTransition();
    }
}

void TrackView::setGpsFix(bool state)
{
    trackFaceStatus.setGps(SDK::GUI::SensorStatusRow::gpsState(state));
}

void TrackView::setAccessoryStatus(uint8_t state)
{
    mAccessoryState = state;
    updateHrIcon();
}

void TrackView::setPaused(bool paused)
{
    if (mPaused == paused) {
        return;
    }
    mPaused = paused;

    pauseIndicator.setVisible(paused);
    pauseIndicator.invalidate();
}

void TrackView::setPausedTime(std::time_t seconds)
{
    if (!mPaused) {
        return;
    }
    pauseIndicator.setTime(seconds);
}

void TrackView::updateHrIcon()
{
    // In-activity: icon follows the live HR source, not the raw link state.
    trackFaceStatus.setHr(SDK::GUI::SensorStatusRow::hrStateFromSource(
            mAccessoryState, mHrSource));
}

void TrackView::setRoute(const Ride::RouteShape& shape)
{
    mFaceRoute.setRoute(shape);
}

void TrackView::setRideData(const Ride::Data& data)
{
    mRide = data;
    // Only the face on screen is redrawn; the others catch up when shown.
    switch (mCurrentFaceId) {
    case FaceId::ID_CLIMB:     mFaceClimb.setData(mRide, mIsImperial);             break;
    case FaceId::ID_ROUTE:     mFaceRoute.setData(mRide, mIsImperial);             break;
    case FaceId::ID_INTERVALS: mFaceIntervals.setData(mRide, mTrack, mIsImperial); break;
    default: break;
    }
    trackFaceOverview.setHrTarget(mRide.hrTargetZone, mRide.hrTargetState);
}

void TrackView::showRideAlert(const RideAlertInfo& alert)
{
    mBanner.show(alert, mIsImperial);
}

void TrackView::handleTickEvent()
{
    TrackViewBase::handleTickEvent();
    mBanner.tick(App::Config::kFrameRate);
}
