#ifndef RIDEBANNER_HPP
#define RIDEBANNER_HPP

#include <gui/containers/RideUi.hpp>
#include <touchgfx/widgets/Box.hpp>
#include <gui/model/Model.hpp>

/**
 * @brief A banner across the middle of the track screen for one-off Ride
 *        events: a climb starting or topping out, leaving or rejoining the
 *        route, the heart rate leaving its zone, the next interval step.
 *
 * The service plays the buzz; this only says what it was about, for a few
 * seconds, over whichever face is showing.
 */
class RideBanner : public touchgfx::Container
{
public:
    static constexpr uint16_t kShowSeconds = 4;

    RideBanner();

    void show(const RideAlertInfo& alert, bool isImperial);

    /// Call once a frame; hides the banner when its time is up.
    void tick(uint32_t frameRate);

private:
    touchgfx::Box    mBox;
    RideUi::Text<24> mTitle;
    RideUi::Text<64> mDetail;
    static constexpr uint32_t kArmed = 0xFFFFFFFFu;
    uint32_t         mFramesLeft = 0;
};

#endif // RIDEBANNER_HPP
