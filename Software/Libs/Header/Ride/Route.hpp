/**
 ******************************************************************************
 * @file    Route.hpp
 * @brief   A planned route held in a fixed buffer, in local metres.
 *
 * Points arrive one at a time (from GpxReader) and are projected onto a flat
 * east/north plane around the first point -- an equirectangular projection,
 * accurate to well under a metre per kilometre over the size of a day's ride,
 * which is far finer than GPS. Working in metres keeps every later step (the
 * distance to the route, progress along it, drawing it) simple float math.
 *
 * The buffer holds kMaxPoints. A GPX export often has a point every few
 * metres, so while loading:
 *   - points closer than the current spacing to the last kept point are
 *     skipped (the last point of the file is always kept);
 *   - when the buffer fills, it is simplified in place with Douglas-Peucker,
 *     the tolerance doubling until it is three-quarters full, and the spacing
 *     follows the tolerance.
 * A 200 km route therefore loads at a tolerance of a few metres -- straight
 * roads collapse to two points, bends keep their shape.
 ******************************************************************************
 */

#ifndef RIDE_ROUTE_HPP
#define RIDE_ROUTE_HPP

#include <cstdint>

namespace Ride {

struct RoutePoint {
    float x;    ///< Metres east of the origin.
    float y;    ///< Metres north of the origin.
    float cum;  ///< Distance along the route from its start, metres.
};

/**
 * The route drawn for a square screen area, for the GUI. Plain data: it is
 * passed to the GUI by pointer and copied there.
 */
struct RouteShape {
    static constexpr uint8_t kMaxPoints = 96;

#pragma pack(push, 1)
    struct Point {
        uint8_t x;
        uint8_t y;
    };
#pragma pack(pop)

    uint8_t count = 0;
    Point   points[kMaxPoints] = {};
    uint8_t minx = 0, miny = 0, maxx = 0, maxy = 0;  ///< Bounding box of the points.

    // Transform from route metres to these pixels, for the rider's dot.
    float originX = 0.0f;  ///< Route x at pixel 0.
    float originY = 0.0f;  ///< Route y at pixel 0 (top edge).
    float pxPerM  = 0.0f;
};

class Route {
public:
    static constexpr uint16_t kMaxPoints      = 2000;
    static constexpr float    kStartSpacingM  = 5.0f;
    static constexpr float    kStartToleranceM = 2.0f;
    static constexpr double   kMetresPerDegree = 111319.49;

    // -- Building -------------------------------------------------------------

    /// Empty the route, ready for add().
    void begin();

    /// Add the next point of the file.
    void add(double lat, double lon);

    /// The file is done: keep its last point and measure the route.
    void finish();

    // -- Using ------------------------------------------------------------------

    bool     valid() const { return mFinished && mCount >= 2; }
    uint16_t size() const { return mCount; }
    const RoutePoint& operator[](uint16_t i) const { return mPts[i]; }
    float    lengthM() const { return mCount ? mPts[mCount - 1].cum : 0.0f; }
    float    toleranceM() const { return mTolerance; }

    /// Project a position onto the route's plane.
    void project(double lat, double lon, float& x, float& y) const;

    /**
     * Draw the route to fit a @p box x @p box pixel square (y down), keeping
     * its aspect ratio, in at most RouteShape::kMaxPoints points.
     */
    void buildShape(RouteShape& out, uint8_t box) const;

    /// Pixel of route point (x, y) in @p shape, clamped to the box. False if it
    /// lies outside the box.
    static bool toScreen(const RouteShape& shape, uint8_t box, float x, float y,
                         uint8_t& px, uint8_t& py);

    /// Distance from (px, py) to segment a-b, and the fraction t (0..1) along it.
    static float segmentDistance(float px, float py, float ax, float ay, float bx, float by,
                                 float& t);

private:
    void store(float x, float y);
    void simplify(float tolerance);

    RoutePoint mPts[kMaxPoints];
    uint8_t    mKeep[(kMaxPoints + 7) / 8];
    uint16_t   mCount     = 0;
    bool       mFinished  = false;

    double     mLat0 = 0.0, mLon0 = 0.0, mCosLat0 = 1.0;
    float      mSpacing   = kStartSpacingM;
    float      mTolerance = kStartToleranceM;

    bool       mHavePending = false;
    float      mPendingX = 0.0f, mPendingY = 0.0f;
};

}  // namespace Ride

#endif  // RIDE_ROUTE_HPP
