# Watch App CMakeLists.txt - Simplified version using SDK
cmake_minimum_required(VERSION 3.21)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")

# App configuration
#
# This app is derived from UNA's Cycling example but has its own identity, so
# it installs next to the stock app instead of replacing it. APP_NAME stays
# "Cycling": it only names the build targets and ELFs (and the simulator
# Makefile's sources), not anything on the watch.
set(APP_NAME "Cycling")
set(APP_USER_NAME "Ride")          # Launcher name.
set(APP_FILE_NAME "Ride")          # Ride.uapp; install it in Apps/Ride/ on the watch.
set(APP_TYPE "Activity")
set(DEV_ID "MADOOROY")
# Placeholder: md5("UnaWatchCycling")[:16], per Docs/sdk-setup.md. Before
# publishing, replace it with the App ID generated for "Ride" on
# apps.unawatch.com (Docs/deploy.md); Tools/package.sh reads it from here.
set(APP_ID "E504A310DAC4B452")
set(BUILD_VERSION "1.0.0")         # App version shown in the store; bump per release.
set(RESOURCES_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../Resources")
set(OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../Output")
set(LIBS_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../Libs")
set(TOUCHGFX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../TouchGFX-GUI")

# Includes all build steps and required tools
# Sets toolchain before project
include($ENV{UNA_SDK}/cmake/una-app.cmake)

# Includes sets: SCRIPTS_PATH,
# UNA_SDK_SOURCES_COMMON, UNA_SDK_SOURCES_SERVICE, UNA_SDK_SOURCES_GUI,
# UNA_SDK_INCLUDE_DIRS_COMMON, UNA_SDK_INCLUDE_DIRS_SERVICE, UNA_SDK_INCLUDE_DIRS_GUI
include($ENV{UNA_SDK}/cmake/una-sdk.cmake)

# Includes LIBS_SOURCES, LIBS_INCLUDE_DIRS
include(${LIBS_PATH}/libs.cmake)

# Includes TOUCHGFX_SOURCES, TOUCHGFX_INCLUDE_DIRS
if(DEFINED TOUCHGFX_PATH)
    include(${TOUCHGFX_PATH}/touchgfx.cmake)
endif()

project(${APP_NAME})

# Set up version
una_app_setup_version(BUILD_VERSION ${CMAKE_CURRENT_SOURCE_DIR})

# Build service
set(SERVICE_SOURCES
    ${LIBS_SOURCES}
    ${UNA_SDK_SOURCES_COMMON}
    ${UNA_SDK_SOURCES_SERVICE}
)
set(SERVICE_INCLUDE_DIRS
    ${LIBS_INCLUDE_DIRS}
    ${UNA_SDK_INCLUDE_DIRS_COMMON}
    ${UNA_SDK_INCLUDE_DIRS_SERVICE}
    "${CMAKE_CURRENT_SOURCE_DIR}/../TouchGFX-GUI/gui/include"
)
una_app_build_service(${APP_NAME}Service.elf)


if(DEFINED TOUCHGFX_PATH)
    set(GUI_SOURCES
        ${TOUCHGFX_SOURCES}
        ${UNA_SDK_SOURCES_COMMON}
        ${UNA_SDK_SOURCES_GUI}
    )
    set(GUI_INCLUDE_DIRS
        ${LIBS_INCLUDE_DIRS}
        ${UNA_SDK_INCLUDE_DIRS_COMMON}
        ${UNA_SDK_INCLUDE_DIRS_GUI}
        ${TOUCHGFX_INCLUDE_DIRS}
    )
    una_app_build_gui(${APP_NAME}GUI.elf)
endif()

# Build the complete app using SDK functions
una_app_build_app()