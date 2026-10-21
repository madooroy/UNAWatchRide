#include "Ride/GpxReader.hpp"

#include <gtest/gtest.h>

#include <cstring>
#include <string>
#include <utility>
#include <vector>

using Ride::GpxReader;

namespace {

using Points = std::vector<std::pair<double, double>>;

bool collect(void* ctx, double lat, double lon)
{
    static_cast<Points*>(ctx)->emplace_back(lat, lon);
    return true;
}

const char* kGpx = R"(<?xml version="1.0" encoding="UTF-8"?>
<!-- exported by an app; a comment with <trkpt lat="1" lon="1"> inside -->
<gpx version="1.1" creator="test" xmlns="http://www.topografix.com/GPX/1/1">
  <metadata><name>Sunday loop</name></metadata>
  <wpt lat="10.0" lon="10.0"><name>Cafe</name></wpt>
  <trk><name>Loop</name><trkseg>
    <trkpt lat="51.5007" lon="-0.1246"><ele>12.0</ele><time>2026-10-01T08:00:00Z</time></trkpt>
    <trkpt lon="-0.1250" lat="51.5010"><ele>12.4</ele></trkpt>
    <trkpt  lat = '51.5013'   lon = '-0.1255' />
    <gpx:trkpt lat="51.5016" lon="-0.1260"></gpx:trkpt>
    <trkpt lat="95.0" lon="0.0"></trkpt>
    <trkpt lat="51.5019"></trkpt>
  </trkseg></trk>
  <rte><rtept lat="-33.8568" lon="151.2153"/></rte>
</gpx>
)";

}  // namespace

TEST(GpxReader, ParseNumber)
{
    double v = 0;
    auto parse = [&](const char* s) { return GpxReader::parseNumber(s, s + std::strlen(s), v); };

    ASSERT_TRUE(parse("51.5007"));
    EXPECT_DOUBLE_EQ(v, 51.5007);
    ASSERT_TRUE(parse("-0.1246"));
    EXPECT_NEAR(v, -0.1246, 1e-12);
    ASSERT_TRUE(parse("  +12"));
    EXPECT_DOUBLE_EQ(v, 12.0);
    ASSERT_TRUE(parse("1.5e1"));
    EXPECT_DOUBLE_EQ(v, 15.0);
    ASSERT_TRUE(parse("25E-1"));
    EXPECT_DOUBLE_EQ(v, 2.5);
    ASSERT_TRUE(parse(".5"));
    EXPECT_DOUBLE_EQ(v, 0.5);

    EXPECT_FALSE(parse(""));
    EXPECT_FALSE(parse("-"));
    EXPECT_FALSE(parse("abc"));
    EXPECT_FALSE(parse("1e"));
}

TEST(GpxReader, ReadsTrackAndRoutePointsOnly)
{
    Points pts;
    GpxReader r(collect, &pts);
    ASSERT_TRUE(r.feed(kGpx, std::strlen(kGpx)));

    ASSERT_EQ(pts.size(), 5u);
    EXPECT_NEAR(pts[0].first, 51.5007, 1e-9);
    EXPECT_NEAR(pts[0].second, -0.1246, 1e-9);
    EXPECT_NEAR(pts[1].first, 51.5010, 1e-9);   // lon before lat.
    EXPECT_NEAR(pts[1].second, -0.1250, 1e-9);
    EXPECT_NEAR(pts[2].first, 51.5013, 1e-9);   // Single quotes, spaces, self-closing.
    EXPECT_NEAR(pts[3].first, 51.5016, 1e-9);   // Namespace prefix.
    EXPECT_NEAR(pts[4].first, -33.8568, 1e-9);  // rtept.
    EXPECT_NEAR(pts[4].second, 151.2153, 1e-9);

    EXPECT_EQ(r.pointsRead(), 5u);
    EXPECT_EQ(r.pointsRejected(), 2u);  // lat 95, and no lon.
}

TEST(GpxReader, ChunkingDoesNotMatter)
{
    Points whole;
    {
        GpxReader r(collect, &whole);
        r.feed(kGpx, std::strlen(kGpx));
    }
    for (size_t chunk : {size_t{1}, size_t{3}, size_t{7}, size_t{64}}) {
        Points pts;
        GpxReader r(collect, &pts);
        const size_t n = std::strlen(kGpx);
        for (size_t at = 0; at < n; at += chunk) {
            r.feed(kGpx + at, std::min(chunk, n - at));
        }
        EXPECT_EQ(pts, whole) << "chunk " << chunk;
    }
}

TEST(GpxReader, CallbackCanStop)
{
    int seen = 0;
    GpxReader r(
        [](void* ctx, double, double) {
            return ++*static_cast<int*>(ctx) < 2;
        },
        &seen);
    EXPECT_FALSE(r.feed(kGpx, std::strlen(kGpx)));
    EXPECT_EQ(seen, 2);
    EXPECT_FALSE(r.feed(kGpx, std::strlen(kGpx)));  // Ignored once stopped.
    EXPECT_EQ(seen, 2);
}

TEST(GpxReader, LongTagsAreCutSafely)
{
    std::string gpx = "<trkpt lat=\"1.0\" lon=\"2.0\" ";
    gpx += std::string(1000, 'x');
    gpx += "=\"y\"></trkpt><trkpt ";
    gpx += std::string(400, ' ');
    gpx += "lat=\"3\" lon=\"4\"/>";
    Points pts;
    GpxReader r(collect, &pts);
    r.feed(gpx.data(), gpx.size());
    ASSERT_EQ(pts.size(), 1u);  // The second tag's attributes are past the cut.
    EXPECT_DOUBLE_EQ(pts[0].first, 1.0);
    EXPECT_EQ(r.pointsRejected(), 1u);
}
