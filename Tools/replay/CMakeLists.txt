# Ride replay: runs a recorded ride through the watch's climb detection and
# route following on a PC. See README.md in this folder.
#
#   cmake -S Tools/replay -B Tools/replay/build && cmake --build Tools/replay/build
cmake_minimum_required(VERSION 3.21)
project(ride-replay CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT MSVC)
    add_compile_options(-Wall -Wextra -O2)
endif()

if(NOT DEFINED UNA_SDK)
    set(UNA_SDK "$ENV{UNA_SDK}")
endif()
if(NOT IS_DIRECTORY "${UNA_SDK}/Libs/Header/SDK")
    message(FATAL_ERROR "Set UNA_SDK to the una-sdk checkout (got '${UNA_SDK}').")
endif()

set(APP_LIBS "${CMAKE_CURRENT_LIST_DIR}/../../Software/Libs")

add_executable(replay
    replay.cpp
    "${APP_LIBS}/Sources/Ride/ClimbDetector.cpp"
    "${APP_LIBS}/Sources/Ride/GpxReader.cpp"
    "${APP_LIBS}/Sources/Ride/Route.cpp"
    "${APP_LIBS}/Sources/Ride/RouteFollower.cpp"
)
target_include_directories(replay PRIVATE "${APP_LIBS}/Header" "${UNA_SDK}/Libs/Header")
