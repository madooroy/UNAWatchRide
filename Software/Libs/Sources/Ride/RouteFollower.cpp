/**
 ******************************************************************************
 * @file    RouteFollower.cpp
 * @brief   See RouteFollower.hpp.
 ******************************************************************************
 */

#include "Ride/RouteFollower.hpp"

#include <algorithm>
#include <cmath>

namespace Ride {

namespace {

/// Segments within this of the nearest one count as equally near when
/// choosing between passes.
constexpr float kTieM = RouteFollower::kOnM;

}  // namespace

void RouteFollower::reset()
{
    mStatus = Status{};
    mOffRun = 0;
    mOnRun  = 0;
    mFromJoin  = false;
    mJoinAlong = 0.0f;
    mOnRouteAlong = 0.0f;
    mHaveLast = mHaveHeading = false;
    mHavePrevFix = false;
    mMovedM = 0.0f;
    mMovedSinceJoinM = 0.0f;
    mWrongWayRun = 0;

    mClosed = false;
    if (mRoute.valid()) {
        const RoutePoint& a = mRoute[0];
        const RoutePoint& b = mRoute[static_cast<uint16_t>(mRoute.size() - 1u)];
        mClosed = std::hypot(b.x - a.x, b.y - a.y) <= kClosedM && mRoute.lengthM() > 4.0f * kClosedM;
    }
    mStatus.remainingM = mRoute.valid() ? mRoute.lengthM() : 0.0f;
}

float RouteFollower::alongDelta(float from, float to) const
{
    float d = to - from;
    if (mClosed) {
        const float L = mRoute.lengthM();
        if (d > 0.5f * L) {
            d -= L;
        } else if (d < -0.5f * L) {
            d += L;
        }
    }
    return d;
}

float RouteFollower::alongGap(float a, float b) const
{
    return std::fabs(alongDelta(a, b));
}

RouteFollower::Match RouteFollower::segMatch(float x, float y, uint16_t i) const
{
    const RoutePoint& a = mRoute[i];
    const RoutePoint& b = mRoute[static_cast<uint16_t>(i + 1)];
    Match m;
    float t;
    m.dist  = Route::segmentDistance(x, y, a.x, a.y, b.x, b.y, t);
    m.seg   = i;
    m.along = a.cum + t * (b.cum - a.cum);
    m.cosHeading = 1.0f;
    if (mHaveHeading) {
        const float sx = b.x - a.x, sy = b.y - a.y;
        const float n  = std::sqrt((sx * sx + sy * sy) * (mHeadX * mHeadX + mHeadY * mHeadY));
        m.cosHeading = n > 1e-6f ? (sx * mHeadX + sy * mHeadY) / n : 1.0f;
    }
    m.forward = m.cosHeading >= 0.0f;
    return m;
}

RouteFollower::Match RouteFollower::choose(float x, float y, uint16_t from, uint16_t count,
                                           float expectedAlong) const
{
    const uint16_t segs = static_cast<uint16_t>(mRoute.size() - 1u);
    auto segAt = [&](uint16_t k) -> uint16_t {
        uint32_t i = static_cast<uint32_t>(from) + k;
        return static_cast<uint16_t>(mClosed ? i % segs : (i < segs ? i : segs - 1u));
    };

    // Nearest distance in the range.
    float nearest = 1e30f;
    for (uint16_t k = 0; k < count; ++k) {
        const Match m = segMatch(x, y, segAt(k));
        nearest = m.dist < nearest ? m.dist : nearest;
    }
    const float limit = nearest + kTieM;

    // Passes: runs of segments within the limit. A pass's candidate is its
    // nearest point. Prefer a pass going the rider's way, then the one whose
    // progress is closest to the expected progress.
    Match    chosen;
    bool     haveChosen = false;
    float    bestGap = 1e30f;
    bool     inPass = false;
    uint16_t passFirst = 0;
    Match    local;
    auto closePass = [&](uint16_t passEnd) {
        // The pass goes the rider's way if a segment right at its nearest
        // point does: at a corner both legs are there, while on a winding
        // road taken the other way, the bends a few tens of metres off do
        // not count.
        float bestCos = -1.0f;
        for (uint16_t q = passFirst; q < passEnd; ++q) {
            const Match n = segMatch(x, y, segAt(q));
            if (n.dist <= local.dist + kDirectionNearM && n.cosHeading > bestCos) {
                bestCos = n.cosHeading;
            }
        }
        local.cosHeading = bestCos;
        local.forward    = bestCos >= 0.0f;
        const float gap = alongGap(local.along, expectedAlong);
        const bool better = !haveChosen ||
                            (local.forward && !chosen.forward) ||
                            (local.forward == chosen.forward && gap < bestGap);
        if (better) {
            chosen = local;
            bestGap = gap;
            haveChosen = true;
        }
        inPass = false;
    };
    for (uint16_t k = 0; k < count; ++k) {
        const Match m = segMatch(x, y, segAt(k));
        if (m.dist <= limit) {
            // Within a pass the nearest point wins, so a fast rider is never
            // held back along a straight road.
            if (!inPass) {
                passFirst = k;
                local = m;
            } else if (m.dist < local.dist) {
                local = m;
            }
            inPass = true;
        } else if (inPass) {
            closePass(k);
        }
    }
    if (inPass) {
        closePass(count);
    }
    return chosen;
}

RouteFollower::Event RouteFollower::tick(double lat, double lon, bool fix)
{
    if (!fix || !mRoute.valid()) {
        return Event::None;
    }

    float x, y;
    mRoute.project(lat, lon, x, y);
    const uint16_t segs = static_cast<uint16_t>(mRoute.size() - 1u);

    // Distance actually moved, for telling progress from re-identification.
    if (mHavePrevFix) {
        const float step = std::hypot(x - mPrevFixX, y - mPrevFixY);
        mMovedM += step;
        if (mStatus.joined) {
            mMovedSinceJoinM += step;
        }
    }
    mPrevFixX = x;
    mPrevFixY = y;
    mHavePrevFix = true;

    // Direction of travel, from the last fix that is far enough away.
    if (mHaveLast) {
        const float dx = x - mLastX, dy = y - mLastY;
        if (dx * dx + dy * dy >= kHeadingMinMoveM * kHeadingMinMoveM) {
            mHeadX = dx;
            mHeadY = dy;
            mHaveHeading = true;
            mLastX = x;
            mLastY = y;
        }
    } else {
        mLastX = x;
        mLastY = y;
        mHaveLast = true;
    }

    Match m;
    if (!mStatus.matched) {
        // First fix: the whole route, preferring the pass nearest its start
        // (a loop's start rather than its finish).
        m = choose(x, y, 0, segs, 0.0f);
    } else {
        const float expected = mStatus.alongM + kExpectedAdvanceM;
        const uint16_t seg   = mStatus.segment;
        uint16_t from, count;
        if (mClosed) {
            from  = static_cast<uint16_t>((seg + segs - (kBehindSegs % segs)) % segs);
            count = static_cast<uint16_t>(kBehindSegs + kAheadSegs < segs ? kBehindSegs + kAheadSegs : segs);
        } else {
            from = seg > kBehindSegs ? static_cast<uint16_t>(seg - kBehindSegs) : 0;
            const uint16_t to = (segs - seg) > kAheadSegs ? static_cast<uint16_t>(seg + kAheadSegs) : segs;
            count = static_cast<uint16_t>(to - from);
        }
        m = choose(x, y, from, count, expected);
        if (m.dist > kOnM) {
            // Not on the route near where we were: before calling it off
            // course, look along all of it.
            const Match g = choose(x, y, 0, segs, expected);
            if (g.dist < m.dist) {
                m = g;
            }
        } else if (mHaveHeading && m.cosHeading < -kWrongWayCos) {
            // On the route, but clearly riding it backwards here. If the
            // route also takes this road clearly the rider's way somewhere
            // else (out on it, back the same way later; or a loop joined part
            // way, heading home), and keeps doing so for a few fixes, that is
            // where the rider is. Junctions and hairpins, where the heading is
            // briefly across both, do not switch.
            const Match g = choose(x, y, 0, segs, expected);
            if (g.dist <= kOnM && g.cosHeading > kWrongWayCos) {
                if (++mWrongWayRun >= kWrongWayFixes) {
                    m = g;
                    mWrongWayRun = 0;
                }
            } else {
                mWrongWayRun = 0;
            }
        } else {
            mWrongWayRun = 0;
        }
    }

    // Progress since joining, carried around a loop. Only fixes on the route
    // count, measured from the last one: while the rider is away from it the
    // nearest point on it wanders, and adding that up would credit a detour
    // as progress. Coming back on further along (a shortcut) counts the part
    // skipped, as the route's own "to go" would.
    //
    // A change of position along the route far bigger than the distance
    // actually moved is not riding: it is the match moving to another pass
    // over the same road (the way the rider is going turned out to be the
    // other pass). It is not credited.
    // Riding a stretch of the route backwards (a different way back to the
    // car, on the route's own roads) is not progress, but it does not undo
    // any either.
    if (mStatus.joined && m.dist <= kOnM) {
        const float d = alongDelta(mOnRouteAlong, m.along);
        const bool backwards = d < 0.0f && mHaveHeading && m.cosHeading < -kWrongWayCos;
        if (!backwards && std::fabs(d) <= 2.0f * mMovedM + kJumpSlackM) {
            mStatus.riddenM += d;
            if (mStatus.riddenM < 0.0f) {
                mStatus.riddenM = 0.0f;
            }
        }
        mOnRouteAlong = m.along;
        mMovedM = 0.0f;
    }

    mStatus.matched = true;
    mStatus.segment = m.seg;
    mStatus.offM    = m.dist;
    mStatus.alongM  = m.along;
    mStatus.x = x;
    mStatus.y = y;

    if (mStatus.arrived) {
        mStatus.remainingM = 0.0f;
        return Event::None;
    }

    const float L = mRoute.lengthM();
    if (!mStatus.joined) {
        // Joining waits for a direction of travel as well, so that a rider
        // who starts on a road the route takes twice joins the right pass.
        if (m.dist > kOnM || !mHaveHeading) {
            mStatus.remainingM = L;
            return Event::None;  // Still making our way to the route.
        }
        mStatus.joined = true;
        mJoinAlong = m.along;
        mOnRouteAlong = m.along;
        mMovedM = 0.0f;
        mJoinX = x;
        mJoinY = y;
        // Joined well round a loop (parked on the circuit): the ride finishes
        // where it joined. Joined near the start (a car-park lead-in skipped,
        // a few hundred metres in): it finishes at the route's end.
        const float midJoin = std::max(kMidJoinM, kMidJoinFraction * mRoute.lengthM());
        mFromJoin = mClosed && alongGap(m.along, 0.0f) > midJoin;
        // An open route joined part way along has that much less to ride. A
        // loop joined at its start/finish starts from nothing ridden, even if
        // the match landed on the finish side of the join.
        if (mFromJoin) {
            mStatus.riddenM = 0.0f;
        } else if (mClosed) {
            mStatus.riddenM = m.along > 0.5f * mRoute.lengthM() ? 0.0f : m.along;
        } else {
            mStatus.riddenM = m.along;
        }
    }

    // What is done and what is left. From the rider's position on the route
    // wherever that is unambiguous -- it is right however they got there
    // (turning back early on an out-and-back, a shortcut). From the distance
    // ridden since joining where it is not: a loop joined part way round
    // (its finish is the join point), and the first kSettleM of a loop,
    // where its start and finish are the same place.
    float done, finishX, finishY, finishAlong;
    if (mFromJoin || (mClosed && mMovedSinceJoinM < kSettleM)) {
        done = mStatus.riddenM;
    } else if (mClosed && mStatus.riddenM >= kArriveMinFraction * L && m.along < kSettleM) {
        done = L;  // Just over the finish of a loop, on round past its start.
    } else {
        done = m.along;
    }
    mStatus.remainingM = L - done;
    if (mFromJoin) {
        finishX = mJoinX;
        finishY = mJoinY;
        finishAlong = mJoinAlong;
    } else {
        const RoutePoint& end = mRoute[segs];
        finishX = end.x;
        finishY = end.y;
        finishAlong = L;
    }
    if (mStatus.remainingM < 0.0f) {
        mStatus.remainingM = 0.0f;
    }

    Event ev = Event::None;
    if (!mStatus.offCourse) {
        mOffRun = (m.dist > kOffM) ? static_cast<uint8_t>(mOffRun + (mOffRun < 255 ? 1 : 0)) : 0;
        if (mOffRun >= kOffFixes) {
            mStatus.offCourse = true;
            mOffRun = 0;
            mOnRun  = 0;
            ev = Event::OffCourse;
        }
    } else {
        mOnRun = (m.dist < kOnM) ? static_cast<uint8_t>(mOnRun + (mOnRun < 255 ? 1 : 0)) : 0;
        if (mOnRun >= kOnFixes) {
            mStatus.offCourse = false;
            mOnRun  = 0;
            mOffRun = 0;
            ev = Event::BackOnCourse;
        }
    }

    // Arrival: most of the route behind the rider, and the finish reached,
    // along the route or as the crow flies (a ride that ends in a car park
    // beside the finish, or a few metres short of it).
    //
    // When the ride strayed from the route's order (sections skipped, others
    // ridden the other way), the route progress undercounts, so a full lap's
    // distance ridden since joining also counts as "most of the route" -- but
    // only at the finish on the finishing stretch, going the finishing way,
    // so passing the car park mid-ride in the other direction does not.
    const bool  mostDone  = done >= kArriveMinFraction * L;
    const bool  lapRidden = mMovedSinceJoinM >= L;
    if (ev == Event::None && (mostDone || lapRidden)) {
        const bool alongDone = mostDone && !mStatus.offCourse && m.dist <= kOnM &&
                               mStatus.remainingM <= kFinishM;
        const bool nearDone  = std::hypot(x - finishX, y - finishY) <= kArriveM &&
                               alongGap(m.along, finishAlong) <= kFinishStretchM &&
                               (!mHaveHeading || m.cosHeading >= 0.0f);
        if (alongDone || nearDone) {
            mStatus.arrived    = true;
            mStatus.offCourse  = false;
            mStatus.remainingM = 0.0f;
            ev = Event::Arrived;
        }
    }
    return ev;
}

}  // namespace Ride
