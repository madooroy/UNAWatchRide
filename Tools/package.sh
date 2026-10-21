#!/usr/bin/env bash
# Build the watch app and pack the community-app-store upload:
#   Output/Ride_<version>_store.zip = Ride_<version>.uapp + app-manifest.json
#                                     + icon.png + previews/
#
# Usage: UNA_SDK=/path/to/una-sdk Tools/package.sh
#        Tools/package.sh --manifest-only   # write Output/app-manifest.json only
#
# Needs ST's ARM toolchain (STM32CubeCLT / STM32CubeIDE) on PATH for the build;
# see README.md. The app version and App ID come from
# Software/Apps/Cycling-CMake/CMakeLists.txt (BUILD_VERSION, APP_ID).
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
: "${UNA_SDK:?Set UNA_SDK to the una-sdk checkout}"
CMAKELISTS="$ROOT/Software/Apps/Cycling-CMake/CMakeLists.txt"
BUILD="$ROOT/build"
PLACEHOLDER_ID="E504A310DAC4B452"

APP_ID=$(sed -n 's/^set(APP_ID "\([0-9A-F]*\)").*/\1/p' "$CMAKELISTS")
VERSION=$(sed -n 's/^set(BUILD_VERSION "\([^"]*\)").*/\1/p' "$CMAKELISTS")
[ -n "$APP_ID" ] && [ -n "$VERSION" ] || { echo "Cannot read APP_ID/BUILD_VERSION from $CMAKELISTS" >&2; exit 1; }
NAME="Ride_${VERSION}.uapp"

if [ "$APP_ID" = "$PLACEHOLDER_ID" ]; then
    echo "WARNING: APP_ID is still the local placeholder ($APP_ID)." >&2
    echo "         Generate the App ID for \"Ride\" on apps.unawatch.com and put it in" >&2
    echo "         $CMAKELISTS before uploading." >&2
fi

write_manifest() {
    cat > "$1" <<JSON
{
  "manifest_version": 1,
  "type": ["activity"],
  "name": "Ride",
  "icon": "icon.png",
  "binary": "$NAME",
  "previews": "previews/",
  "appVersion": "$VERSION",
  "minKernelVersion": "1.4.0",
  "requiredHardware": ["GPS"],
  "description": "A cycling computer for your wrist. Everything a ride needs (speed, distance, heart rate with zones, elevation, auto-pause, auto-laps, external HR straps) plus: climb detection with live grade, climb gain, average grade and VAM, and a buzz at the start and summit of every climb; route following from a GPX file, with a route map, your position, distance to go and off-course alerts; five interval presets with countdown beeps, each step saved as its own lap; and heart-rate zone coaching that tells you when you drift out of your target zone. FIT files carry grade on every point and lap intensity, ready for Strava and training apps.",
  "stravaExport": true,
  "id": "$APP_ID",
  "supportsLaps": true,
  "supportsDistance": true,
  "supportsTrack": true,
  "supportsHeartbeat": true,
  "supportsElevation": true,
  "supportsStep": false,
  "supportsSpeed": "speed",
  "customMeasures": []
}
JSON
    # Raise minKernelVersion to the floor the SDK's ABI requires.
    python3 "$UNA_SDK/Utilities/Scripts/app_packer/min_kernel_version.py" --stamp "$1"
}

mkdir -p "$ROOT/Output"

if [ "${1:-}" = "--manifest-only" ]; then
    write_manifest "$ROOT/Output/app-manifest.json"
    echo "Manifest: $ROOT/Output/app-manifest.json"
    exit 0
fi

cmake -G "Unix Makefiles" -S "$ROOT/Software/Apps/Cycling-CMake" -B "$BUILD"
cmake --build "$BUILD" -j"$(nproc 2>/dev/null || echo 4)"

UAPP=$(ls -t "$BUILD"/Ride_*.uapp 2>/dev/null | head -1 || true)
[ -n "$UAPP" ] || { echo "No Ride_*.uapp in $BUILD: did the build succeed?" >&2; exit 1; }
NAME=$(basename "$UAPP")

STAGE=$(mktemp -d)
trap 'rm -rf "$STAGE"' EXIT
cp "$UAPP" "$STAGE/"
cp "$ROOT/Resources/icon.png" "$STAGE/icon.png"
mkdir -p "$STAGE/previews"
cp "$ROOT"/docs/store/store_*.png "$STAGE/previews/"
write_manifest "$STAGE/app-manifest.json"

ZIP="$ROOT/Output/Ride_${VERSION}_store.zip"
rm -f "$ZIP"
(cd "$STAGE" && zip -qr "$ZIP" .)
cp "$UAPP" "$ROOT/Output/"
echo "Store package: $ZIP"
unzip -l "$ZIP"
