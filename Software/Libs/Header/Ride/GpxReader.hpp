/**
 ******************************************************************************
 * @file    GpxReader.hpp
 * @brief   Streaming reader for the track and route points of a GPX file.
 *
 * GPX files from Strava, Komoot, RideWithGPS, Garmin Connect and friends can
 * run to megabytes, far more than the watch should hold at once, so the file
 * is fed in chunks of any size and every <trkpt> or <rtept> found is handed to
 * a callback as it completes. Waypoints (<wpt>) are not part of the path and
 * are skipped, as is everything else (elevation, time, extensions).
 *
 * Only the tag's lat and lon attributes are read. Namespace prefixes
 * ("gpx:trkpt") are accepted. Numbers are parsed here, not with strtof, so the
 * result does not depend on the C library's locale or float formatting.
 *
 * Allocation-free; about 200 bytes.
 ******************************************************************************
 */

#ifndef RIDE_GPX_READER_HPP
#define RIDE_GPX_READER_HPP

#include <cstddef>
#include <cstdint>

namespace Ride {

class GpxReader {
public:
    /// Called for each path point, in file order. Return false to stop.
    using PointFn = bool (*)(void* ctx, double lat, double lon);

    GpxReader(PointFn fn, void* ctx) : mFn(fn), mCtx(ctx) {}

    /// Feed the next @p len bytes of the file. False once the callback asked
    /// to stop; further input is then ignored.
    bool feed(const char* data, size_t len);

    uint32_t pointsRead() const { return mPoints; }
    uint32_t pointsRejected() const { return mRejected; }  ///< Tags with bad lat/lon.

    /// Parse a decimal number ("-12.5", "3e-2") from @p s up to @p end.
    /// False if no number is there. Exposed for tests.
    static bool parseNumber(const char* s, const char* end, double& out);

private:
    static constexpr size_t kTagMax = 192;  ///< Longer tags are cut; lat/lon come early.

    void endTag();
    static bool findAttr(const char* tag, size_t len, const char* name, double& out);

    PointFn  mFn;
    void*    mCtx;
    bool     mStopped  = false;
    bool     mInTag    = false;
    bool     mInQuote  = false;
    char     mQuote    = 0;
    size_t   mLen      = 0;
    char     mTag[kTagMax] = {};
    uint32_t mPoints   = 0;
    uint32_t mRejected = 0;
};

}  // namespace Ride

#endif  // RIDE_GPX_READER_HPP
