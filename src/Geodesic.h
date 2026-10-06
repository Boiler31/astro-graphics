#pragma once
#include <glm/glm.hpp>

// CPU reference for the light-bending math used in shaders/geodesic.glsl (same algorithm, but in
// double precision). Used by tests/geodesic_tests.cpp and the GUI's shadow-size readout.
//
// Units: G = c = M = 1, so the horizon is at r = 2, the photon sphere at r = 3 and the
// critical impact parameter is 3*sqrt(3) (see PLAN.md section 1).
//
// A photon in Schwarzschild spacetime moves in a plane, and its path obeys the Binet equation
// u'' + u = 3 M u^2 (u = 1/r). That path is reproduced exactly by this Newtonian-looking ODE in
// "Cartesian" coordinates built from (r, theta, phi):
//
//      d^2 x / d lambda^2 = -3 M h^2 x / |x|^5,      h = |x cross v| (conserved)
namespace geo
{
constexpr double kHorizon = 2.0;
constexpr double kPhotonSphere = 3.0;
constexpr double kCriticalImpact = 5.196152422706632; // 3 * sqrt(3)
constexpr double kCaptureRadius = kHorizon * (1.0 + 1e-3);

glm::dvec3 Accel(const glm::dvec3& x, double h2);

// One RK4 step of the (x, v) system with fixed angular momentum h2 = |x cross v|^2.
void Rk4Step(glm::dvec3& x, glm::dvec3& v, double h2, double dt);

// Converts a unit direction n (the camera ray, as seen by an observer hovering at camPos, expressed
// in the same Cartesian axes) to the coordinate-space velocity used by the integrator. The radial
// part is squashed by sqrt(1 - 2M/r) (the metric's g_rr); the result is normalized.
glm::dvec3 StaticObserverVelocity(const glm::dvec3& camPos, const glm::dvec3& n);

struct TraceResult
{
	bool captured = false;   // fell below the horizon (or ran out of steps)
	bool outOfSteps = false; // step budget exhausted (counted as captured)
	glm::dvec3 dir{0.0};     // asymptotic direction if escaped (unit)
	int steps = 0;
	double minR = 0.0;       // closest approach reached
};

// Traces a camera ray backwards from a static observer until it is captured or escapes beyond
// rFar. Step size is stepScale * (r - 2M), matching the shader.
TraceResult TraceFromStaticObserver(const glm::dvec3& camPos, const glm::dvec3& n, int maxSteps,
                                    double stepScale, double rFar = 5000.0);

// Angular radius (radians) of the black hole's shadow for a static observer at radius r:
// sin(alpha) = (3 sqrt(3) M / r) sqrt(1 - 2M/r).
double ShadowAngularRadius(double r);
} // namespace geo
