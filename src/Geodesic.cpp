#include "Geodesic.h"

#include <algorithm>
#include <cmath>

namespace geo
{
glm::dvec3 Accel(const glm::dvec3& x, double h2)
{
	double r2 = glm::dot(x, x);
	return -3.0 * h2 * x / (r2 * r2 * std::sqrt(r2));
}

void Rk4Step(glm::dvec3& x, glm::dvec3& v, double h2, double dt)
{
	glm::dvec3 k1x = v, k1v = Accel(x, h2);
	glm::dvec3 k2x = v + 0.5 * dt * k1v, k2v = Accel(x + 0.5 * dt * k1x, h2);
	glm::dvec3 k3x = v + 0.5 * dt * k2v, k3v = Accel(x + 0.5 * dt * k2x, h2);
	glm::dvec3 k4x = v + dt * k3v, k4v = Accel(x + dt * k3x, h2);
	x += dt / 6.0 * (k1x + 2.0 * k2x + 2.0 * k3x + k4x);
	v += dt / 6.0 * (k1v + 2.0 * k2v + 2.0 * k3v + k4v);
}

glm::dvec3 StaticObserverVelocity(const glm::dvec3& camPos, const glm::dvec3& n)
{
	double r = glm::length(camPos);
	glm::dvec3 rhat = camPos / r;
	double f = std::max(1.0 - kHorizon / r, 0.0);
	double nr = glm::dot(n, rhat);
	glm::dvec3 nt = n - nr * rhat;
	return glm::normalize(std::sqrt(f) * nr * rhat + nt);
}

TraceResult TraceFromStaticObserver(const glm::dvec3& camPos, const glm::dvec3& n, int maxSteps,
                                    double stepScale, double rFar)
{
	TraceResult res;
	glm::dvec3 x = camPos;
	glm::dvec3 v = StaticObserverVelocity(camPos, n);
	glm::dvec3 h = glm::cross(x, v);
	double h2 = glm::dot(h, h);
	res.minR = glm::length(x);

	bool done = false;
	for (int i = 0; i < maxSteps && !done; i++) {
		double r = glm::length(x);
		res.minR = std::min(res.minR, r);
		if (r < kCaptureRadius) {
			res.captured = true;
			done = true;
		} else if (r > rFar && glm::dot(x, v) > 0.0) {
			done = true;
		} else {
			Rk4Step(x, v, h2, stepScale * (r - kHorizon));
			res.steps++;
		}
	}
	if (!done) {
		res.captured = true;
		res.outOfSteps = true;
	}
	res.dir = glm::normalize(v);
	return res;
}

double ShadowAngularRadius(double r)
{
	double f = 1.0 - kHorizon / r;
	return std::asin(std::min(1.0, kCriticalImpact / r * std::sqrt(f)));
}
} // namespace geo
