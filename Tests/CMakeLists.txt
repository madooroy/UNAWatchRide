# Host unit tests for the cycling logic in Software/Libs/*/Ride.
#
#   export UNA_SDK=/path/to/una-sdk
#   cmake -S Tests -B Tests/build && cmake --build Tests/build -j && ctest --test-dir Tests/build
#
# The Ride classes are plain C++17 with no kernel, sensor or GUI dependency,
# so they build with the host compiler. The SDK workout sources they use are
# compiled in from $UNA_SDK.
cmake_minimum_required(VERSION 3.21)
project(una-cycling-host-tests CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

if(NOT DEFINED UNA_SDK)
    set(UNA_SDK "$ENV{UNA_SDK}")
endif()
if(NOT IS_DIRECTORY "${UNA_SDK}/Libs/Header/SDK")
    message(FATAL_ERROR "Set UNA_SDK to the una-sdk checkout (got '${UNA_SDK}').")
endif()

set(APP_LIBS "${CMAKE_CURRENT_LIST_DIR}/../Software/Libs")

include(FetchContent)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        52eb8108c5bdec04579160ae17225d66034bd723  # Same pin as the SDK's host tests.
)
FetchContent_MakeAvailable(googletest)

add_executable(cycling-tests
    ClimbDetector_test.cpp
    HrZoneCoach_test.cpp
    IntervalSession_test.cpp
    GpxReader_test.cpp
    Route_test.cpp
    RouteFollower_test.cpp

    "${APP_LIBS}/Sources/Ride/ClimbDetector.cpp"
    "${APP_LIBS}/Sources/Ride/HrZoneCoach.cpp"
    "${APP_LIBS}/Sources/Ride/IntervalSession.cpp"
    "${APP_LIBS}/Sources/Ride/GpxReader.cpp"
    "${APP_LIBS}/Sources/Ride/Route.cpp"
    "${APP_LIBS}/Sources/Ride/RouteFollower.cpp"

    "${UNA_SDK}/Libs/Source/Workout/WorkoutEngine.cpp"
    "${UNA_SDK}/Libs/Source/Workout/Intervals.cpp"
    "${UNA_SDK}/Libs/Source/Workout/TargetGauge.cpp"
    "${UNA_SDK}/Libs/Source/Workout/WorkoutAlerts.cpp"
)

target_include_directories(cycling-tests PRIVATE
    "${APP_LIBS}/Header"
    "${UNA_SDK}/Libs/Header"
)

target_link_libraries(cycling-tests PRIVATE GTest::gtest_main)

enable_testing()
include(GoogleTest)
gtest_discover_tests(cycling-tests)
