/**
 ******************************************************************************
 * @file    RouteFollower.hpp
 * @brief   Follows the rider along a Route: distance off it, progress along it,
 *          distance to go, and when they leave and rejoin it.
 *
 * Matching
 * --------
 * Each fix is matched to the route within a window around the last match --
 * a little behind, further ahead. Where the route uses the same road more
 * than once inside that window (laps of a short circuit, the return leg of an
 * out-and-back, a figure of eight), several segments are about equally near;
 * of those, the one whose progress is closest to the rider's progress a
 * second ago wins, so progress stays continuous instead of jumping a lap.
 * Only when the window has nothing within kReacquireM (a detour, or starting
 * part way along) is the whole route searched, preferring a match ahead of
 * the old one.
 *
 * Off course
 * ----------
 * Hysteretic in distance and in time, so GPS scatter on a road beside the
 * route, or one bad fix, does not buzz the rider:
 *   on  -> off  more than kOffM from the route for kOffFixes fixes in a row;
 *   off -> on   within kOnM for kOnFixes fixes in a row.
 * A tick without a fix changes nothing.
 ******************************************************************************
 */

#ifndef RIDE_ROUTE_FOLLOWER_HPP
#define RIDE_ROUTE_FOLLOWER_HPP

#include "Ride/Route.hpp"

#include <cstdint>

namespace Ride {

class RouteFollower {
public:
    static constexpr float   kOffM        = 50.0f;
    static constexpr float   kOnM         = 30.0f;
    static constexpr uint8_t kOffFixes    = 10;
    static constexpr uint8_t kOnFixes     = 3;
    static constexpr float   kReacquireM  = 2.0f * kOffM;
    static constexpr float   kFinishM     = 40.0f;   ///< Within this of the end counts as arrived.
    static constexpr uint16_t kBehindSegs = 10;
    static constexpr uint16_t kAheadSegs  = 80;
    /// Progress expected between fixes when choosing among equally near
    /// segments: a second at a typical riding speed.
    static constexpr float    kExpectedAdvanceM = 6.0f;

    enum class Event : uint8_t {
        None,
        OffCourse,
        BackOnCourse,
        Arrived,  ///< Reached the end of the route (once).
    };

    struct Status {
        bool     matched    = false;  ///< At least one fix has been matched.
        bool     offCourse  = false;
        bool     arrived    = false;
        float    offM       = 0.0f;   ///< Distance from the route, metres.
        float    alongM     = 0.0f;   ///< Progress from the route's start, metres.
        float    remainingM = 0.0f;   ///< Route distance still to ride, metres.
        float    x = 0.0f, y = 0.0f;  ///< Rider position on the route's plane.
        uint16_t segment    = 0;      ///< Segment index of the match.
    };

    explicit RouteFollower(const Route& route) : mRoute(route) {}

    /// Start following from scratch (new activity, or a new route loaded).
    void reset();

    /// One GPS tick. @p fix false: hold the state.
    Event tick(double lat, double lon, bool fix);

    const Status& status() const { return mStatus; }

private:
    struct Match {
        uint16_t seg  = 0;
        float    dist = 0.0f;
        float    along = 0.0f;
    };

    Match best(float x, float y, uint16_t from, uint16_t to) const;
    Match bestNear(float x, float y, uint16_t from, uint16_t to, float expectedAlong) const;

    const Route& mRoute;
    Status       mStatus;
    uint8_t      mOffRun = 0;
    uint8_t      mOnRun  = 0;
};

}  // namespace Ride

#endif  // RIDE_ROUTE_FOLLOWER_HPP
