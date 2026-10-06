# Black Hole Fly-Through — Project Plan

A real-time OpenGL renderer for a Schwarzschild (non-spinning) black hole with a thin
accretion disk. It bends light by integrating photon paths (null geodesics) for every pixel.
The final deliverable is a 60–90 s video of a camera flying in from deep space, past the disk,
and down toward the photon sphere. It shows gravitational lensing, the black hole's shadow,
the lensed "halo" of the disk's far side, Doppler beaming, and gravitational redshift.

The goal is **physically motivated, not a research code**. The equations below are the real
ones. Artistic liberties (disk temperature, exposure, bloom) are called out explicitly so they
can be explained in the class presentation.

---

## 1. Core idea: ray tracing in curved spacetime

The HW6 ray tracer shot straight rays from the camera. Here each ray is **marched step by step
along a curved path**, using the GPU fragment shader (one invocation per pixel):

```
for each pixel:
    ray = camera ray (position x, direction v)        // traced *backward* in time
    repeat N steps:
        advance x, v by the geodesic equation (RK4)
        if ray crossed the disk plane  -> accumulate disk emission
        if r < horizon                  -> pixel is black (fell in)
        if r > far radius               -> sample star-field by direction v, stop
```

Rendering is a single full-screen quad. All the physics lives in `blackhole.frag`. No
meshes are needed.

### 1.1 Units

Geometric units: **G = c = M = 1**. All distances are in multiples of M (= GM/c²).

| Feature | Radius |
|---|---|
| Event horizon (Schwarzschild radius) | r_s = 2M |
| Photon sphere (light orbits unstably) | 3M |
| ISCO (innermost stable circular orbit) = disk inner edge | 6M |
| Critical impact parameter (shadow edge) | b_c = 3√3 M ≈ 5.196M |

To quote physical sizes in the HUD/presentation, multiply by GM/c². That is ≈ 1.48 km per
solar mass, so ≈ 6.3×10⁹ m for Sgr A* (4.3×10⁶ M☉).

### 1.2 The geodesic equation (the trick that makes this cheap)

In Schwarzschild spacetime a photon's orbit lies in a plane, and its shape obeys the Binet
equation `u'' + u = 3M u²` (u = 1/r). The orbit can be reproduced exactly with a Newtonian-looking
ODE in Cartesian coordinates:

```
d²x/dλ² = -3 M h² x / |x|⁵        where  h = |x × v|  (conserved, compute once per ray)
```

Here x is the Schwarzschild coordinate position (r, θ, φ) drawn as Cartesian. This form needs no
Christoffel symbols and no polar-axis singularities. It costs about one `pow` per evaluation and
integrates well with RK4. (This approach is popularized by Riccardo Antonelli's "Starless"
renderer, and it is worth citing.)

**Integrator:** RK4 with an adaptive step `dλ = clamp(k · (r - 2M) / r · r, dmin, dmax)`.
Steps are small near the horizon and large far away. Expect 200–600 steps per ray. A step
count slider in the GUI trades quality against FPS.

**Termination:**
- `r < 2M·(1+ε)` → captured (black).
- `r > R_far` (≈ 500–1000M) and moving outward → escaped, look up the sky by `normalize(v)`.
- Step budget exhausted → treat as captured (it happens only for rays skimming the photon sphere).

### 1.3 Camera rays near the hole (static observer)

The camera is an observer hovering at fixed r (a "static observer"). Its local orthonormal
direction `n` (from the usual pinhole camera basis) must be converted into the coordinate
velocity used above. Split `n` into radial and tangential parts:

```
v = sqrt(1 - 2M/r) · n_radial · r̂  +  n_tangential
```

The radial squash comes from the metric's g_rr. Without it the shadow is the wrong size
when the camera is close. Overall scale of v doesn't matter: the path shape is scale-invariant.

**Validation:** a static observer at radius r sees a shadow of angular radius α where
`sin α = (3√3 M / r) · sqrt(1 − 2M/r)`. Measure it on screen and compare (see §5).

### 1.4 Accretion disk

**Geometry.** A thin disk in the y = 0 plane (OpenGL y-up), with r from r_in = 6M (ISCO) to
r_out ≈ 20–30M. Each RK4 step checks for a sign change of y. On a crossing, linearly
interpolate the hit point and test r_in ≤ r ≤ r_out. Keep marching after a hit instead of
stopping: a ray can cross the disk 2–3 times. Those later crossings create the "top hat" halo
image of the far side of the disk and the thin secondary rings. The disk is semi-transparent:
accumulate `color += transmittance · emission; transmittance *= (1 − opacity)`.

**Temperature profile** (thin disk, Novikov–Thorne/Page–Thorne in its simplest form):

```
F(r) ∝ (1 / r³) · (1 − sqrt(r_in / r))         T(r) = T_peak_scale · F(r)^(1/4)
```

The temperature is zero at the ISCO, peaks near r ≈ 8M, and falls off as r^(-3/4).

> **Artistic liberty #1:** real stellar-mass black hole disks are ~10⁷ K (X-rays, invisible to
> the eye). We expose `T_peak` as a slider and default to ~6,000–10,000 K so it glows
> orange-white. *Interstellar* made the same choice.

**Gas motion & redshift.** Gas moves on Keplerian circular orbits:
`Ω = sqrt(M / r³)`, `u^t = 1 / sqrt(1 − 3M/r)`.
The observed/emitted frequency ratio for a static camera at r_cam is

```
g = ν_obs / ν_emit = [ 1 / sqrt(1 − 2M/r_cam) ] / [ u^t · (1 − Ω · b_y) ]
```

Here `b_y = L_y / E` is the photon's conserved angular momentum about the disk axis per unit
energy. Compute it once per pixel from the camera ray. The photon travels *toward* the camera,
so use `−n`:

```
b_y = ( x_cam × (−n) )_y / sqrt(1 − 2M/r_cam)
```

This one formula contains **Doppler shift** (Ω·b_y term: approaching side blue, receding side
red), **transverse Doppler / time dilation** (u^t), and **gravitational redshift**.

**Applying g:**
- Observed color temperature: `T_obs = g · T(r)` → look up the blackbody RGB.
- Observed brightness: `I_obs = g⁴ · I_emit` (bolometric beaming). This is why one side of the
  disk is dramatically brighter.

**Blackbody color LUT.** At startup, build a 1D texture on the CPU (T = 1,000–40,000 K, ~1024
texels). Integrate Planck's law against the CIE 1931 color matching functions (use the analytic
fit by Wyman, Sloan & Shirley 2013, no data files), convert XYZ → linear sRGB, and normalize
the chromaticity. Brightness is handled separately through T⁴ and g⁴.

**Texture/animation.** Procedural fBm noise in polar disk coordinates (r, φ), with the angle
advected by `φ − Ω(r)·t`. Differential rotation shears the noise into spiral streaks, and the
inner disk visibly spins faster than the outer disk. This is real physics that also looks good.

### 1.5 Background sky

An equirectangular Milky Way / star map, sampled by the final escaped ray direction. Good
free-for-education sources are NASA SVS "Deep Star Maps" and ESA Gaia sky maps (check the
license and credit in the video). A procedural star field (hash-based point stars) is the
fallback so the project never blocks on assets. Lensing of the sky gives the Einstein ring
and the doubled/stretched stars around the shadow.

Optional: blueshift the sky by `1/sqrt(1 − 2M/r_cam)` for a static observer. Stars brighten
and whiten as you hover close, which is a real effect.

### 1.6 Moving camera (aberration)

A static observer near the hole is unphysical-looking for a fly-by. A camera that is
*moving* sees relativistic aberration: the scene bunches up toward the direction of motion.
Apply a Lorentz boost to the ray direction before converting to coordinates (§1.3):

```
n' = ( n + [ (γ−1)(n·β̂) − γβ ] β̂ ) / ( γ (1 − β·n) )
```

Multiply g by the camera's Doppler factor. Implement this after the static version works,
with a "static / orbiting / custom β" observer dropdown in the GUI.

Crossing the horizon itself is **out of scope**, because Schwarzschild coordinates break
down there. The video ends with a plunge toward ~2.5–3M and a cut or fade to black.

---

## 2. Rendering pipeline

```
[blackhole.frag] → RGBA16F HDR target (optionally at 0.5–0.75 res for speed)
      ↓
[bloom] bright-pass → 5–6 level downsample/upsample blur chain (dual-filter / "Kawase")
      ↓
[tonemap.frag] exposure · ACES filmic curve → sRGB → default framebuffer
      ↓
[ImGui] overlay (hidden during recording)
```

> **Artistic liberty #2:** bloom and tonemapping imitate a camera/eye. They are not part of the
> physics, but they are needed because the disk spans many orders of magnitude in brightness.

---

## 3. Project structure

Reuse the CSCE 441 CMake template (same env vars: `GLFW_DIR`, `GLEW_DIR`, `GLM_INCLUDE_DIR`,
which are already set on this machine) and the `Program` shader wrapper class from HW2/HW4.

```
astro-graphics/
├── CMakeLists.txt            # from CS441 HW6 template + third_party + shaders copy step
├── PLAN.md
├── README.md                 # build/run/controls, credits for sky map + references
├── src/
│   ├── main.cpp              # GLFW window, main loop, input routing
│   ├── App.{h,cpp}           # owns renderer, camera, GUI state, modes (interactive/record)
│   ├── Program.{h,cpp}       # from CS441; + simple `#include` preprocessing + hot reload
│   ├── Camera.{h,cpp}        # free-fly camera (WASD + mouse), orthonormal basis, FOV
│   ├── CameraPath.{h,cpp}    # Catmull-Rom keyframes (pos, look-at, FOV, β), save/load JSON-ish text
│   ├── Framebuffer.{h,cpp}   # HDR FBO helper
│   ├── Bloom.{h,cpp}         # mip chain + passes
│   ├── Blackbody.{h,cpp}     # Planck × CIE → 1D RGB texture
│   ├── Geodesic.{h,cpp}      # CPU reference integrator (same math as shader) for tests
│   ├── Recorder.{h,cpp}      # glReadPixels → PNG sequence (stb_image_write)
│   └── Gui.{h,cpp}           # Dear ImGui panels
├── shaders/
│   ├── fullscreen.vert
│   ├── geodesic.glsl         # shared: accel(), rk4Step(), constants
│   ├── disk.glsl             # temperature, redshift g, noise texture
│   ├── blackhole.frag        # main per-pixel tracer
│   ├── bloom_down.frag, bloom_up.frag
│   └── tonemap.frag
├── resources/
│   └── sky/                  # equirectangular star map (not committed if large; see README)
├── third_party/
│   ├── imgui/                # Dear ImGui + glfw/opengl3 backends
│   └── stb/                  # stb_image.h, stb_image_write.h
├── tests/
│   └── geodesic_tests.cpp    # separate small executable, no GL needed
└── tools/
    └── make_video.ps1        # ffmpeg PNG sequence → MP4
```

**Target OpenGL 4.1 core** (same as CS441-era code). A fragment shader is enough, so compute
shaders aren't needed. The RTX 3070 has plenty of headroom: expect 60 fps at 1080p with ~300
RK4 steps.

**Note:** `cmake` isn't on the PATH in a plain shell. Use the VS-bundled CMake / CMake GUI
as in CS441, or add CMake to PATH.

---

## 4. Milestones

Each milestone ends with something visible on screen, so there is always a presentable
fallback if time runs short.

### M0 — Skeleton (build + window + quad)
- CMakeLists from the HW6 template; vendor ImGui + stb; shaders copied/referenced from `shaders/`.
- GLFW window, full-screen triangle, a shader that outputs UV colors.
- **Shader hot-reload on `R`**. This gives a fast iteration loop for the rest of the project.
- ✅ Done when: gradient on screen, editing the shader + pressing R updates it.

### M1 — Flat-space ray tracer + sky
- Camera class (free-fly), generate per-pixel ray directions from FOV/basis uniforms.
- Load the equirectangular sky; sample by ray direction. Procedural-star fallback.
- ImGui panel: FPS, camera position, FOV.
- ✅ Done when: you can look around a normal undistorted starry sky.

### M2 — Schwarzschild lensing
- `geodesic.glsl`: accel, RK4, adaptive step, termination rules (§1.2).
- Static-observer ray conversion (§1.3).
- CPU `Geodesic.cpp` with the identical algorithm + `tests/` (§5).
- GUI: step count, step-size factor, toggle "GR on/off" (off = straight rays).
- ✅ Done when: black shadow with the correct angular size, Einstein ring, and stars smeared around the edge.

### M3 — Disk geometry
- Plane-crossing detection + interpolation, multiple crossings, transmittance.
- Flat debug coloring first: checkerboard in (r, φ) makes the lensing geometry obvious and
  is a good slide for the presentation.
- ✅ Done when: the iconic image is visible, i.e. the disk's far side arched over and under the shadow.

### M4 — Disk physics shading
- Blackbody LUT (§1.4), temperature profile, redshift factor g, `T_obs = gT`, `I ∝ g⁴`.
- GUI toggles for **each effect separately**: Doppler, gravitational redshift, beaming, lensing.
  These toggles are the educational core of the presentation ("here's what each piece of
  relativity contributes").
- ✅ Done when: asymmetric disk, bright blue-white approaching side and dim red receding side.

### M5 — Disk detail & animation
- fBm noise advected by Keplerian Ω(r); radial density falloff at inner/outer edges.
- Simulation time slider/speed (time in units of M; display the orbital period at ISCO).

### M6 — HDR polish
- RGBA16F target, bloom chain, ACES tonemap, exposure slider, optional reduced internal resolution.
- Optional: sky blueshift for close static observers (§1.5).

### M7 — Camera choreography & moving observers
- `CameraPath`: keyframes (position, look target, FOV, observer velocity), Catmull-Rom
  interpolation, ease-in/out. Add a keyframe from the current free-fly camera with a key press.
- Aberration for moving observers (§1.6).
- HUD (toggleable): r in M and km, local time-dilation factor `sqrt(1 − 2M/r)` ("1 hour here =
  X hours far away"), and speed.

### M8 — Recording & final render
- Offline mode: fixed timestep per frame, higher step count, 2×2 supersampling. Render
  in tiles if a frame risks the Windows GPU timeout (TDR, ~2 s).
- PNG sequence → `tools/make_video.ps1` (ffmpeg, H.264, 1080p60).
- ✅ Done when: the final MP4 exists.

### M9 — Stretch goals (pick by interest)
- **Kerr (spinning) black hole**: asymmetric D-shaped shadow, frame dragging, and a closer
  ISCO. This needs a different integrator (Hamiltonian in Boyer–Lindquist or Kerr–Schild
  coordinates), so it is a substantial jump.
- Volumetric thick disk / corona (ray-march density instead of a plane).
- Relativistic jets along the spin axis.
- Split-screen "Newtonian vs. GR" comparison.
- Gravitational-wave-free fun: a second object (star) orbiting and being lensed.

---

## 5. Validation (so the "real equations" claim holds up)

Tests in `tests/geodesic_tests.cpp` run the CPU integrator:

1. **Weak-field deflection:** for impact parameter b ≫ M, deflection angle ≈ 4M/b (Einstein's
   1919 eclipse prediction). This is a great astronomy-class connection.
2. **Critical impact parameter:** rays with b slightly below 3√3 M are captured, and rays slightly above escape.
3. **Photon sphere:** a ray launched tangentially at r = 3M stays near r = 3M for ~1 orbit before
   diverging (unstable orbit).
4. **Conservation:** h = |x × v| drifts < 1e-4 relative over a full trace.
5. **On-screen shadow size** vs. `sin α = (3√3 M/r)·sqrt(1−2M/r)` at several camera radii.
   This is checked manually with a GUI readout.

---

## 6. Final video storyboard (~75 s)

> **Update:** the shipped tour (`resources/paths/tour.txt`) is a ~44 s hyperbolic fly-by instead: a fast
> approach, a close pass at 7.5M (~0.4c, hole kept centered, 100 degree FOV), then back out with the hole
> still framed. The storyboard below is the original plan.

| Time | Camera | What the audience sees / narration hook |
|---|---|---|
| 0–10 s | r ≈ 200M, drifting | Star field with a small dark spot; stars near it are doubled and smeared (lensing). |
| 10–25 s | approach to ~50M, slightly above disk plane | Disk appears; one side much brighter (Doppler beaming). |
| 25–40 s | swing to edge-on, ~25M | The far side of the disk is lensed over the top and under the bottom of the shadow. |
| 40–55 s | orbit around at ~15M | Brightness asymmetry flips as the view goes around; the inner disk visibly shears. |
| 55–70 s | descend toward ~4–5M, HUD on | Sky compresses into a bright ring, time-dilation readout climbs. |
| 70–75 s | plunge toward the photon sphere | Shadow fills the view → fade to black. |

Hold for comparison shots: GR-toggle off/on, Doppler off/on (from M4 toggles).

---

## 7. Risks & mitigations

| Risk | Mitigation |
|---|---|
| Too slow at high step counts | Adaptive step; render at reduced resolution and upscale; offline mode for the final video. |
| Windows TDR kills a long offline frame | Tile rendering (scissor rectangles across multiple draws). |
| Numerical blow-up near horizon | Step scales with (r − 2M); hard capture threshold at 2M(1+ε). |
| Shadow size subtly wrong | §1.3 radial factor + §5 test 5 catches it. |
| Sky asset licensing | Procedural star field fallback; credit sources in README and video. |
| Scope creep (Kerr!) | Kerr is explicitly M9; M0–M8 already make a complete, strong project. |

---

## 8. References (for the write-up / citations)

- R. Antonelli, *Starless* — real-time Schwarzschild ray tracer and the Binet/Cartesian trick.
- O. James, E. von Tunzelmann, P. Franklin, K. Thorne, "Gravitational lensing by spinning black
  holes in astrophysics, and in the movie *Interstellar*," Class. Quantum Grav. 32 (2015).
- J.-P. Luminet, "Image of a spherical black hole with thin accretion disk," A&A 75 (1979). This
  was the first "photograph" of a black hole, hand-plotted. It makes a nice historical slide.
- Event Horizon Telescope Collaboration, M87* (2019) and Sgr A* (2022) images, for comparison.
- Page & Thorne (1974), thin-disk flux; Novikov & Thorne (1973).
- Wyman, Sloan, Shirley, "Simple Analytic Approximations to the CIE XYZ Color Matching
  Functions," JCGT (2013).
