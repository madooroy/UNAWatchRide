#include "Ride/Route.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <memory>

using Ride::Route;
using Ride::RouteShape;

namespace {

constexpr double kLat0 = 45.0;
constexpr double kLon0 = 7.0;
constexpr double kDegM = Route::kMetresPerDegree;

/// Point @p east / @p north metres from (kLat0, kLon0).
void at(double east, double north, double& lat, double& lon)
{
    lat = kLat0 + north / kDegM;
    lon = kLon0 + east / (kDegM * std::cos(kLat0 * 3.14159265358979323846 / 180.0));
}

// Route is ~32 KB: keep it off the stack.
std::unique_ptr<Route> makeRoute() { return std::make_unique<Route>(); }

}  // namespace

TEST(Route, EmptyAndSinglePointAreInvalid)
{
    auto r = makeRoute();
    r->begin();
    r->finish();
    EXPECT_FALSE(r->valid());

    r->begin();
    r->add(kLat0, kLon0);
    r->finish();
    EXPECT_FALSE(r->valid());
}

TEST(Route, ProjectionIsInMetres)
{
    auto r = makeRoute();
    r->begin();
    double lat, lon;
    at(0, 0, lat, lon);
    r->add(lat, lon);
    at(1000, 2000, lat, lon);
    r->add(lat, lon);
    r->finish();
    ASSERT_TRUE(r->valid());

    float x, y;
    r->project(lat, lon, x, y);
    EXPECT_NEAR(x, 1000.0f, 0.5f);
    EXPECT_NEAR(y, 2000.0f, 0.5f);
    EXPECT_NEAR(r->lengthM(), std::sqrt(1000.0f * 1000.0f + 2000.0f * 2000.0f), 1.0f);
}

TEST(Route, ClosePointsAreThinnedButTheEndIsKept)
{
    auto r = makeRoute();
    r->begin();
    double lat, lon;
    for (int i = 0; i <= 1003; ++i) {  // Every metre for 1003 m.
        at(i, 0, lat, lon);
        r->add(lat, lon);
    }
    r->finish();
    ASSERT_TRUE(r->valid());
    EXPECT_LE(r->size(), 1003 / 5 + 2);
    EXPECT_NEAR((*r)[r->size() - 1].x, 1003.0f, 0.5f);  // Exact end.
    EXPECT_NEAR(r->lengthM(), 1003.0f, 0.5f);
}

TEST(Route, LongRouteFitsAndKeepsItsShape)
{
    // 200 km of a sine-wave road (+-300 m, 4 km wavelength), a point every 2 m:
    // 100k input points into a 2000-point buffer.
    auto r = makeRoute();
    r->begin();
    double lat, lon;
    const double kPi = 3.14159265358979323846;
    for (int i = 0; i <= 100000; ++i) {
        const double e = i * 2.0;
        const double n = 300.0 * std::sin(2.0 * kPi * e / 4000.0);
        at(e, n, lat, lon);
        r->add(lat, lon);
    }
    r->finish();
    ASSERT_TRUE(r->valid());
    EXPECT_LE(r->size(), Route::kMaxPoints);
    EXPECT_GT(r->size(), 500);
    EXPECT_LT(r->toleranceM(), 20.0f);

    // Every kept point is on the curve, and the length is close to the arc.
    for (uint16_t i = 0; i < r->size(); ++i) {
        const auto& p = (*r)[i];
        const double n = 300.0 * std::sin(2.0 * kPi * p.x / 4000.0);
        EXPECT_NEAR(p.y, n, 1.0) << i;
    }
    // The curve's arc length is 210.68 km; simplification cuts corners a little.
    EXPECT_NEAR(r->lengthM(), 210679.0f, 0.005f * 210679.0f);
    EXPECT_LE(r->lengthM(), 210679.0f + 1.0f);
    EXPECT_NEAR((*r)[r->size() - 1].x, 200000.0f, 2.0f);
}

TEST(Route, StraightLongRouteCollapses)
{
    auto r = makeRoute();
    r->begin();
    double lat, lon;
    for (int i = 0; i <= 20000; ++i) {  // 20 km due north, every metre.
        at(0, i, lat, lon);
        r->add(lat, lon);
    }
    r->finish();
    EXPECT_LT(r->size(), 1600);
    EXPECT_NEAR(r->lengthM(), 20000.0f, 2.0f);
}

TEST(Route, SegmentDistance)
{
    float t;
    EXPECT_FLOAT_EQ(Route::segmentDistance(5, 3, 0, 0, 10, 0, t), 3.0f);
    EXPECT_FLOAT_EQ(t, 0.5f);
    EXPECT_FLOAT_EQ(Route::segmentDistance(-4, 3, 0, 0, 10, 0, t), 5.0f);  // Beyond the start.
    EXPECT_FLOAT_EQ(t, 0.0f);
    EXPECT_FLOAT_EQ(Route::segmentDistance(13, 4, 0, 0, 10, 0, t), 5.0f);  // Beyond the end.
    EXPECT_FLOAT_EQ(t, 1.0f);
    EXPECT_FLOAT_EQ(Route::segmentDistance(3, 4, 0, 0, 0, 0, t), 5.0f);    // Degenerate.
}

TEST(Route, ShapeFitsTheBox)
{
    auto r = makeRoute();
    r->begin();
    double lat, lon;
    // An L: 3 km east then 1 km north, a point every 10 m.
    for (int i = 0; i <= 300; ++i) {
        at(i * 10.0, 0, lat, lon);
        r->add(lat, lon);
    }
    for (int i = 1; i <= 100; ++i) {
        at(3000.0, i * 10.0, lat, lon);
        r->add(lat, lon);
    }
    r->finish();

    RouteShape s;
    r->buildShape(s, 140);
    ASSERT_GE(s.count, 2);
    EXPECT_LE(s.count, RouteShape::kMaxPoints);
    EXPECT_EQ(s.minx, 0);
    EXPECT_EQ(s.maxx, 139);
    // 1 km of height on a 3 km width: about a third of the box, centred.
    EXPECT_NEAR(s.maxy - s.miny, 139 / 3, 2);
    EXPECT_NEAR((s.miny + s.maxy) / 2, 70, 2);
    // Start bottom-left, end top-right (y grows down).
    EXPECT_EQ(s.points[0].x, 0);
    EXPECT_EQ(s.points[s.count - 1].x, 139);
    EXPECT_LT(s.points[s.count - 1].y, s.points[0].y);

    // The rider at the corner maps to the shape's corner.
    uint8_t px, py;
    EXPECT_TRUE(Route::toScreen(s, 140, 3000.0f, 0.0f, px, py));
    EXPECT_EQ(px, 139);
    EXPECT_EQ(py, s.points[0].y);
    // Far outside: clamped, and reported as outside.
    EXPECT_FALSE(Route::toScreen(s, 140, -5000.0f, 0.0f, px, py));
    EXPECT_EQ(px, 0);
}

TEST(Route, ShapeOfAWigglyRouteIsCapped)
{
    auto r = makeRoute();
    r->begin();
    double lat, lon;
    for (int i = 0; i <= 2000; ++i) {
        at(i * 10.0, (i % 2) ? 400.0 : 0.0, lat, lon);  // Saw-tooth: every point a corner.
        r->add(lat, lon);
    }
    r->finish();
    RouteShape s;
    r->buildShape(s, 140);
    EXPECT_LE(s.count, RouteShape::kMaxPoints);
    EXPECT_GE(s.count, 2);
}
