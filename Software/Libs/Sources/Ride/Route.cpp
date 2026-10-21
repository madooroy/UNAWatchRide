/**
 ******************************************************************************
 * @file    Route.cpp
 * @brief   See Route.hpp.
 ******************************************************************************
 */

#include "Ride/Route.hpp"

#include <cmath>
#include <cstring>

namespace Ride {

namespace {

constexpr double kPi = 3.14159265358979323846;

inline float dist2(float ax, float ay, float bx, float by)
{
    const float dx = bx - ax;
    const float dy = by - ay;
    return dx * dx + dy * dy;
}

}  // namespace

float Route::segmentDistance(float px, float py, float ax, float ay, float bx, float by, float& t)
{
    const float vx = bx - ax;
    const float vy = by - ay;
    const float len2 = vx * vx + vy * vy;
    t = 0.0f;
    if (len2 > 1e-6f) {
        t = ((px - ax) * vx + (py - ay) * vy) / len2;
        if (t < 0.0f) {
            t = 0.0f;
        } else if (t > 1.0f) {
            t = 1.0f;
        }
    }
    const float cx = ax + t * vx;
    const float cy = ay + t * vy;
    return std::sqrt(dist2(px, py, cx, cy));
}

void Route::begin()
{
    mCount       = 0;
    mFinished    = false;
    mSpacing     = kStartSpacingM;
    mTolerance   = kStartToleranceM;
    mHavePending = false;
}

void Route::project(double lat, double lon, float& x, float& y) const
{
    x = static_cast<float>((lon - mLon0) * kMetresPerDegree * mCosLat0);
    y = static_cast<float>((lat - mLat0) * kMetresPerDegree);
}

void Route::simplify(float tolerance)
{
    const uint16_t n = mCount;
    if (n < 3) {
        return;
    }

    std::memset(mKeep, 0, sizeof(mKeep));
    auto keep   = [this](uint16_t i) { mKeep[i >> 3] |= static_cast<uint8_t>(1u << (i & 7u)); };
    auto isKept = [this](uint16_t i) { return (mKeep[i >> 3] & (1u << (i & 7u))) != 0; };

    keep(0);
    keep(static_cast<uint16_t>(n - 1));

    // Douglas-Peucker without a stack: each pass splits every kept span whose
    // farthest point is beyond the tolerance, until a pass changes nothing.
    // Each pass is O(n) and the passes are about log2(n) on real roads.
    bool changed = true;
    while (changed) {
        changed = false;
        uint16_t a = 0;
        while (a < n - 1) {
            uint16_t b = static_cast<uint16_t>(a + 1);
            while (!isKept(b)) {
                ++b;
            }
            float    worst = 0.0f;
            uint16_t at    = 0;
            for (uint16_t i = static_cast<uint16_t>(a + 1); i < b; ++i) {
                float t;
                const float d = segmentDistance(mPts[i].x, mPts[i].y, mPts[a].x, mPts[a].y,
                                                mPts[b].x, mPts[b].y, t);
                if (d > worst) {
                    worst = d;
                    at    = i;
                }
            }
            if (worst > tolerance) {
                keep(at);
                changed = true;
            }
            a = b;
        }
    }

    uint16_t w = 0;
    for (uint16_t i = 0; i < n; ++i) {
        if (isKept(i)) {
            mPts[w++] = mPts[i];
        }
    }
    mCount = w;
}

void Route::store(float x, float y)
{
    if (mCount == kMaxPoints) {
        float tol = mTolerance;
        do {
            simplify(tol);
            mTolerance = tol;
            tol *= 2.0f;
        } while (mCount > (kMaxPoints * 3u) / 4u);
        if (mSpacing < mTolerance) {
            mSpacing = mTolerance;
        }
    }
    mPts[mCount++] = RoutePoint{x, y, 0.0f};
}

void Route::add(double lat, double lon)
{
    if (mFinished) {
        return;
    }
    if (mCount == 0) {
        mLat0    = lat;
        mLon0    = lon;
        mCosLat0 = std::cos(lat * kPi / 180.0);
        store(0.0f, 0.0f);
        return;
    }

    float x, y;
    project(lat, lon, x, y);

    const RoutePoint& last = mPts[mCount - 1];
    if (dist2(last.x, last.y, x, y) >= mSpacing * mSpacing) {
        store(x, y);
        mHavePending = false;
    } else {
        // Too close to keep for now, but remembered so the route still ends
        // exactly where the file does.
        mPendingX    = x;
        mPendingY    = y;
        mHavePending = true;
    }
}

void Route::finish()
{
    if (mHavePending && mCount > 0) {
        const RoutePoint& last = mPts[mCount - 1];
        if (dist2(last.x, last.y, mPendingX, mPendingY) > 0.01f) {
            store(mPendingX, mPendingY);
        }
        mHavePending = false;
    }

    float cum = 0.0f;
    for (uint16_t i = 0; i < mCount; ++i) {
        if (i > 0) {
            cum += std::sqrt(dist2(mPts[i - 1].x, mPts[i - 1].y, mPts[i].x, mPts[i].y));
        }
        mPts[i].cum = cum;
    }
    mFinished = true;
}

void Route::buildShape(RouteShape& out, uint8_t box) const
{
    out = RouteShape{};
    if (!valid() || box < 2) {
        return;
    }

    float minx = mPts[0].x, maxx = mPts[0].x, miny = mPts[0].y, maxy = mPts[0].y;
    for (uint16_t i = 1; i < mCount; ++i) {
        const RoutePoint& p = mPts[i];
        minx = p.x < minx ? p.x : minx;
        maxx = p.x > maxx ? p.x : maxx;
        miny = p.y < miny ? p.y : miny;
        maxy = p.y > maxy ? p.y : maxy;
    }
    const float w    = maxx - minx;
    const float h    = maxy - miny;
    float       span = w > h ? w : h;
    if (span < 1.0f) {
        span = 1.0f;
    }

    out.pxPerM  = static_cast<float>(box - 1) / span;
    out.originX = minx - (span - w) / 2.0f;  // Centre the short side.
    out.originY = maxy + (span - h) / 2.0f;

    // Keep a point when it is at least minPx from the last one kept; widen
    // the spacing until the route fits the shape's buffer.
    float minPx = 2.0f;
    for (;;) {
        uint8_t n = 0;
        bool    overflow = false;
        float   lx = 0.0f, ly = 0.0f;
        for (uint16_t i = 0; i < mCount; ++i) {
            const float fx = (mPts[i].x - out.originX) * out.pxPerM;
            const float fy = (out.originY - mPts[i].y) * out.pxPerM;
            const bool  last = (i == mCount - 1);
            if (i == 0 || last || dist2(lx, ly, fx, fy) >= minPx * minPx) {
                if (n == RouteShape::kMaxPoints) {
                    overflow = true;
                    break;
                }
                out.points[n].x = static_cast<uint8_t>(std::lround(fx));
                out.points[n].y = static_cast<uint8_t>(std::lround(fy));
                ++n;
                lx = fx;
                ly = fy;
            }
        }
        if (!overflow) {
            out.count = n;
            break;
        }
        minPx *= 1.5f;
    }

    out.minx = out.maxx = out.points[0].x;
    out.miny = out.maxy = out.points[0].y;
    for (uint8_t i = 1; i < out.count; ++i) {
        const auto& p = out.points[i];
        out.minx = p.x < out.minx ? p.x : out.minx;
        out.maxx = p.x > out.maxx ? p.x : out.maxx;
        out.miny = p.y < out.miny ? p.y : out.miny;
        out.maxy = p.y > out.maxy ? p.y : out.maxy;
    }
}

bool Route::toScreen(const RouteShape& shape, uint8_t box, float x, float y, uint8_t& px,
                     uint8_t& py)
{
    float fx = (x - shape.originX) * shape.pxPerM;
    float fy = (shape.originY - y) * shape.pxPerM;
    const float maxPx = static_cast<float>(box - 1);
    const bool  inside = fx >= 0.0f && fx <= maxPx && fy >= 0.0f && fy <= maxPx;
    fx = fx < 0.0f ? 0.0f : (fx > maxPx ? maxPx : fx);
    fy = fy < 0.0f ? 0.0f : (fy > maxPx ? maxPx : fy);
    px = static_cast<uint8_t>(std::lround(fx));
    py = static_cast<uint8_t>(std::lround(fy));
    return inside;
}

}  // namespace Ride
