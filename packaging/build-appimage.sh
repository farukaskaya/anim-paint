#!/usr/bin/env bash
# Build a self-contained Linux AppImage for AnimPaint.
# Requires: gcc, make, libsdl2-dev, curl (or APPIMAGETOOL pointing to appimagetool)
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="${VERSION:-1.0.0}"
ARCH="${ARCH:-x86_64}"
DIST="$ROOT/dist"
APPDIR="$DIST/AnimPaint.AppDir"
OUT="$DIST/AnimPaint-${VERSION}-${ARCH}.AppImage"

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/lib" \
         "$APPDIR/usr/share/applications" \
         "$APPDIR/usr/share/icons/hicolor/256x256/apps"

make -C "$ROOT" clean all

cp "$ROOT/animpaint" "$APPDIR/usr/bin/animpaint"
cp "$ROOT/packaging/AppRun" "$APPDIR/AppRun"
chmod +x "$APPDIR/AppRun" "$APPDIR/usr/bin/animpaint"

cp "$ROOT/packaging/animpaint.desktop" "$APPDIR/animpaint.desktop"
cp "$ROOT/packaging/animpaint.desktop" "$APPDIR/usr/share/applications/animpaint.desktop"
cp "$ROOT/assets/animpaint.png" "$APPDIR/animpaint.png"
cp "$ROOT/assets/animpaint.png" "$APPDIR/usr/share/icons/hicolor/256x256/apps/animpaint.png"

# Bundle the SDL2 shared library if it is dynamically linked.
SDL_LIB="$(ldd "$ROOT/animpaint" | awk '/libSDL2/ { print $3; exit }')"
if [ -n "$SDL_LIB" ] && [ -f "$SDL_LIB" ]; then
    cp -L "$SDL_LIB" "$APPDIR/usr/lib/"
fi

TOOL="${APPIMAGETOOL:-}"
if [ -z "$TOOL" ]; then
    # Keep the tool outside dist/ so it is never uploaded as a release asset.
    CACHE_DIR="${XDG_CACHE_HOME:-$HOME/.cache}/anim-paint"
    mkdir -p "$CACHE_DIR"
    TOOL="$CACHE_DIR/appimagetool-${ARCH}.AppImage"
    if [ ! -x "$TOOL" ]; then
        echo "Downloading appimagetool..."
        curl -fL -o "$TOOL" \
            "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-${ARCH}.AppImage"
        chmod +x "$TOOL"
    fi
fi

mkdir -p "$DIST"
ARCH="$ARCH" APPIMAGE_EXTRACT_AND_RUN=1 "$TOOL" "$APPDIR" "$OUT"
echo "Created $OUT"
