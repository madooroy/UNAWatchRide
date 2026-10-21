/**
 ******************************************************************************
 * @file    RideData.hpp
 * @brief   What the service tells the GUI about the Ride features each
 *          second: grade and climbs, HR zone coaching, intervals and route
 *          following. Kept apart from Track::Data so each message stays well
 *          inside the kernel's 256-byte message block.
 ******************************************************************************
 */

#ifndef RIDE_DATA_HPP
#define RIDE_DATA_HPP

#include <cstdint>

namespace Ride {

/// Route state, as the GUI shows it.
enum class RouteState : uint8_t {
    None = 0,    ///< No route.gpx in the app's folder.
    Disabled,    ///< Route following is switched off in Settings.
    Failed,      ///< route.gpx is there but has no usable track or route points.
    Loaded,      ///< Ready; becomes Following when a ride starts.
    Following,   ///< A ride is following it.
};

/// What a one-off alert was about, for the GUI's banner.
enum class AlertKind : uint8_t {
    None = 0,
    ClimbStarted,
    ClimbFinished,   ///< a = distance m, b = gain m, c = average grade %.
    OffCourse,       ///< a = distance from the route, m.
    BackOnCourse,
    RouteArrived,
    HrAbove,         ///< a = target zone.
    HrBelow,         ///< a = target zone.
    HrBackIn,        ///< a = target zone.
    IntervalStep,    ///< a = phase (IntervalSession::Phase), b = rep, c = reps.
};

struct Data {
    // -- Grade and climbs ------------------------------------------------------
    float    gradePct      = 0.0f;   ///< Live grade, percent (kernel's if it has one).
    float    climbDistM    = 0.0f;   ///< Current climb so far.
    float    climbGainM    = 0.0f;
    float    climbGradePct = 0.0f;   ///< Current climb's average grade.
    float    climbVamMph   = 0.0f;   ///< Current climb's VAM, m/h (0 until meaningful).
    float    climbedGainM  = 0.0f;   ///< Sum of finished climbs' gains.
    bool     gradeValid    = false;
    bool     inClimb       = false;
    uint8_t  climbCount    = 0;      ///< Finished climbs (saturates at 255).
    uint8_t  reserved0     = 0;

    // -- Heart-rate zone coaching ---------------------------------------------
    uint8_t  hrZone        = 0;      ///< Zone of the current heart rate; 0 = below zone 1 or none.
    uint8_t  hrTargetZone  = 0;      ///< 0 = coaching off.
    uint8_t  hrTargetState = 0;      ///< SDK::Workout::Zone of the HR against the target.
    uint8_t  reserved1     = 0;

    // -- Intervals --------------------------------------------------------------
    uint8_t  ivPhase       = 0;      ///< IntervalSession::Phase; 0 = no session.
    uint8_t  ivPreset      = 0;
    uint16_t ivRep         = 0;
    uint16_t ivReps        = 0;
    uint16_t ivRemainingSec = 0;     ///< 0 for an open step.
    uint32_t ivStepSec     = 0;      ///< Active seconds in the current step.

    // -- Route --------------------------------------------------------------------
    float    routeOffM       = 0.0f;
    float    routeAlongM     = 0.0f;
    float    routeRemainingM = 0.0f;
    float    routeLengthM    = 0.0f;
    uint8_t  routeState    = 0;      ///< RouteState.
    bool     routeMatched  = false;  ///< The rider has been placed on the route.
    bool     offCourse     = false;
    bool     arrived       = false;
    uint8_t  riderPx       = 0;      ///< Rider on the RouteShape's pixels.
    uint8_t  riderPy       = 0;
    bool     riderInside   = false;  ///< Within the route's box (else clamped to its edge).
    uint8_t  reserved2     = 0;
};

static_assert(sizeof(Data) <= 128, "Ride::Data must leave room in a 256-byte message block");

}  // namespace Ride

#endif  // RIDE_DATA_HPP
