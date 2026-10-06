#pragma once
#include <glm/glm.hpp>

// Camera observers. The renderer's base observer is a *static* one hovering at the camera position
// (see staticObserverVelocity in shaders/geodesic.glsl). A camera that is moving relative to that
// static observer sees the sky aberrated (everything bunches up toward the direction of motion) and
// Doppler shifted. Velocities are in units of c, measured by the static observer at the camera's
// position and expressed along the world axes (the local orthonormal frame).
enum class ObserverModel
{
	Static = 0,        // hovering (needs a rocket in real life)
	FreeFall = 1,      // falling radially from rest at infinity: v = sqrt(2M/r) inward
	CircularOrbit = 2, // circular geodesic orbit in the disk's sense of rotation: v = sqrt(M/(r-2M))
	AlongView = 3      // moving along the viewing direction (a demo of aberration)
};

const char* ObserverModelName(ObserverModel m);

// Velocity of the camera relative to the static observer at `pos`, scaled by `boost` in [0, 1]
// (boost = 0 is always static, 1 is the full model speed). `forward` is the viewing direction and
// `viewSpeed` the speed used by AlongView. Magnitude is capped below 1.
glm::vec3 ObserverVelocity(ObserverModel model, const glm::vec3& pos, const glm::vec3& forward, float boost,
                           float viewSpeed);

// Rate of the camera's clock relative to a distant static clock: sqrt(1 - 2M/r) / gamma.
double ClockRate(double r, double beta);

// A viewing direction n in the moving observer's frame -> the viewing direction in the static
// observer's frame (relativistic aberration; the camera ray is traced backwards, so this is the
// direction the *incoming* photon came from). `doppler` receives nu_observed / nu_static for light
// arriving along that ray: > 1 (blueshift) when looking forward.
// This is the same math as aberrate() in shaders/blackhole.frag.
glm::dvec3 AberrateToStaticFrame(const glm::dvec3& n, const glm::dvec3& beta, double* doppler = nullptr);

// The inverse of AberrateToStaticFrame: the direction (in the moving observer's frame) in which an
// object that lies along `nStatic` in the static observer's frame appears. Used to keep the black
// hole centered for a fast camera: it appears shifted toward the direction of motion, so the
// camera has to look that much further forward to center it.
glm::dvec3 AberrateToObserverFrame(const glm::dvec3& nStatic, const glm::dvec3& beta);
