# astro-graphics

Real-time OpenGL black hole fly-through (Schwarzschild lensing + accretion disk) for an
intro astronomy class. See [PLAN.md](PLAN.md) for the design and milestones.

**Status:** M0-M8 done, i.e. everything in PLAN.md except the optional M9 stretch goals. That is:
shader hot-reload, a free-fly camera with roll, a procedural/texture sky, Schwarzschild lensing with a
validated integrator, a physically shaded accretion disk (temperature profile, Doppler and gravitational
redshift, beaming, blackbody colors, sheared turbulence animated by Keplerian rotation), an HDR pipeline
(bloom, ACES tonemapping, supersampling), keyframed camera paths, moving observers with aberration, a HUD,
and offline recording to mp4.

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
`--nogr` starts with straight rays; `--novsync` and `--size W H` for benchmarking; `--scale S`
(internal render scale, 2 = 2x supersampling), `--exposure`, `--bloom`, `--skygain`, `--time T`
(pin the simulation clock), `--fx gtdcb` (which disk effects are on), `--tpeak`, `--bright`.

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
| Z / C / X | Roll left / right / level the horizon (keyframed too) |
| Shift | 4x faster (speed also scales with distance from the origin) |
| Right mouse drag | Look around |
| Space | Play / pause the camera path |
| K | Add a keyframe from the current view |
| H | Toggle the HUD (distance, clock rate, speed) |
| F1 | Hide / show the control panel |
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

## Camera paths and observers

`resources/paths/tour.txt` is a ~44 s hyperbolic fly-by: a fast approach from 220M, a close pass at 7.5M at about 0.4c, and a flight back out, with the hole kept centered the whole way (see "Aim at hole" below). The
original storyboard is in PLAN.md; `--tour` loads the built-in version, `--path FILE` loads a saved one. Press K to add keyframes
from the current view, edit/scrub them in the *Camera path* panel, and Save/Load text files. A keyframe's "Aim at hole" setting makes the camera look at the hole and, for a fast camera, compensates for aberration so the hole stays centered. Positions
are in units of M (the hole is at the origin). The camera eases in and out at the ends.

The *Observer* panel chooses how the camera moves relative to a hovering observer: free fall (v = sqrt(2M/r)),
a circular orbit, or along the view direction. Motion gives relativistic aberration (the sky bunches up
ahead) and a Doppler shift of everything seen. The *boost* (keyframed) scales that motion from 0 to full speed.
`--observer N --boost B --pathtime T --hud --panel --savepath FILE` are handy for scripted renders.

## Recording the video

Offline rendering uses a fixed time step per frame, 2x2 supersampling and 800 ray steps by default, so
the result does not depend on how fast your machine is. The mp4 is encoded with Windows' built-in H.264
encoder (GPU-accelerated where available); no ffmpeg needed.

```powershell
# the full 75 s tour at 1080p60 with the HUD (about 2 minutes, ~350 MB; renders\ is git-ignored)
.\build\Release\BlackHole.exe --tour --record renders\blackhole_tour.mp4 --size 1920 1080 --hud

# your own path, 4K, a time range, no HUD
.\build\Release\BlackHole.exe --path resources\paths\mine.txt --record renders\mine.mp4 --size 3840 2160 --range 10 40

# lossless PNG sequence instead (large: several GB), then optionally tools\make_video.ps1
.\build\Release\BlackHole.exe --tour --recordpng renders\frames --size 1920 1080
```

Options: `--fps N` (60), `--bitrate MBPS` (40), `--steps N` (800), `--scale S` (2 = supersampling),
`--fade S` / `--fadein S` (1.5 / 0.5 s), `--range A B`, `--simspeed X` (disk animation speed, M per second
of path time), `--hud`. A 1080p frame takes ~25 ms (supersampled, including encoding).
