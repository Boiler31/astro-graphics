// Light bending around a Schwarzschild (non-spinning) black hole. GLSL port of src/Geodesic.cpp,
// which is validated in tests/geodesic_tests.cpp. Units: G = c = M = 1 (see PLAN.md section 1).
//
// A photon's path obeys the Binet equation u'' + u = 3 M u^2 (u = 1/r). That is reproduced
// exactly by this Newtonian-looking ODE in "Cartesian" coordinates:
//
//     d^2 x / d lambda^2 = -3 M h^2 x / |x|^5,     h = |x cross v|  (conserved)
//
// so no Christoffel symbols or polar-axis singularities are needed.

const float R_HORIZON = 2.0;                         // event horizon, 2M
const float R_PHOTON = 3.0;                          // photon sphere, 3M
const float CAPTURE_RADIUS = R_HORIZON * (1.0 + 1e-3);

vec3 geodesicAccel(vec3 x, float h2)
{
	float r2 = dot(x, x);
	return -3.0 * h2 * x / (r2 * r2 * sqrt(r2));
}

// One RK4 step of the (x, v) system with fixed angular momentum h2 = |x cross v|^2.
void rk4Step(inout vec3 x, inout vec3 v, float h2, float dt)
{
	vec3 k1x = v;
	vec3 k1v = geodesicAccel(x, h2);
	vec3 k2x = v + 0.5 * dt * k1v;
	vec3 k2v = geodesicAccel(x + 0.5 * dt * k1x, h2);
	vec3 k3x = v + 0.5 * dt * k2v;
	vec3 k3v = geodesicAccel(x + 0.5 * dt * k2x, h2);
	vec3 k4x = v + dt * k3v;
	vec3 k4v = geodesicAccel(x + dt * k3x, h2);
	x += dt / 6.0 * (k1x + 2.0 * k2x + 2.0 * k3x + k4x);
	v += dt / 6.0 * (k1v + 2.0 * k2v + 2.0 * k3v + k4v);
}

// Camera ray n (unit, in world axes) seen by an observer hovering at camPos -> coordinate-space
// velocity for the integrator. The radial part is squashed by sqrt(1 - 2M/r) (the metric's g_rr);
// without it the shadow would be the wrong size when the camera is close.
vec3 staticObserverVelocity(vec3 camPos, vec3 n)
{
	float r = length(camPos);
	vec3 rhat = camPos / r;
	float f = max(1.0 - R_HORIZON / r, 0.0);
	float nr = dot(n, rhat);
	vec3 nt = n - nr * rhat;
	return normalize(sqrt(f) * nr * rhat + nt);
}
