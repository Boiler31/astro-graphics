# astro-graphics

Real-time OpenGL black hole fly-through (Schwarzschild lensing + accretion disk) for an
intro astronomy class. See [PLAN.md](PLAN.md) for the design and milestones.

**Status:** M0-M2 done (window + shader hot-reload, free-fly camera, procedural/texture sky,
Schwarzschild lensing with a validated light-bending integrator). Next: M3 (accretion disk geometry).

## Build (Windows, Visual Studio 2022)

Needs the same environment variables as the CSCE 441 projects: `GLFW_DIR`, `GLEW_DIR`,
`GLM_INCLUDE_DIR`. `cmake` isn't on PATH by default; use the copy bundled with Visual Studio:

```powershell
$cm = "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
& $cm -S . -B build -G "Visual Studio 17 2022" -A x64
& $cm --build build --config Release
.\build\Release\BlackHole.exe
```

Or open `build\BlackHole.sln` in Visual Studio (startup project is `BlackHole`). Source files are
globbed, so re-run the configure step after adding a new `.cpp`.

Command-line flags (mostly for testing): `--frames N` exits after N frames and prints the average
frame time; `--screenshot out.png` renders one frame (no GUI) and exits; `--cam X Y Z YAW PITCH`
sets the start pose (degrees; the black hole is at the origin, distances are in units of M);
`--nogr` starts with straight rays; `--novsync` and `--size W H` for benchmarking.

## Physics validation

`GeodesicTests` runs the CPU reference of the light-bending math (`src/Geodesic.cpp`, same algorithm
as `shaders/geodesic.glsl`) against known GR results: weak-field deflection 4M/b (Einstein), the
critical impact parameter 3*sqrt(3) M, the photon sphere at 3M, angular-momentum conservation, and
the shadow size sin(a) = 3*sqrt(3) M/r * sqrt(1-2M/r) at several camera distances.

```powershell
& $cm --build build --config Release --target GeodesicTests
.\build\Release\GeodesicTests.exe
```

## Controls

| Input | Action |
|---|---|
| W A S D | Move forward / left / back / right |
| Q / E | Move down / up |
| Shift | 4x faster (speed also scales with distance from the origin) |
| Right mouse drag | Look around |
| R | Reload shaders from `shaders/` (a failed compile keeps the last working shader and shows the error in the overlay) |
| Esc | Quit |

Shaders are read directly from the source tree, so edit a `.glsl`/`.frag` file, save, press R.
Shader files may `#include "other.glsl"` (relative to `shaders/`).

## Sky

By default a procedural star field + Milky Way band is used. To use a real star map, put an
equirectangular image at `resources/sky/sky.jpg` (see that folder's README) and choose "Texture"
in the Sky panel.

## Third-party

- [Dear ImGui](https://github.com/ocornut/imgui) v1.91.9 (MIT), vendored in `third_party/imgui`
- [stb](https://github.com/nothings/stb) (public domain / MIT), vendored in `third_party/stb`
- GLFW, GLEW, GLM from the CS441 install locations above
