#include "Observer.h"

#include <algorithm>
#include <cmath>

const char* ObserverModelName(ObserverModel m)
{
	switch (m) {
	case ObserverModel::Static: return "Static (hovering)";
	case ObserverModel::FreeFall: return "Free fall (radial)";
	case ObserverModel::CircularOrbit: return "Circular orbit";
	case ObserverModel::AlongView: return "Along view direction";
	}
	return "?";
}

glm::vec3 ObserverVelocity(ObserverModel model, const glm::vec3& pos, const glm::vec3& forward, float boost,
                           float viewSpeed)
{
	const float r = glm::length(pos);
	if (r < 1e-6f || boost <= 0.f) {
		return glm::vec3(0.f);
	}
	const glm::vec3 rhat = pos / r;
	glm::vec3 v(0.f);
	switch (model) {
	case ObserverModel::Static:
		break;
	case ObserverModel::FreeFall:
		v = -std::sqrt(2.f / r) * rhat;
		break;
	case ObserverModel::CircularOrbit: {
		// Same sense as the disk's gas (rotation about +y): velocity along y cross x.
		glm::vec3 t = glm::cross(glm::vec3(0.f, 1.f, 0.f), pos);
		float tl = glm::length(t);
		if (tl > 1e-6f) {
			v = (t / tl) * std::sqrt(1.f / std::max(r - 2.f, 1e-3f));
		}
		break;
	}
	case ObserverModel::AlongView:
		v = forward * viewSpeed;
		break;
	}
	v *= std::min(boost, 1.f);
	float s = glm::length(v);
	if (s > 0.99f) {
		v *= 0.99f / s;
	}
	return v;
}

double ClockRate(double r, double beta)
{
	double f = std::max(1.0 - 2.0 / r, 0.0);
	return std::sqrt(f) * std::sqrt(std::max(1.0 - beta * beta, 0.0));
}

glm::dvec3 AberrateToStaticFrame(const glm::dvec3& n, const glm::dvec3& beta, double* doppler)
{
	double b2 = glm::dot(beta, beta);
	if (b2 < 1e-14) {
		if (doppler) *doppler = 1.0;
		return n;
	}
	double gamma = 1.0 / std::sqrt(1.0 - b2);
	double bn = glm::dot(beta, n);
	if (doppler) {
		*doppler = 1.0 / (gamma * (1.0 - bn));
	}
	return glm::normalize(n / gamma - beta + (gamma / (1.0 + gamma)) * bn * beta);
}
