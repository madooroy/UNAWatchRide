/**
 ******************************************************************************
 * @file    RouteFollower.hpp
 * @brief   Follows the rider along a Route: distance off it, progress along it,
 *          distance to go, and when they leave and rejoin it.
 *
 * Matching
 * --------
 * Each fix is matched within a window around the last match, a little behind
 * and further ahead. Where the route uses the same road more than once
 * (laps of a short circuit, out-and-backs, a road ridden both ways), several
 * stretches of route are about equally near; each is a "pass". A pass whose
 * direction agrees with the rider's direction of travel beats one that
 * opposes it, and among those, the pass whose progress is closest to the
 * rider's progress a second ago wins, so progress stays continuous.
 *
 * Whenever the window has nothing within kOnM, the whole route is searched
 * with the same preferences before anything is called off course: a rider
 * on another stretch of the route (a skipped spur, a shortcut back onto it,
 * a road the route uses twice) is on course, wherever the window was.
 *
 * Loops
 * -----
 * A route whose ends are within kClosedM of each other is a loop. Matching
 * wraps around its end, and progress is counted from where the rider joined
 * it, so a ride started part way round a loop (from a car park on the
 * circuit) finishes where it started, kFinishM short of one full lap.
 *
 * Alerts
 * ------
 * Nothing is alerted before the rider first comes within kOnM of the route
 * ("joining" it), so riding from the car to the route is quiet. Then, with
 * hysteresis in distance and time:
 *   on  -> off  more than kOffM from the route for kOffFixes fixes in a row;
 *   off -> on   within kOnM for kOnFixes fixes in a row.
 * Arrival is the finish (the route's end, or the join point on a loop
 * started part way round) reached with at least kArriveMinFraction of the
 * route ridden: within kFinishM of it along the route, or within kArriveM of
 * it as the crow flies. After arrival the follower keeps the rider's
 * position but raises no more events. A tick without a fix changes nothing.
 ******************************************************************************
 */

#ifndef RIDE_ROUTE_FOLLOWER_HPP
#define RIDE_ROUTE_FOLLOWER_HPP

#include "Ride/Route.hpp"

#include <cstdint>

namespace Ride {

class RouteFollower {
public:
    static constexpr float    kOffM        = 50.0f;
    static constexpr float    kOnM         = 30.0f;
    static constexpr uint8_t  kOffFixes    = 10;
    static constexpr uint8_t  kOnFixes     = 3;
    /// After "back on course", no new off-course alert for this many fixes
    /// (90 s at 1 Hz), so a brief wobble buzzes once, not off-on-off-on.
    /// Replayed rides wobbled off again 60-66 s after rejoining, so a minute
    /// was just too short.
    static constexpr uint8_t  kRealertFixes = 90;
    static constexpr float    kFinishM     = 40.0f;   ///< Along the route, from the finish.
    static constexpr float    kArriveM     = 60.0f;   ///< Straight line, from the finish.
    static constexpr float    kArriveMinFraction = 0.75f;  ///< Of the route ridden before arrival counts.
    /// Arriving as the crow flies also needs the match within this of the
    /// finish along the route (the finishing stretch, not a road beside it).
    static constexpr float    kFinishStretchM = 300.0f;
    /// Riding needed after joining a loop at its start before the position on
    /// the route, rather than the distance ridden, says what is left.
    static constexpr float    kSettleM = 1000.0f;
    static constexpr float    kClosedM     = 150.0f;  ///< Ends this close make a loop.
    /// A loop joined further round than this, and than kMidJoinFraction of
    /// it, finishes where it was joined.
    static constexpr float    kMidJoinM    = 1500.0f;
    static constexpr float    kMidJoinFraction = 0.10f;
    static constexpr uint16_t kBehindSegs  = 10;
    static constexpr uint16_t kAheadSegs   = 80;
    /// Progress expected between fixes when choosing among passes: a second at
    /// a typical riding speed.
    static constexpr float    kExpectedAdvanceM = 6.0f;
    /// Travel needed before the direction of travel is updated: long enough
    /// that GPS scatter of a few metres cannot turn it round.
    static constexpr float    kHeadingMinMoveM  = 8.0f;
    /// A change in position along the route more than twice the distance
    /// moved, plus this, is a change of pass, not progress. (A detour that
    /// rejoins further along still counts what it skipped.)
    static constexpr float    kJumpSlackM       = 200.0f;
    /// Moving to another pass because the rider is riding this one
    /// backwards needs |cos| beyond this on both, for kWrongWayFixes fixes.
    static constexpr float    kWrongWayCos      = 0.5f;
    static constexpr uint8_t  kWrongWayFixes    = 3;
    /// Segments this close to a pass's nearest point decide its direction.
    static constexpr float    kDirectionNearM   = 8.0f;

    enum class Event : uint8_t {
        None,
        OffCourse,
        BackOnCourse,
        Arrived,  ///< Reached the finish (once).
    };

    struct Status {
        bool     matched    = false;  ///< At least one fix has been matched.
        bool     joined     = false;  ///< Has been within kOnM of the route.
        bool     offCourse  = false;
        bool     arrived    = false;
        float    offM       = 0.0f;   ///< Distance from the route, metres.
        float    alongM     = 0.0f;   ///< Position along the route from its start, metres.
        float    riddenM    = 0.0f;   ///< Route distance ridden since joining, metres.
        float    remainingM = 0.0f;   ///< Route distance still to ride to the finish, metres.
        float    x = 0.0f, y = 0.0f;  ///< Rider position on the route's plane.
        uint16_t segment    = 0;      ///< Segment index of the match.
    };

    explicit RouteFollower(const Route& route) : mRoute(route) {}

    /// Start following from scratch (new activity, or a new route loaded).
    void reset();

    /// One GPS tick. @p fix false: hold the state.
    Event tick(double lat, double lon, bool fix);

    const Status& status() const { return mStatus; }

    /// True if the route is a loop (its ends within kClosedM).
    bool closed() const { return mClosed; }

    /// Length of a full ride of the route from where it was joined: the
    /// route's length (a loop joined part way round is still one lap).
    float courseLengthM() const { return mRoute.lengthM(); }

private:
    struct Match {
        uint16_t seg  = 0;
        float    dist = 1e30f;
        float    along = 0.0f;
        bool     forward = true;  ///< Runs the way the rider is going.
        float    cosHeading = 1.0f;  ///< Cosine between segment and direction of travel.
    };

    Match segMatch(float x, float y, uint16_t i) const;
    /// Choose a pass among segments from @p from, @p count of them, wrapping
    /// on a loop. See the file comment for the preferences.
    Match choose(float x, float y, uint16_t from, uint16_t count, float expectedAlong) const;
    float alongGap(float a, float b) const;     ///< |a - b|, around the loop if closed.
    float alongDelta(float from, float to) const;  ///< to - from, around the loop if closed.

    const Route& mRoute;
    Status       mStatus;
    uint8_t      mOffRun = 0;
    uint8_t      mOnRun  = 0;
    uint8_t      mSinceBackOn = kRealertFixes;  ///< Fixes since the last BackOnCourse.

    bool  mClosed    = false;
    bool  mFromJoin  = false;   ///< A loop joined part way round: finish at the join point.
    float mJoinAlong = 0.0f;
    float mOnRouteAlong = 0.0f;  ///< Position at the last fix on the route.
    float mMovedM = 0.0f;        ///< Distance moved since that fix.
    uint8_t mWrongWayRun = 0;
    float mMovedSinceJoinM = 0.0f;
    bool  mHavePrevFix = false;
    float mPrevFixX = 0.0f, mPrevFixY = 0.0f;
    float mJoinX = 0.0f, mJoinY = 0.0f;

    bool  mHaveLast  = false;   ///< A previous fix for the direction of travel.
    float mLastX = 0.0f, mLastY = 0.0f;
    bool  mHaveHeading = false;
    float mHeadX = 0.0f, mHeadY = 0.0f;
};

}  // namespace Ride

#endif  // RIDE_ROUTE_FOLLOWER_HPP
