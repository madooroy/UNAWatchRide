#include <gui/containers/TrackFaceRoute.hpp>
#include <gui/model/Model.hpp>
#include <SDK/GUI/Color.hpp>
#include <touchgfx/widgets/canvas/CWRUtil.hpp>

TrackFaceRoute::TrackFaceRoute()
{
    setWidth(240);
    setHeight(240);

    mTrack.setPosition(kMapX, kMapY, kMapSize, kMapSize);
    mTrack.setLineWidth(3);
    mTrack.setColor(RideUi::rgb(SDK::GUI::Color::TEAL));
    add(mTrack);

    // Start of the route: a small hollow-looking grey dot.
    mStart.setPosition(0, 0, 2 * 4 + 2, 2 * 4 + 2);
    mStart.setCenter(5, 5);
    mStart.setRadius(4);
    mStart.setLineWidth(0);
    mStartPainter.setColor(RideUi::rgb(SDK::GUI::Color::GRAY));
    mStart.setPainter(mStartPainter);
    mStart.setVisible(false);
    add(mStart);

    // The rider.
    mDot.setPosition(0, 0, 2 * kDotR + 2, 2 * kDotR + 2);
    mDot.setCenter(kDotR + 1, kDotR + 1);
    mDot.setRadius(kDotR);
    mDot.setLineWidth(0);
    mDotPainter.setColor(RideUi::rgb(SDK::GUI::Color::YELLOW_DARK));
    mDot.setPainter(mDotPainter);
    mDot.setVisible(false);
    add(mDot);

    mValue.init(*this, 55, 160, 115, 42, T_TMP_SEMIBOLD_35);
    mUnit.init(*this, 172, 178, 45, 19, T_TMP_REGULAR_18_L);
    mLabel.init(*this, 30, 204, 180, 23, T_TMP_ITALIC_18);
}

void TrackFaceRoute::setRoute(const Ride::RouteShape& shape)
{
    mHasShape = shape.count >= 2;
    if (mHasShape) {
        mTrack.setPoints(reinterpret_cast<const uint8_t*>(shape.points), shape.count);
        mStart.moveTo(kMapX + shape.points[0].x - 5, kMapY + shape.points[0].y - 5);
    }
    mTrack.setVisible(mHasShape);
    mStart.setVisible(mHasShape);
    invalidate();
}

void TrackFaceRoute::setData(const Ride::Data& d, bool isImperial)
{
    char s[24];

    // Rider dot.
    const bool showDot = mHasShape && d.routeMatched;
    if (showDot) {
        const int16_t x = kMapX + Model::kRouteInset + d.riderPx - (kDotR + 1);
        const int16_t y = kMapY + Model::kRouteInset + d.riderPy - (kDotR + 1);
        if (!mDot.isVisible() || mDot.getX() != x || mDot.getY() != y) {
            mDot.invalidate();
            mDot.setXY(x, y);
            mDot.setVisible(true);
            mDot.invalidate();
        }
        // Red when the rider is off the route or off the edge of the map.
        const bool red = d.offCourse || !d.riderInside;
        if (red != mDotRed) {
            mDotRed = red;
            mDotPainter.setColor(RideUi::rgb(red ? SDK::GUI::Color::RED : SDK::GUI::Color::YELLOW_DARK));
            mDot.invalidate();
        }
    } else if (mDot.isVisible()) {
        mDot.setVisible(false);
        mDot.invalidate();
    }

    const auto state = static_cast<Ride::RouteState>(d.routeState);
    if (state == Ride::RouteState::Failed) {
        mValue.set("---");
        mUnit.set("");
        mLabel.set("Route file unreadable");
        mLabel.setColor(RideUi::rgb(SDK::GUI::Color::RED));
        return;
    }

    if (d.arrived) {
        mValue.set("0.00");
        mUnit.set(isImperial ? "mi" : "km");
        mLabel.set("Route complete");
        mLabel.setColor(RideUi::rgb(SDK::GUI::Color::GREEN));
        return;
    }

    if (d.offCourse) {
        // How far off, in m or ft.
        RideUi::height(s, sizeof(s), d.routeOffM, isImperial, false);
        mValue.set(s);
        mUnit.set(isImperial ? "ft" : "m");
        mLabel.set("Off course");
        mLabel.setColor(RideUi::rgb(SDK::GUI::Color::RED));
        mValue.setColor(RideUi::rgb(SDK::GUI::Color::RED));
        return;
    }

    mValue.setColor(RideUi::textColor());
    RideUi::distance(s, sizeof(s), d.routeRemainingM, isImperial);
    mValue.set(s);
    mUnit.set(isImperial ? "mi" : "km");
    mLabel.set(d.routeMatched ? "To go" : "Finding route...");
    mLabel.setColor(RideUi::textColor());
}
