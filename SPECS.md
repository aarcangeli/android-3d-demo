# android-3d-demo — Software Specifications

## Overview

Android application demonstrating real-time 3D rendering via the Android NDK and OpenGL ES 2.0.
The UI layer is built entirely in C++ using a custom retained-mode widget toolkit; no XML layouts or Android Views are used for the 3D content.

---

## Platform & Build

| Property | Value |
|---|---|
| Min SDK | 21 (Android 5.0) |
| Target SDK | 35 |
| ABI | arm64-v8a |
| C++ standard | C++20 |
| NDK | r26 |
| Graphics API | OpenGL ES 2.0 |
| Font rendering | FreeType 2.13 |
| Build system | CMake 3.22+ via Android Gradle Plugin |

---

## Scene

### Objects

Three objects are placed in the world at startup:

| Object | World Position | Shape | Ray-cast method |
|---|---|---|---|
| Cube | (−2.5, 0, 0) | AABB ±0.8 | Slab test |
| Sphere | (0, 0, 0) | Radius 0.8 | Analytic ray-sphere |
| Cylinder | (2.5, 0, 0) | Radius 0.6, half-height 0.8 | Analytic ray-cylinder (side + caps) |

All objects share a global Y-axis auto-rotation (`autoRotation`) applied around each object's own center.
The scene origin has a ground-plane grid (±5 units) and world-space X/Y/Z axis indicators.

### Lighting

Single directional light hardcoded in the vertex shader at direction `(0.8, 1.5, 0.6)` (normalised).
Diffuse contribution clamped to a minimum of 0.25 (ambient floor).
Normal transformed via the upper-left 3×3 of the model matrix.

---

## Camera

Spherical orbit camera with the following parameters:

| Parameter | Default | Range |
|---|---|---|
| Azimuth | 45° | unbounded |
| Elevation | 45° | −89° … +89° |
| Distance | 5 units | 0.05 … 50 units |
| FoV Y | 45° | fixed |

World up vector is always (0, 1, 0); the camera right vector has no Y component, guaranteeing the horizon stays level during orbit.

Near clip plane = `distance × 0.005` (minimum 0.001), far clip = 500 units.
This keeps the depth buffer precise at very close distances.

---

## Touch Gestures

| Gesture | Action |
|---|---|
| Single-finger drag | Orbit camera (azimuth / elevation) |
| Two-finger pinch | Zoom (change distance) |
| Two-finger drag | Pan (translate orbit target) |
| Single tap | Select / deselect object |
| Double tap | Relocate orbit pivot to hit point (animated) |
| Double-tap + vertical drag | Zoom (distance) |

### Orbit

Dragging right decreases azimuth; dragging up increases elevation.
Sensitivity: 0.25°/pixel.

### Pan

Pan speed is calibrated to `2 × distance × tan(fovY/2) / viewportHeight` so one pixel always corresponds to the same fraction of the visible frustum height.

### Double-tap pivot animation

When the user double-taps a surface, the orbit pivot smoothly transitions from the old target to the hit point over 350 ms using a smooth-step (cubic Hermite) easing.
A yellow crosshair marker is displayed at the current pivot during and after the animation.

### Double-tap + drag zoom

If the second tap is followed immediately by a vertical drag (without lifting), the gesture zooms instead of placing the pivot.
Dragging down zooms in; dragging up zooms out.

---

## Object Selection

- **Single tap** on an object selects it (deselects all others). Tapping empty space deselects.
- **Selected state** is highlighted with a cyan wireframe outline:
  - Cube: 12 box edges (slightly expanded to avoid z-fighting)
  - Sphere: 4 latitude rings + 8 longitude arcs
  - Cylinder: top ring + bottom ring + 8 vertical lines
- The wireframe is rendered with `GL_LEQUAL` depth test and geometry expanded ~3% outside the mesh surface.

---

## Ray Casting

Rays are constructed from touch coordinates via:
1. Convert screen pixel → NDC
2. Scale by `tan(fovY/2)` and aspect ratio to get camera-space offset
3. Combine camera right, up, forward vectors (extracted from view matrix rows) to produce a world-space ray

Each object is tested in its local space:
- Ray is translated by `−object.position`, then inverse-rotated by `−autoRotation` around Y, then inverse-scaled.
- The resulting parameter `t` is the same in world space (rotation and uniform scale preserve ray parameterisation).

The closest positive-`t` hit among all objects wins.

---

## Auto-Rotation

A global `autoRotation` angle (degrees) is incremented each frame by `autoRotSpeed`.
Default: paused (`autoRotSpeed = 0`).
The Pause/Resume button in the Controls panel toggles the speed between 0 and 0.5°/frame.
Speed can be halved (−) or doubled (+) via the speed controls.

---

## UI

Built with a custom C++ retained-mode widget toolkit rendered entirely via OpenGL ES 2.0 (no Android Views).

### Layout

| Mode | Layout |
|---|---|
| Wide (≥ 480 dp) | Horizontal: scrollable info panel | 3D viewport | controls panel |
| Compact (< 480 dp) | Vertical: 3D viewport | tab container (Info / Controls) |

### Widgets available

`Container`, `LinearLayout`, `ScrollContainer`, `TabContainer`, `Label`, `Button`, `GLWidget`

### Controls panel

- **Reset View** — restores default camera (azimuth 45°, elevation 45°, distance 5, pivot at origin), clears selection and hit marker
- **Pause / Resume** — toggles auto-rotation
- **Speed − / +** — halves / doubles rotation speed (clamped 0.1 … 8°/frame)
- **Screenshot** (placeholder)

### FPS display

Computed on the Kotlin/JNI side (1-second interval), displayed as a text overlay in the top-left corner of the 3D viewport.

---

## Renderer Architecture

```
MainActivity (Kotlin)
  └── GLSurfaceView + GLRenderer (Kotlin)
        └── JNI bridge → openglndkdemo.so (C++)
              ├── UISystem (widget tree, layout, input routing)
              │     └── GLWidget (owns SceneRenderer callback)
              └── SceneRenderer
                    ├── Camera (spherical orbit, view/proj matrices)
                    ├── SceneObject[] (type, position, scale, selected)
                    ├── Mesh VBOs (cube, sphere, cylinder, grid, axes)
                    ├── Shaders (lit-object, flat-color/grid)
                    └── Input handler (gestures, ray cast, tap detection)
```

### Shader programs

| Program | Vertex inputs | Uniforms | Use |
|---|---|---|---|
| `objProg` | `aPos`, `aNorm`, `aCol` | `uMVP`, `uModel` | All 3D objects (lit) |
| `gridProg` | `aPos` | `uMVP`, `uColor` | Grid, axes, wireframes, hit marker |

---

## Future Work

- Object list in the UI (per-object visibility, selection, transform editing)
- Additional object types
- Material / color picker per object
- Screenshot export
