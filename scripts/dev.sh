#!/bin/bash
# ViewCam dev runner — build Debug and launch with QML hot-reload.
# Hot-reload is compile-time: Debug builds auto-watch qml/ and reload on save.
# No env vars needed. Just edit qml/*.qml and watch the window update.
#
# Usage:
#   scripts/dev.sh            # build (if needed) then run
#   scripts/dev.sh --no-build # run existing binary without rebuilding
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="$PROJECT_DIR/build"
LAUNCHER="$PROJECT_DIR/bin/viewcam.sh"
BIN="$PROJECT_DIR/bin/ViewCam"

if [[ "${1:-}" != "--no-build" ]]; then
    echo "[dev] Configuring Debug build..."
    cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug -Wno-dev --fresh 2>/dev/null \
        || cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Debug -Wno-dev
    echo "[dev] Building..."
    cmake --build "$BUILD_DIR" -j8
fi

if [ ! -x "$BIN" ]; then
    echo "ERROR: $BIN not found. Build failed?" >&2
    exit 1
fi

echo "[dev] Hot-reload: ACTIVE (watching qml/ — edit any .qml and save)"
echo "[dev] Binary: $BIN"
echo ""

if [ -x "$LAUNCHER" ]; then
    exec "$LAUNCHER"
else
    exec "$BIN"
fi
