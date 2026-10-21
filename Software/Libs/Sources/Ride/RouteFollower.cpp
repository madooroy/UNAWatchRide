/**
 ******************************************************************************
 * @file    RouteFollower.cpp
 * @brief   See RouteFollower.hpp.
 ******************************************************************************
 */

#include "Ride/RouteFollower.hpp"

namespace Ride {

namespace {

/// When the whole route is searched, segments this close to the nearest one
/// count as equally near, and the first of them (from the preferred start) is
/// taken. That resolves a loop's start/finish to the start, and a road ridden
/// twice to the pass ahead.
constexpr float kTieM = RouteFollower::kOnM;

}  // namespace

void RouteFollower::reset()
{
    mStatus = Status{};
    mOffRun = 0;
    mOnRun  = 0;
}

RouteFollower::Match RouteFollower::best(float x, float y, uint16_t from, uint16_t to) const
{
    Match m;
    m.dist = 1e30f;
    for (uint16_t i = from; i < to; ++i) {
        const RoutePoint& a = mRoute[i];
        const RoutePoint& b = mRoute[static_cast<uint16_t>(i + 1)];
        float t;
        const float d = Route::segmentDistance(x, y, a.x, a.y, b.x, b.y, t);
        if (d < m.dist) {
            m.dist  = d;
            m.seg   = i;
            m.along = a.cum + t * (b.cum - a.cum);
        }
    }
    return m;
}

RouteFollower::Event RouteFollower::tick(double lat, double lon, bool fix)
{
    if (!fix || !mRoute.valid()) {
        return Event::None;
    }

    float x, y;
    mRoute.project(lat, lon, x, y);
    const uint16_t segs = static_cast<uint16_t>(mRoute.size() - 1u);

    // Whole-route search. Segments within kTieM of the nearest one form
    // stretches of road (passes); take the first pass, from preferFrom on and
    // then from the start, and the nearest point within that pass.
    auto searchAll = [&](uint16_t preferFrom) {
        const Match nearest = best(x, y, 0, segs);
        const float limit   = nearest.dist + kTieM;
        for (int pass = 0; pass < 2; ++pass) {
            const uint16_t lo = pass == 0 ? preferFrom : 0;
            const uint16_t hi = pass == 0 ? segs : preferFrom;
            for (uint16_t i = lo; i < hi; ++i) {
                Match m = best(x, y, i, static_cast<uint16_t>(i + 1));
                if (m.dist > limit) {
                    continue;
                }
                for (uint16_t j = static_cast<uint16_t>(i + 1); j < hi; ++j) {
                    const Match n = best(x, y, j, static_cast<uint16_t>(j + 1));
                    if (n.dist > limit) {
                        break;  // End of this pass.
                    }
                    if (n.dist < m.dist) {
                        m = n;
                    }
                }
                return m;
            }
        }
        return nearest;
    };

    Match m;
    if (!mStatus.matched) {
        m = searchAll(0);
    } else {
        const uint16_t seg  = mStatus.segment;
        const uint16_t from = seg > kBehindSegs ? static_cast<uint16_t>(seg - kBehindSegs) : 0;
        const uint16_t to   = (segs - seg) > kAheadSegs ? static_cast<uint16_t>(seg + kAheadSegs) : segs;
        m = best(x, y, from, to);
        if (m.dist > kReacquireM) {
            const Match g = searchAll(seg);
            if (g.dist < m.dist) {
                m = g;
            }
        }
    }

    mStatus.matched    = true;
    mStatus.segment    = m.seg;
    mStatus.offM       = m.dist;
    mStatus.alongM     = m.along;
    mStatus.remainingM = mRoute.lengthM() - m.along;
    if (mStatus.remainingM < 0.0f) {
        mStatus.remainingM = 0.0f;
    }
    mStatus.x = x;
    mStatus.y = y;

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

    // Arrival needs the second half of the route behind the rider, so the
    // start of a loop, which is also its end, never counts.
    if (ev == Event::None && !mStatus.arrived && !mStatus.offCourse &&
        m.dist <= kOnM && mStatus.remainingM <= kFinishM &&
        mStatus.alongM >= 0.5f * mRoute.lengthM()) {
        mStatus.arrived = true;
        ev = Event::Arrived;
    }
    return ev;
}

}  // namespace Ride
