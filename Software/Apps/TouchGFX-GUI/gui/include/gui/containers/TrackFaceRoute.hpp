#ifndef TRACKFACEROUTE_HPP
#define TRACKFACEROUTE_HPP

#include <gui/containers/RideUi.hpp>
#include <gui/containers/PolyLine.hpp>
#include <touchgfx/widgets/canvas/Circle.hpp>
#include "RideData.hpp"
#include "Ride/Route.hpp"

/**
 * @brief Track face for following a route: the whole route, north up, with
 *        the rider's position, and the distance still to ride.
 *
 * The route is drawn from the RouteShape the service built (a 144 px square,
 * already centred), inset by Model::kRouteInset into the 150 px map area. Off course, the distance line
 * turns into a red "Off course" warning with the distance from the route.
 */
class TrackFaceRoute : public touchgfx::Container
{
public:
    TrackFaceRoute();

    /// The route to draw. @p shape must stay valid (it is the Model's copy).
    void setRoute(const Ride::RouteShape& shape);

    void setData(const Ride::Data& d, bool isImperial);

private:
    static constexpr int16_t kMapX    = 45;
    static constexpr int16_t kMapY    = 14;
    static constexpr int16_t kMapSize = 150;
    static constexpr int16_t kDotR    = 5;

    PolyLine                   mTrack;
    touchgfx::Circle           mStart;
    touchgfx::PainterABGR2222  mStartPainter;
    touchgfx::Circle           mDot;
    touchgfx::PainterABGR2222  mDotPainter;

    RideUi::Text<16>           mValue;
    RideUi::Text<8>            mUnit;
    RideUi::Text<24>           mLabel;

    bool mHasShape = false;
    bool mDotRed   = false;
};

#endif // TRACKFACEROUTE_HPP
