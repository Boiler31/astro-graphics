// Validation of the light-bending math against known GR results (PLAN.md section 5).
// No OpenGL needed. Exit code is the number of failed checks.
#include "Blackbody.h"
#include "Geodesic.h"
#include "Observer.h"

#include <cmath>
#include <cstdio>
#include <initializer_list>

static int g_failures = 0;

static void Check(bool ok, const char* name, const char* fmt, double a, double b)
{
	char detail[160];
	std::snprintf(detail, sizeof(detail), fmt, a, b);
	std::printf("[%s] %s  (%s)\n", ok ? "PASS" : "FAIL", name, detail);
	if (!ok) g_failures++;
}

// Short names for the observer helpers used by TestObservers.
static glm::dvec3 geo_ab(const glm::dvec3& n, const glm::dvec3& beta, double* d)
{
	return AberrateToStaticFrame(n, beta, d);
}
static double geo_clock(double r, double beta)
{
	return ClockRate(r, beta);
}

static double AngleBetween(const glm::dvec3& a, const glm::dvec3& b)
{
	return std::atan2(glm::length(glm::cross(a, b)), glm::dot(a, b));
}

// Static observer at (0, 0, r0) looking inward with a straight-line impact parameter b, in the
// x-z plane. b = r sin(psi) / sqrt(1 - 2M/r) for a static observer.
static glm::dvec3 InwardDir(double r0, double b)
{
	double f = 1.0 - geo::kHorizon / r0;
	double s = b * std::sqrt(f) / r0;
	return glm::dvec3(s, 0.0, -std::sqrt(1.0 - s * s));
}

// 1. Weak-field deflection: alpha = 4M/b + (15 pi / 4)(M/b)^2 + ... (Einstein 1915; the eclipse
//    measurement of 1919 confirmed the 4M/b term).
static void TestWeakFieldDeflection()
{
	const double r0 = 1.0e6;
	for (double b : {50.0, 100.0, 400.0, 2000.0}) {
		glm::dvec3 n = InwardDir(r0, b);
		geo::TraceResult res = geo::TraceFromStaticObserver(glm::dvec3(0, 0, r0), n, 20000, 0.1, 1.0e7);
		glm::dvec3 v0 = geo::StaticObserverVelocity(glm::dvec3(0, 0, r0), n);
		double defl = AngleBetween(v0, res.dir);
		double expected = 4.0 / b + (15.0 * 3.14159265358979 / 4.0) / (b * b);
		double relErr = std::fabs(defl - expected) / expected;
		char name[64];
		std::snprintf(name, sizeof(name), "weak-field deflection, b = %.0f M", b);
		Check(relErr < 0.01, name, "measured %.6f rad, expected %.6f rad", defl, expected);
	}
}

// 2. Critical impact parameter: b slightly below 3 sqrt(3) M is captured, slightly above escapes.
static void TestCriticalImpactParameter()
{
	const double r0 = 1.0e4;
	const double bc = geo::kCriticalImpact;
	for (double eps : {1e-2, 1e-3}) {
		glm::dvec3 cam(0, 0, r0);
		auto below = geo::TraceFromStaticObserver(cam, InwardDir(r0, bc * (1.0 - eps)), 20000, 0.25);
		auto above = geo::TraceFromStaticObserver(cam, InwardDir(r0, bc * (1.0 + eps)), 20000, 0.25);
		char name[64];
		std::snprintf(name, sizeof(name), "critical b: captured below / escapes above (eps=%g)", eps);
		Check(below.captured && !above.captured, name, "below captured=%.0f, above captured=%.0f",
		      below.captured ? 1.0 : 0.0, above.captured ? 1.0 : 0.0);
	}
}

// 3. Photon sphere: a photon launched tangentially at r = 3M stays on the circular (unstable)
//    orbit for at least a full revolution.
static void TestPhotonSphere()
{
	glm::dvec3 x(0, 0, geo::kPhotonSphere);
	glm::dvec3 v = geo::StaticObserverVelocity(x, glm::dvec3(1, 0, 0)); // tangential
	glm::dvec3 h = glm::cross(x, v);
	double h2 = glm::dot(h, h);
	double maxDev = 0.0, swept = 0.0;
	glm::dvec3 prev = x;
	const double dt = 0.01;
	for (int i = 0; i < 200000 && swept < 2.0 * 3.14159265358979; i++) {
		geo::Rk4Step(x, v, h2, dt);
		maxDev = std::fmax(maxDev, std::fabs(glm::length(x) - geo::kPhotonSphere));
		swept += AngleBetween(prev, x);
		prev = x;
	}
	Check(maxDev < 1e-3 && swept >= 2.0 * 3.14159265358979, "photon sphere holds for one revolution",
	      "max |r-3M| = %.2e, swept %.3f rad", maxDev, swept);
}

// 4. Conservation: h = |x cross v| should not drift over a full trace.
static void TestConservation()
{
	for (double k : {0.25, 0.1}) {
		glm::dvec3 x(0, 0, 100.0);
		glm::dvec3 v = geo::StaticObserverVelocity(x, InwardDir(100.0, 8.0));
		double h0 = glm::length(glm::cross(x, v));
		double maxDrift = 0.0;
		for (int i = 0; i < 5000; i++) {
			double r = glm::length(x);
			if (r < geo::kCaptureRadius || (r > 5000.0 && glm::dot(x, v) > 0.0)) break;
			double h = h0 * h0;
			geo::Rk4Step(x, v, h, k * (r - geo::kHorizon));
			maxDrift = std::fmax(maxDrift, std::fabs(glm::length(glm::cross(x, v)) - h0) / h0);
		}
		char name[64];
		std::snprintf(name, sizeof(name), "angular momentum conserved (step scale %.2f)", k);
		Check(maxDrift < 1e-4, name, "max relative drift %.2e (limit %.0e)", maxDrift, 1e-4);
	}
}

// 5. Shadow size: bisect the capture boundary in view angle and compare with
//    sin(alpha) = (3 sqrt(3) M / r) sqrt(1 - 2M/r). This exercises the static-observer ray
//    conversion and the integrator together.
static void TestShadowSize()
{
	for (double r : {6.0, 10.0, 20.0, 50.0, 200.0}) {
		glm::dvec3 cam(0, 0, r);
		double lo = 0.0, hi = 1.5707963; // captured at lo, escapes at hi
		for (int i = 0; i < 40; i++) {
			double mid = 0.5 * (lo + hi);
			glm::dvec3 n(std::sin(mid), 0.0, -std::cos(mid));
			if (geo::TraceFromStaticObserver(cam, n, 20000, 0.25).captured) lo = mid; else hi = mid;
		}
		double measured = 0.5 * (lo + hi);
		double expected = geo::ShadowAngularRadius(r);
		char name[64];
		std::snprintf(name, sizeof(name), "shadow angular radius at r = %.0f M", r);
		Check(std::fabs(measured - expected) / expected < 1e-3, name,
		      "measured %.5f rad, expected %.5f rad", measured, expected);
	}
}

// 6. Blackbody colors follow the familiar sequence: red-orange when cool, near white around
//    6500 K (daylight), blue-white when hot.
static void TestBlackbodyColors()
{
	glm::vec3 cool = blackbody::Color(2000.0);
	Check(cool.r == 1.0f && cool.g < 0.55f && cool.b < 0.15f, "2000 K is deep orange-red", "g = %.2f, b = %.2f", cool.g, cool.b);
	glm::vec3 day = blackbody::Color(6500.0);
	Check(day.r > 0.85f && day.g > 0.85f && day.b > 0.85f, "6500 K is nearly white", "min channel %.2f, max %.2f",
	      std::fmin(day.r, std::fmin(day.g, day.b)), std::fmax(day.r, std::fmax(day.g, day.b)));
	glm::vec3 hot = blackbody::Color(20000.0);
	Check(hot.b == 1.0f && hot.r < 0.8f && hot.r > 0.4f, "20000 K is blue-white", "r = %.2f, g = %.2f", hot.r, hot.g);
	// Blue fraction rises and red fraction falls monotonically with temperature.
	bool monotone = true;
	glm::vec3 prev = blackbody::Color(1000.0);
	for (double t = 1100.0; t <= 40000.0; t *= 1.1) {
		glm::vec3 c = blackbody::Color(t);
		if (c.b / (c.r + 1e-9f) < prev.b / (prev.r + 1e-9f) - 1e-6f) monotone = false;
		prev = c;
	}
	Check(monotone, "blue/red ratio increases with temperature", "%.0f, %.0f", 1.0, 1.0);
}

// 7. Moving observers: relativistic aberration and clock rates.
static void TestObservers()
{
	const glm::dvec3 vhat(0, 0, -1); // direction of motion
	for (double beta : {0.3, 0.8, 0.99}) {
		glm::dvec3 b = beta * vhat;
		double d;
		// Looking straight ahead stays straight ahead, with the blueshift sqrt((1+b)/(1-b)).
		glm::dvec3 fwd = geo_ab(vhat, b, &d);
		double expected = std::sqrt((1.0 + beta) / (1.0 - beta));
		char name[96];
		std::snprintf(name, sizeof(name), "aberration: forward stays forward, Doppler (beta = %.2f)", beta);
		Check(glm::length(fwd - vhat) < 1e-9 && std::fabs(d - expected) < 1e-9 * expected, name,
		      "Doppler %.6f, expected %.6f", d, expected);

		// Looking sideways in the observer's frame: the incoming ray in the static frame has
		// cos(theta) = -beta relative to the direction of motion (light from behind 90 degrees).
		glm::dvec3 side = geo_ab(glm::dvec3(1, 0, 0), b, &d);
		std::snprintf(name, sizeof(name), "aberration: 90 degrees <-> cos = -beta (beta = %.2f)", beta);
		Check(std::fabs(glm::dot(side, vhat) + beta) < 1e-9, name, "cos = %.6f, expected %.6f", glm::dot(side, vhat), -beta);
	}
	// Aberration preserves unit length and is the identity at rest.
	glm::dvec3 n = glm::normalize(glm::dvec3(0.3, -0.5, -0.8));
	glm::dvec3 a = geo_ab(n, glm::dvec3(0.2, 0.1, -0.5), nullptr);
	Check(std::fabs(glm::length(a) - 1.0) < 1e-12, "aberrated direction is a unit vector", "|n| = %.12f (%.0f)", glm::length(a), 1.0);
	Check(glm::length(geo_ab(n, glm::dvec3(0.0), nullptr) - n) < 1e-15, "aberration is the identity at rest", "%.0f %.0f", 0.0, 0.0);

	// Free fall from rest at infinity: v = sqrt(2M/r); orbiting gas: v = sqrt(M/(r-2M)).
	glm::vec3 pos(0, 0, 10.f);
	glm::vec3 ff = ObserverVelocity(ObserverModel::FreeFall, pos, glm::vec3(0, 0, -1), 1.f, 0.8f);
	Check(std::fabs(glm::length(ff) - std::sqrt(2.0f / 10.f)) < 1e-6 && ff.z < 0.f, "free-fall speed sqrt(2M/r), directed inward",
	      "|v| = %.5f, expected %.5f", glm::length(ff), std::sqrt(2.0 / 10.0));
	glm::vec3 orb = ObserverVelocity(ObserverModel::CircularOrbit, pos, glm::vec3(0, 0, -1), 1.f, 0.8f);
	Check(std::fabs(glm::length(orb) - std::sqrt(1.0f / 8.f)) < 1e-6 && std::fabs(glm::dot(orb, glm::normalize(pos))) < 1e-6,
	      "circular-orbit speed sqrt(M/(r-2M)), tangential", "|v| = %.5f, expected %.5f", glm::length(orb), std::sqrt(1.0 / 8.0));
	Check(glm::length(ObserverVelocity(ObserverModel::FreeFall, pos, glm::vec3(0, 0, -1), 0.f, 0.8f)) == 0.f,
	      "boost 0 means static", "%.0f %.0f", 0.0, 0.0);

	// Clock rates: a static clock at r = 3M runs at sqrt(1/3); motion slows it further.
	Check(std::fabs(geo_clock(3.0, 0.0) - std::sqrt(1.0 / 3.0)) < 1e-12, "static clock at 3M runs at sqrt(1/3)",
	      "%.6f vs %.6f", geo_clock(3.0, 0.0), std::sqrt(1.0 / 3.0));
	Check(std::fabs(geo_clock(1e9, 0.6) - 0.8) < 1e-6, "clock at rest far away, moving at 0.6c: 0.8", "%.6f vs %.6f",
	      geo_clock(1e9, 0.6), 0.8);
}

int main()
{
	TestWeakFieldDeflection();
	TestBlackbodyColors();
	TestObservers();
	TestCriticalImpactParameter();
	TestPhotonSphere();
	TestConservation();
	TestShadowSize();
	std::printf("\n%d failure(s)\n", g_failures);
	return g_failures;
}
