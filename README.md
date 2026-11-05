# AnimPaint 🎨🎬

> **No tool bloat. Just the iconic Windows 95/98 MS Paint tools paired with a clean 2-layer animation studio.**

**AnimPaint** is a native, ultra-lightweight 2D animation suite written in C (using SDL2 and ffmpeg). It replaces the confusing clutter of modern digital art apps with the beloved, laser-focused toolset of classic MS Paint, while adding essential animation superpowers: **onion skinning, a frame-reel timeline, 2 dedicated layers (Background & Animation), and one-click MP4 / GIF export**.

---

## ✨ Key Features

### 🖌️ Classic MS Paint Toolset (Zero Tool Bloat)
1. **Free-Form Select (Lasso)**
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

### 🚀 Direct MP4 & GIF Export (via ffmpeg)
- **One-Click MP4:** Directly encodes high-compatibility H.264 video (`output.mp4`) ready for web, Discord, YouTube, or social media.
- **High-Quality GIF:** Two-pass palette generation (`output.gif`) with zero color banding.
- **Native Project Save/Load:** Lightweight `.anp` binary files to preserve your frames and layers.

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
| `Ctrl + S` | Save Project (`project.anp`) |
| `Ctrl + O` | Load Project (`project.anp`) |
| `Ctrl + E` | Export MP4 (`output.mp4`) |
| `Ctrl + G` | Export GIF (`output.gif`) |
| `P` | Select **Pencil** Tool |
| `B` | Select **Brush** Tool |
| `E` | Select **Eraser** Tool |
| `G` | Select **Bucket (Fill)** Tool |
| `I` | Select **Eyedropper** Tool |
| `L` | Select **Line** Tool |
| `R` | Select **Rectangle** Tool |
| `C` | Select **Ellipse** Tool |
| `S` | Select **Selection** Tool |
| `Esc` | Deselect active marquee |
| `Mouse Wheel` | Zoom in / Zoom out |

---

## 🛠️ Building & Running

### Requirements
- Linux (x86_64, ARM)
- GCC or Clang
- SDL2 (`libsdl2-dev`)
- FFmpeg (for video & GIF export)

### Compile
```bash
make
```

### Launch
```bash
./animpaint
```
or simply:
```bash
make run
```

---

## 📁 Project Architecture
```
anim-paint/
├── Makefile          # Clean compilation build script
├── README.md         # Documentation & guide
├── src/
│   ├── main.c        # App lifecycle, event dispatch & playback loop
│   ├── canvas.h/c    # 2-layer canvas data structures, compositing & onion skin
│   ├── tools.h/c     # 16 classic drawing tools & shape algorithms
│   ├── ui.h/c        # Retro Windows 95 beveled UI & timeline renderer
│   ├── export.h/c    # FFmpeg pipe exporter for MP4, GIF & .anp project format
│   └── font.h/c      # Embedded 8x8 retro bitmap font
└── tests/
    └── test_anim.c   # Automated test suite
```
