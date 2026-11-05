#!/usr/bin/env bash
# Build a portable Windows x86_64 release from MSYS2 / MinGW-w64.
# Run this from an "MSYS2 MINGW64" shell after installing:
#   pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 make zip
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="${VERSION:-1.0.0}"
DIST="$ROOT/dist"
PKG="$DIST/AnimPaint-${VERSION}-windows-x86_64"

rm -rf "$PKG"
mkdir -p "$PKG"

make -C "$ROOT" clean all

if [ -f "$ROOT/animpaint.exe" ]; then
    BIN="$ROOT/animpaint.exe"
elif [ -f "$ROOT/animpaint" ]; then
    BIN="$ROOT/animpaint"
else
    echo "Build failed: animpaint executable not found" >&2
    exit 1
fi

cp "$BIN" "$PKG/AnimPaint.exe"
cp "$ROOT/README.md" "$ROOT/LICENSE" "$PKG/" 2>/dev/null || true

# Ship SDL2.dll next to the executable so the release is portable.
GCC_BIN="$(dirname "$(command -v gcc)")"
for CANDIDATE in "$GCC_BIN/SDL2.dll" /mingw64/bin/SDL2.dll; do
    if [ -f "$CANDIDATE" ]; then
        cp -L "$CANDIDATE" "$PKG/"
        break
    fi
done
if [ ! -f "$PKG/SDL2.dll" ]; then
    echo "WARNING: SDL2.dll was not found; the portable zip may not run standalone." >&2
fi

mkdir -p "$DIST"
( cd "$DIST" && zip -r "AnimPaint-${VERSION}-windows-x86_64.zip" "AnimPaint-${VERSION}-windows-x86_64" )

# Also publish the bare executable for users who already have SDL2.dll.
cp "$PKG/AnimPaint.exe" "$DIST/AnimPaint-${VERSION}-windows-x86_64.exe"
echo "Created $DIST/AnimPaint-${VERSION}-windows-x86_64.zip"
