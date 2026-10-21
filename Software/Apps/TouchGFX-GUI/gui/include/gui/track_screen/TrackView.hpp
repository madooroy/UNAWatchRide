#ifndef TRACKVIEW_HPP
#define TRACKVIEW_HPP

#include <gui_generated/track_screen/TrackViewBase.hpp>
#include <gui/track_screen/TrackPresenter.hpp>
#include <gui/containers/TrackFaceClimb.hpp>
#include <gui/containers/TrackFaceRoute.hpp>
#include <gui/containers/TrackFaceIntervals.hpp>
#include <gui/containers/RideBanner.hpp>

class TrackView : public TrackViewBase
{
public:
    TrackView();
    virtual ~TrackView() {}
    virtual void setupScreen();
    virtual void tearDownScreen();

    void setPositionId(uint16_t id);
    uint16_t getPositionId();

    void setConfig(bool isImperial, const uint8_t* thresholds, uint8_t thresholdCount);
    void setTimeFormat(bool is12Hour);
    void setTrackData(const Track::Data& data);

    void setTime(uint8_t h, uint8_t m);
    void setBatteryLevel(uint8_t level);
    void setGpsFix(bool state);
    void setAccessoryStatus(uint8_t state);

    /// Show or hide the paused banner over the current track face.
    void setPaused(bool paused);
    /// Seconds spent in the current pause, rendered in the banner.
    void setPausedTime(std::time_t seconds);

    // -- Ride features --------------------------------------------------------

    /// Which optional faces this ride has. Rebuilds the face list.
    void setRideFaces(bool intervals, bool route);
    void setRoute(const Ride::RouteShape& shape);
    void setRideData(const Ride::Data& data);
    void showRideAlert(const RideAlertInfo& alert);

    virtual void handleTickEvent() override;

protected:

    virtual void handleKeyEvent(uint8_t key) override;

    // In-activity HR icon reflects the live source, recomputed from both the
    // accessory link state (engaged?) and the latest HR-sample source.
    void updateHrIcon();

    /// Show face @p id and hide the others.
    void showFace(uint16_t id);

    TrackFaceClimb     mFaceClimb;
    TrackFaceRoute     mFaceRoute;
    TrackFaceIntervals mFaceIntervals;
    RideBanner         mBanner;

    /// Faces this ride cycles through, in order (FaceId values).
    uint16_t mFaces[App::MenuNav::TrackView::ID_COUNT] = {};
    uint16_t mFaceCount = 0;
    bool     mScreenReady = false;
    Track::Data mTrack {};
    Ride::Data  mRide {};

    uint16_t mCurrentFaceId = 0;
    bool     mIsImperial = false;
    bool     mIs12Hour      = false;
    uint8_t  mHrThresholds[App::Config::kHrThresholdsCount] = {};
    uint8_t  mHrThresholdCount = 0;
    uint8_t  mAccessoryState = 0;  // last SDK::Accessory::State (engaged?)
    uint8_t  mHrSource       = 0;  // last HR-sample source (steady vs flashing)
    bool     mPaused         = false;
};

#endif // TRACKVIEW_HPP
