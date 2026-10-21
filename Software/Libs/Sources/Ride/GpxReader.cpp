/**
 ******************************************************************************
 * @file    GpxReader.cpp
 * @brief   See GpxReader.hpp.
 ******************************************************************************
 */

#include "Ride/GpxReader.hpp"

#include <cstring>

namespace Ride {

namespace {

bool isSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool isDigit(char c)
{
    return c >= '0' && c <= '9';
}

}  // namespace

bool GpxReader::parseNumber(const char* s, const char* end, double& out)
{
    while (s < end && isSpace(*s)) {
        ++s;
    }

    bool neg = false;
    if (s < end && (*s == '-' || *s == '+')) {
        neg = (*s == '-');
        ++s;
    }

    double value  = 0.0;
    bool   digits = false;
    while (s < end && isDigit(*s)) {
        value  = value * 10.0 + (*s - '0');
        digits = true;
        ++s;
    }
    if (s < end && *s == '.') {
        ++s;
        double scale = 0.1;
        while (s < end && isDigit(*s)) {
            value += (*s - '0') * scale;
            scale *= 0.1;
            digits = true;
            ++s;
        }
    }
    if (!digits) {
        return false;
    }
    if (s < end && (*s == 'e' || *s == 'E')) {
        ++s;
        bool eneg = false;
        if (s < end && (*s == '-' || *s == '+')) {
            eneg = (*s == '-');
            ++s;
        }
        int exp = 0;
        bool edigits = false;
        while (s < end && isDigit(*s)) {
            if (exp < 400) {
                exp = exp * 10 + (*s - '0');
            }
            edigits = true;
            ++s;
        }
        if (!edigits) {
            return false;
        }
        double p = 1.0;
        for (int i = 0; i < exp; ++i) {
            p *= 10.0;
        }
        value = eneg ? value / p : value * p;
    }

    out = neg ? -value : value;
    return true;
}

bool GpxReader::findAttr(const char* tag, size_t len, const char* name, double& out)
{
    const size_t nlen = std::strlen(name);
    const char*  end  = tag + len;

    for (const char* p = tag; p + nlen < end; ++p) {
        // Whole attribute name only: preceded by whitespace, so "lat" never
        // matches inside "xlat" or a value.
        if (!isSpace(*p) || std::strncmp(p + 1, name, nlen) != 0) {
            continue;
        }
        const char* q = p + 1 + nlen;
        while (q < end && isSpace(*q)) {
            ++q;
        }
        if (q >= end || *q != '=') {
            continue;
        }
        ++q;
        while (q < end && isSpace(*q)) {
            ++q;
        }
        if (q >= end || (*q != '"' && *q != '\'')) {
            continue;
        }
        const char  quote = *q++;
        const char* vend  = q;
        while (vend < end && *vend != quote) {
            ++vend;
        }
        return parseNumber(q, vend, out);
    }
    return false;
}

void GpxReader::endTag()
{
    if (mLen == 0) {
        return;
    }
    const char first = mTag[0];
    if (first == '/' || first == '?' || first == '!') {
        return;  // Closing tag, declaration, comment or CDATA.
    }

    // Element name, without any namespace prefix.
    size_t nameEnd = 0;
    while (nameEnd < mLen && !isSpace(mTag[nameEnd]) && mTag[nameEnd] != '/') {
        ++nameEnd;
    }
    size_t nameStart = 0;
    for (size_t i = 0; i < nameEnd; ++i) {
        if (mTag[i] == ':') {
            nameStart = i + 1;
        }
    }
    const size_t nameLen = nameEnd - nameStart;
    if (nameLen != 5) {
        return;
    }
    const char* name = mTag + nameStart;
    if (std::strncmp(name, "trkpt", 5) != 0 && std::strncmp(name, "rtept", 5) != 0) {
        return;
    }

    double lat = 0.0;
    double lon = 0.0;
    if (!findAttr(mTag, mLen, "lat", lat) || !findAttr(mTag, mLen, "lon", lon) ||
        lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
        ++mRejected;
        return;
    }

    ++mPoints;
    if (!mFn(mCtx, lat, lon)) {
        mStopped = true;
    }
}

bool GpxReader::feed(const char* data, size_t len)
{
    for (size_t i = 0; i < len && !mStopped; ++i) {
        const char c = data[i];

        if (!mInTag) {
            if (c == '<') {
                mInTag   = true;
                mInQuote = false;
                mLen     = 0;
            }
            continue;
        }

        if (mInQuote) {
            if (c == mQuote) {
                mInQuote = false;
            }
        } else if (c == '"' || c == '\'') {
            mInQuote = true;
            mQuote   = c;
        } else if (c == '>') {
            mTag[mLen < kTagMax ? mLen : kTagMax - 1] = '\0';
            endTag();
            mInTag = false;
            continue;
        }

        if (mLen < kTagMax - 1) {
            mTag[mLen++] = c;
        }
    }
    return !mStopped;
}

}  // namespace Ride
