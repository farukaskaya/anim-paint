# Faruk Paint 🎨🎬

> **No tool bloat. Just the iconic Windows 95/98 MS Paint tools paired with a clean 2-layer animation studio.**

**Faruk Paint** is a native, ultra-lightweight 2D animation suite written in C (using SDL2 and ffmpeg). It replaces the confusing clutter of modern digital art apps with the beloved, laser-focused toolset of classic MS Paint, while adding essential animation superpowers: **onion skinning, a frame-reel timeline, 2 dedicated layers (Background & Animation), and one-click MP4 / GIF export**.

---

## ✨ Key Features

### 🖌️ Classic MS Paint Toolset (Zero Tool Bloat)
1. **Free-Form Select (Lasso)** — draw any outline, then cut, drag and drop the selected pixels.
2. **Rectangular Select** (with transparent selection and move/drag)
3. **Eraser / Color Eraser** (Left click = erase; Right click = replace primary with secondary color)
4. **Fill With Color** (Fast scanline flood fill)
5. **Pick Color (Eyedropper)** (Left click = Primary color, Right click = Secondary color)
6. **Magnifier (Zoom)** (1x, 2x, 4x, 8x with mouse wheel support)
7. **Pencil** (Pixel-perfect 1px freehand)
8. **Brush** (Circle, Square, Calligraphy forward/backward slashes with selectable sizes)
9. **Airbrush** (Classic spray can with small, medium, and large dispersion)
10. **Text Tool** (Embedded 8x8 retro font)
11. **Line** (Bresenham line with thickness options 1px, 2px, 4px, 6px)
12. **Curve** (MS Paint's classic 3-step Bezier curve: start-to-end, bend 1, bend 2)
13. **Rectangle** (Outline only, Outline + Fill, Solid fill)
14. **Polygon** (Multi-point polygon)
15. **Ellipse** (Circle / Ellipse outline or fill)
16. **Rounded Rectangle**

### 🎞️ 2-Layer Animation Architecture
- **Layer 0: Background (`[BG Layer]`)** — Frame-by-frame background environment and scenery.
- **Layer 1: Animation (`[Anim Layer]`)** — Transparent foreground for your moving characters and objects.
- Quick toggle with `Tab` or by clicking the layer buttons in the timeline.

### 🧅 Onion Skinning
- Real-time ghost overlays while drawing.
- **Tinted preview:** Warm red/orange tint for previous frames, cool blue tint for next frames.
- Toggle on/off instantly with the `O` key or the toolbar button.

### 🎬 Real-time Timeline & Controls
- **Frame Reel Strip:** Miniature live thumbnail previews of every frame in your animation.
- **Playback:** Spacebar to Play / Pause at your target framerate.
- **FPS Controller:** Adjustable framerate (1 to 30 FPS).
- **Frame Management:** Add new frames (`+ Frame`), Duplicate (`Dup`), Delete (`Del`), or reorder.

### 🎙️ Voiceover for MP4
- **Record from microphone:** *Voiceover → Record Voiceover* (or `Ctrl + R`), click **STOP** when done. The take is written to `voiceover.wav`.
- **Import a WAV:** drop any `.wav` file onto the window, or place `voiceover.wav` next to the app and use *Voiceover → Import voiceover.wav*.
- **Preview before exporting:** *Voiceover → Preview Voiceover* plays the take through your speakers.
- **MP4 only:** when a voiceover is loaded, *File → Export MP4* automatically muxes it as an AAC track trimmed to the animation length. Toggle it off with *Voiceover → Voiceover in MP4*.
- **Supported input:** PCM WAV, 8/16/24/32-bit or 32-bit float, mono or stereo (downmixed to mono).
- Recordings are capped at 10 minutes and can be re-recorded at any time.

### 🚀 Direct MP4 & GIF Export (via ffmpeg)
- **One-Click MP4:** Directly encodes high-compatibility H.264 video (`output.mp4`) ready for web, Discord, YouTube, or social media.
- **High-Quality GIF:** Two-pass palette generation (`output.gif`) with zero color banding.
- **Native Project Save/Load:** Lightweight `.fpaint` binary files to preserve your frames and layers.
- Export and save live in the **File** menu (the duplicate top-right shortcut buttons have been removed to keep the canvas clean).

> **Note:** `.fpaint` replaces the old `.anp` format. Projects saved with older versions are not compatible.

---

## ⌨️ Keyboard Shortcuts

| Shortcut | Action |
| :--- | :--- |
| `Space` | Play / Pause Animation |
| `←` / `→` | Previous / Next Frame |
| `Tab` | Switch between Background and Animation Layer |
| `O` | Toggle Onion Skinning |
| `Ctrl + Z` | Undo |
| `Ctrl + Y` | Redo |
| `Ctrl + S` | Save Project (`project.fpaint`) |
| `Ctrl + O` | Load Project (`project.fpaint`) |
| `Ctrl + N` | New Animation |
| `Ctrl + E` | Export MP4 (`output.mp4`) |
| `Ctrl + G` | Export GIF (`output.gif`) |
| `Ctrl + R` | Start / stop voiceover recording |
| `P` | Select **Pencil** Tool |
| `B` | Select **Brush** Tool |
| `E` | Select **Eraser** Tool |
| `G` | Select **Bucket (Fill)** Tool |
| `I` | Select **Eyedropper** Tool |
| `L` | Select **Line** Tool |
| `R` | Select **Rectangle** Tool |
| `C` | Select **Ellipse** Tool |
| `S` | Select **Rectangular Selection** Tool |
| `F` | Select **Free-Form (Lasso) Selection** Tool |
| `Esc` | Deselect active marquee / cancel lasso |
| `Mouse Wheel` | Zoom in / Zoom out |

### Using Free-Form Select
1. Pick the lasso tool (`F`) and drag an outline around the artwork.
2. Release to close the shape — only pixels **inside** the lasso are selected.
3. Click inside the selection and drag to move it. The source area is cut away automatically.
4. Click outside the selection or press `Esc` to deselect. `Ctrl + Z` undoes a move.

### Voiceover workflow
1. Record a take with `Ctrl + R` (or drag a `.wav` onto the window).
2. Press *Play VO* in the timeline to check timing.
3. Use `Ctrl + E` to export — the MP4 now carries the voiceover.
4. If the audio runs longer than the animation it is trimmed; shorter audio simply ends early.
5. Toggle *Voiceover → Voiceover in MP4* to export a silent MP4 without deleting the take.

---

## 🛠️ Building & Running

### Requirements
- Linux (x86_64, ARM) or Windows (x86_64 via MinGW/MSYS2)
- GCC or Clang
- SDL2 (`libsdl2-dev` on Debian/Ubuntu, `mingw-w64-x86_64-SDL2` on MSYS2)
- FFmpeg (required at runtime for video & GIF export)

### Compile (Linux / macOS)
```bash
make
```

### Launch
```bash
./farukpaint
```
or simply:
```bash
make run
```

### Run the test suite
```bash
make test
```

### Compile on Windows (MSYS2 MinGW64)
```bash
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 make zip
VERSION=1.0.0 packaging/build-windows.sh
```
This produces a portable `.zip` and a bare `.exe` in `dist/`. FFmpeg must be installed and available on `PATH` for MP4/GIF export.

### FFmpeg troubleshooting

Faruk Paint reports the specific failure reason in the status bar instead of a generic error.

- **"FFmpeg not found"** — `ffmpeg.exe` is not on `PATH` for this process. Add its `bin` folder to the system/user `PATH`, then **restart Faruk Paint** (an already-running app does not see new `PATH` entries).
- **"FFmpeg failed (exit code N)"** — FFmpeg ran but rejected the encode. The full FFmpeg log is saved next to the output as `output.mp4.ffmpeg.log` / `output.gif.ffmpeg.log`.
- **"Could not open ... for writing"** — the working folder is read-only (for example `Program Files`). Run Faruk Paint from a writable folder such as `Documents`.

### Build a Linux AppImage
```bash
sudo apt install libsdl2-dev make curl
VERSION=1.0.0 packaging/build-appimage.sh
```
The AppImage is written to `dist/FarukPaint-<version>-x86_64.AppImage`.

---

## 📦 Downloads & Releases

Every `v*` git tag triggers the GitHub Actions release workflow, which publishes:

| Platform | Artifact |
| :--- | :--- |
| Linux x86_64 | `FarukPaint-<version>-x86_64.AppImage` |
| Windows x86_64 | `FarukPaint-<version>-windows-x86_64.zip` (includes `SDL2.dll`) |
| Windows x86_64 | `FarukPaint-<version>-windows-x86_64.exe` |

> **Note:** MP4 and GIF export shells out to `ffmpeg`. Install it with your package manager, or add `ffmpeg.exe` to `PATH` on Windows. The core painting/animation features work without it.
>
> **Windows note:** if you add FFmpeg to `PATH` while Faruk Paint is already open, restart the app — running processes keep the environment they were started with. Faruk Paint also probes `C:\ffmpeg\bin`, `C:\Program Files\ffmpeg\bin` and `C:\Program Files (x86)\ffmpeg\bin` automatically. On failure it now names the exact reason, and FFmpeg's own error output is written to `<output>.ffmpeg.log`.

---

## 📁 Project Architecture
```
faruk-paint/
├── Makefile                      # Clean compilation build script
├── README.md                     # Documentation & guide
├── LICENSE                       # GNU GPL v3.0
├── assets/
│   ├── farukpaint.svg             # Vector app icon
│   └── farukpaint.png             # 256x256 icon used by the AppImage
├── packaging/
│   ├── AppRun                    # AppImage launcher
│   ├── farukpaint.desktop         # Linux desktop entry
│   ├── build-appimage.sh         # Linux AppImage builder
│   └── build-windows.sh          # Portable Windows builder (MSYS2)
├── .github/workflows/
│   └── release.yml               # CI: AppImage + Windows EXE GitHub Release
├── src/
│   ├── main.c                    # App lifecycle, event dispatch & playback loop
│   ├── canvas.h/c                # 2-layer canvas data structures, compositing & onion skin
│   ├── tools.h/c                 # 16 classic drawing tools & shape algorithms
│   ├── ui.h/c                    # Retro Windows 95 beveled UI & timeline renderer
│   ├── export.h/c                # FFmpeg pipe exporter for MP4, GIF & .fpaint project format
│   ├── audio.h/c                 # Voiceover recording, WAV import/export & preview
│   └── font.h/c                  # Embedded 8x8 retro bitmap font
└── tests/
    └── test_farukpaint.c               # Automated test suite
```

---

## 📜 License

Faruk Paint is free software: you can redistribute it and/or modify it under the terms of the **GNU General Public License v3.0** as published by the Free Software Foundation.

See [LICENSE](LICENSE) for the full text. This program is distributed in the hope that it will be useful, but **WITHOUT ANY WARRANTY**; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
