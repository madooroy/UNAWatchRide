#ifndef RIDEUI_HPP
#define RIDEUI_HPP

/**
 * @brief Helpers for the Ride faces, which are laid out in code rather than in
 *        TouchGFX Designer so that regenerating the Designer project never
 *        touches them.
 *
 * Text goes through the "<>" template texts (T_TMP_*) that the project already
 * has for every font; all of those fonts carry the full printable ASCII range
 * (see texts.xml), so any ASCII string renders. Numbers are formatted here
 * with integer arithmetic: the target's printf has no float support, and the
 * values need at most one decimal anyway.
 */

#include <touchgfx/hal/Types.hpp>
#include <touchgfx/TypedText.hpp>
#include <touchgfx/Color.hpp>
#include <touchgfx/Unicode.hpp>
#include <touchgfx/containers/Container.hpp>
#include <touchgfx/widgets/TextAreaWithWildcard.hpp>
#include <touchgfx/widgets/canvas/Line.hpp>
#include <touchgfx/widgets/canvas/PainterABGR2222.hpp>
#include <texts/TextKeysAndLanguages.hpp>

#include <cstdint>
#include <cstdio>

namespace RideUi {

/// Light grey used by every face for text.
inline touchgfx::colortype textColor() { return touchgfx::Color::getColorFromRGB(192, 192, 192); }
/// Teal of the face dividers.
inline touchgfx::colortype lineColor() { return touchgfx::Color::getColorFromRGB(0, 128, 128); }
inline touchgfx::colortype rgb(uint32_t c)
{
    return touchgfx::Color::getColorFromRGB((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

/// A text field with its own wildcard buffer.
template <uint16_t N>
struct Text {
    touchgfx::TextAreaWithOneWildcard area;
    touchgfx::Unicode::UnicodeChar    buf[N] = {};

    void init(touchgfx::Container& parent, int16_t x, int16_t y, int16_t w, int16_t h,
              touchgfx::TypedTextId tmpl, touchgfx::colortype color = textColor())
    {
        area.setPosition(x, y, w, h);
        area.setColor(color);
        area.setLinespacing(0);
        area.setWildcard(buf);
        area.setTypedText(touchgfx::TypedText(tmpl));
        buf[0] = 0;
        parent.add(area);
    }

    /// Set ASCII/UTF-8 text; redraws only when it changed.
    void set(const char* s)
    {
        touchgfx::Unicode::UnicodeChar tmp[N] = {};
        touchgfx::Unicode::fromUTF8(reinterpret_cast<const uint8_t*>(s), tmp, N);
        if (touchgfx::Unicode::strncmp(tmp, buf, N) != 0) {
            area.invalidate();   // Old extent.
            touchgfx::Unicode::strncpy(buf, tmp, N);
            area.invalidate();   // New extent.
        }
    }

    void setColor(touchgfx::colortype c)
    {
        if (area.getColor() != c) {
            area.setColor(c);
            area.invalidate();
        }
    }
};

/// A divider like the Designer faces' (170 px, 10 px wide, round caps).
inline void divider(touchgfx::Container& parent, touchgfx::Line& line,
                    touchgfx::PainterABGR2222& painter, int16_t y)
{
    line.setPosition(35, y, 170, 3);
    painter.setColor(lineColor());
    line.setPainter(painter);
    line.setStart(0, 0);
    line.setEnd(170, 0);
    line.setLineWidth(10);
    line.setLineEndingStyle(touchgfx::Line::ROUND_CAP_ENDING);
    parent.add(line);
}

/// "12.3" / "-4.0": one decimal, rounded half away from zero.
inline void fixed1(char* out, size_t n, float v)
{
    const bool neg = v < 0.0f;
    float mag = neg ? -v : v;
    mag = mag > 99999.0f ? 99999.0f : mag;   // Keeps the text to 8 characters.
    long tenths = static_cast<long>(mag * 10.0f + 0.5f);
    std::snprintf(out, n, "%s%ld.%ld", (neg && tenths != 0) ? "-" : "", tenths / 10, tenths % 10);
}

/// Distance in km or mi from metres: "12.3", or "0.85" under 10.
inline void distance(char* out, size_t n, float metres, bool imperial)
{
    float v = metres / (imperial ? 1609.344f : 1000.0f);
    if (v < 0.0f) {
        v = 0.0f;
    }
    if (v > 9999.0f) {
        v = 9999.0f;
    }
    if (v < 10.0f) {
        const long hundredths = static_cast<long>(v * 100.0f + 0.5f);
        std::snprintf(out, n, "%ld.%02ld", hundredths / 100, hundredths % 100);
    } else {
        fixed1(out, n, v);
    }
}

/// Whole metres or feet, with a sign when asked: "+42".
inline void height(char* out, size_t n, float metres, bool imperial, bool sign)
{
    const float v = imperial ? metres * 3.28084f : metres;
    const long  r = static_cast<long>(v >= 0.0f ? v + 0.5f : v - 0.5f);
    std::snprintf(out, n, sign && r > 0 ? "+%ld" : "%ld", r);
}

/// "m:ss", or "h:mm:ss" from an hour.
inline void duration(char* out, size_t n, uint32_t sec)
{
    if (sec >= 3600u) {
        std::snprintf(out, n, "%lu:%02lu:%02lu", static_cast<unsigned long>(sec / 3600u),
                      static_cast<unsigned long>((sec / 60u) % 60u),
                      static_cast<unsigned long>(sec % 60u));
    } else {
        std::snprintf(out, n, "%lu:%02lu", static_cast<unsigned long>(sec / 60u),
                      static_cast<unsigned long>(sec % 60u));
    }
}

}  // namespace RideUi

#endif  // RIDEUI_HPP
