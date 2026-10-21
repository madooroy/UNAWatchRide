#ifndef MODEL_HPP
#define MODEL_HPP

#include "touchgfx/UIEventListener.hpp"

#include <texts/TextKeysAndLanguages.hpp>
#include <images/BitmapDatabase.hpp>

#include "SDK/Kernel/Kernel.hpp"
#include "SDK/Interfaces/IGuiLifeCycleCallback.hpp"
#include "SDK/Interfaces/ICustomMessageHandler.hpp"
#include <SDK/Utils/Utils.hpp>
#include <SDK/GUI/Config.hpp>
#include <SDK/GUI/Color.hpp>
#include <SDK/GUI/Button.hpp>

#include "Commands.hpp"
#include "Settings.hpp"
#include "ActivitySummary.hpp"
#include "Track.hpp"
#include "AppMenu.hpp"
#include "RideData.hpp"
#include "Ride/Route.hpp"


// ---------------------------------------------------------------------------
// App::Config -- application-level constants (timing, frame rate).
// Screens include this transitively via Presenter -> ModelListener -> Model.hpp.
// ---------------------------------------------------------------------------
namespace App::Config
{
constexpr uint32_t kFrameRate = SDK::GUI::Config::kFrameRate;

constexpr uint32_t kMenuAnimationSteps = 4;
constexpr uint32_t kScreenTimeoutSteps = SDK::Utils::secToTicks(30, kFrameRate);  // 30 s

// HR thresholds
constexpr uint8_t kHrThresholdsCount = CustomMessage::kHrThresholdsCount;
} // namespace App::Config

// ---------------------------------------------------------------------------
// App::Display -- minimum valid values for on-screen display.
// Below these thresholds the widget shows "---" instead of a number.
// ---------------------------------------------------------------------------
/// A one-off Ride alert, as the banner shows it.
struct RideAlertInfo {
    Ride::AlertKind kind = Ride::AlertKind::None;
    float a = 0.0f, b = 0.0f, c = 0.0f;
};

namespace App::Display
{
constexpr float kMinDist  = 0.0f;   ///< km or mi  -- negative = no data
constexpr float kMinSpeed = 0.0f;   ///< km/h or mi/h -- negative = no data
constexpr float kMinPace  = 30.0f;  ///< sec/km or sec/mi -- below any human pace
constexpr float kMinHR    = 20.0f;  ///< bpm -- below physiological minimum
} // namespace App::Display


class FrontendApplication;
class ModelListener;

class Model : public touchgfx::UIEventListener,
              public SDK::Interface::IGuiLifeCycleCallback,
              public SDK::Interface::ICustomMessageHandler
{
public:
    Model();

    void bind(ModelListener* listener) { modelListener = listener; }

    // Controls
    FrontendApplication& application();
    App::MenuNav::Nav&   menu();
    void tick();
    void handleKeyEvent(uint8_t key);
    void invalidate();
    void resetIdleTimer();
    void exitApp();

    // Date/Time
    void getDate(uint8_t& month, uint8_t& day, uint8_t& weekday);
    void getTime(uint8_t& h, uint8_t& m, uint8_t& s);

    // Power
    uint8_t getBatteryLevel() const;

    // Settings
    bool isUnitsImperial() const;
    bool is12HourFormat() const;
    const uint8_t* getHrThresholds() const;
    uint8_t        getHrThresholdsCount() const;
    const Settings& getSettings() const;
    void saveSettings(const Settings& sett);

    // GPS
    bool hasGpsFix() const;

    // Latest external-HR link status (SDK::Accessory::State); the kernel only
    // sends on change, so screens read this on activate to show the current icon.
    uint8_t getAccessoryState() const;

    // Ride features
    const Ride::Data&       getRideData() const { return mRide; }
    /// The route drawn for the Route face, inset by kRouteInset pixels;
    /// count 0 without a route.
    const Ride::RouteShape& getRouteShape() const { return mRouteShape; }
    Ride::RouteState        getRouteState() const { return mRouteState; }
    float                   getRouteLengthM() const { return mRouteLengthM; }
    static constexpr uint8_t kRouteInset = 3;   ///< Room for the line's round caps.

    /// How the next start runs: a plain ride, or the interval preset from
    /// Settings. Set before Start, whether or not it then needs confirming.
    void setStartIntervals(bool intervals) { mStartIntervals = intervals; }
    bool getStartIntervals() const { return mStartIntervals; }

    // Track
    void trackStart();
    bool isTrackActive() const;
    void trackPause();
    void trackResume();
    bool isTrackPaused() const;
    /// Seconds elapsed in the current pause, 0 when not paused.
    ///
    /// Lives here rather than in a presenter because presenters are recreated
    /// on every screen transition, and because the Model keeps receiving the
    /// 1 Hz clock regardless of which screen is up -- a pause that spans a
    /// screen change must keep counting.
    std::time_t getPausedSeconds() const;
    const Track::Data& getTrackData() const;
    void saveLap();
    void saveTrack();
    void discardTrack();

    // Hold-to-confirm: which action the shared TrackHoldConfirmation screen performs.
    enum class HoldConfirmMode { Finish, Discard };
    void setHoldConfirmMode(HoldConfirmMode mode);
    HoldConfirmMode getHoldConfirmMode() const;
    bool isTrackSummaryAvailable() const;
    const ActivitySummary& getTrackSummary() const;

private:
    ModelListener*        modelListener;
    const SDK::Kernel&    mKernel;

    // IGuiLifeCycleCallback
    void onStart()   override;
    void onResume()  override;
    void onSuspend() override;
    void onStop()    override;

    // ICustomMessageHandler
    bool customMessageHandler(SDK::MessageBase* message) override;

    void decIdleTimer();
    bool isAnyKeyPressed(uint8_t key) const;

    // State
    bool     mIsRunning  = false;
    bool     mInvalidate = false;
    uint32_t mIdleTimer  = 0;

    App::MenuNav::Nav mMenu {};
    std::tm           mTime {};

    // Settings (mirrored from Service)
    bool    mUnitsImperial     = false;
    bool    mTimeFormat12h     = false;
    uint8_t mHrThresholds[App::Config::kHrThresholdsCount] = {};
    uint8_t mHrThresholdsCount = App::Config::kHrThresholdsCount;
    Settings mSettings {};

    // Kernel state
    bool    mGpsFix         = false;
    uint8_t mBatteryLevel   = 0;
    uint8_t mAccessoryState = 0;   // SDK::Accessory::State (0 = UNAVAILABLE)

    // Track
    HoldConfirmMode        mHoldConfirmMode = HoldConfirmMode::Discard;
    Track::State           mTrackState      {};
    const ActivitySummary* mActivitySummary = nullptr;
    Track::Data            mTrackData       {};

    /// Wall-clock seconds derived from the 1 Hz LOCAL_TIME message, and the
    /// value it held when the current pause began. Differencing timestamps
    /// rather than counting ticks keeps the pause duration honest if the
    /// service coalesces a multi-second gap into one message.
    std::time_t mNowSec        = 0;
    std::time_t mPauseStartSec = 0;

    // Ride features (mirrored from Service)
    Ride::Data       mRide {};
    Ride::RouteShape mRouteShape {};
    Ride::RouteState mRouteState     = Ride::RouteState::None;
    float            mRouteLengthM   = 0.0f;
    bool             mStartIntervals = false;
};

#endif // MODEL_HPP
