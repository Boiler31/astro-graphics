#include "Blackbody.h"

#include <algorithm>
#include <cmath>

namespace blackbody
{
namespace
{
// Piecewise Gaussian with different widths left and right of the peak.
double Lobe(double x, double mu, double sigmaLeft, double sigmaRight)
{
	double t = (x - mu) / (x < mu ? sigmaLeft : sigmaRight);
	return std::exp(-0.5 * t * t);
}

// CIE 1931 2-degree color matching functions, multi-lobe fit (lambda in nm).
double Xbar(double l)
{
	return 1.056 * Lobe(l, 599.8, 37.9, 31.0) + 0.362 * Lobe(l, 442.0, 16.0, 26.7) - 0.065 * Lobe(l, 501.1, 20.4, 26.2);
}
double Ybar(double l)
{
	return 0.821 * Lobe(l, 568.8, 46.9, 40.5) + 0.286 * Lobe(l, 530.9, 16.3, 31.1);
}
double Zbar(double l)
{
	return 1.217 * Lobe(l, 437.0, 11.8, 36.0) + 0.681 * Lobe(l, 459.0, 26.0, 13.8);
}

// Planck spectral radiance up to a constant factor, lambda in nm. The constant cancels when the
// result is normalized, so only the shape matters. hc/k = 1.4388e7 nm*K.
double Planck(double lambdaNm, double kelvin)
{
	const double c2 = 1.438776877e7;
	double x = c2 / (lambdaNm * kelvin);
	// 1/lambda^5 / (e^x - 1); expm1 keeps precision for small x, and x is capped to avoid overflow.
	return 1.0 / (std::pow(lambdaNm, 5.0) * std::expm1(std::min(x, 700.0)));
}
} // namespace

glm::vec3 Color(double kelvin)
{
	double X = 0.0, Y = 0.0, Z = 0.0;
	for (double l = 360.0; l <= 830.0; l += 1.0) {
		double b = Planck(l, kelvin);
		X += b * Xbar(l);
		Y += b * Ybar(l);
		Z += b * Zbar(l);
	}
	// XYZ -> linear sRGB (D65).
	double r = 3.2406 * X - 1.5372 * Y - 0.4986 * Z;
	double g = -0.9689 * X + 1.8758 * Y + 0.0415 * Z;
	double bl = 0.0557 * X - 0.2040 * Y + 1.0570 * Z;
	r = std::max(r, 0.0);
	g = std::max(g, 0.0);
	bl = std::max(bl, 0.0);
	double m = std::max({r, g, bl});
	if (m <= 0.0) {
		return glm::vec3(0.0f);
	}
	return glm::vec3(static_cast<float>(r / m), static_cast<float>(g / m), static_cast<float>(bl / m));
}

std::vector<glm::vec3> BuildLut(int size, double tMin, double tMax)
{
	std::vector<glm::vec3> lut(size);
	double lo = std::log(tMin), hi = std::log(tMax);
	for (int i = 0; i < size; i++) {
		double t = std::exp(lo + (hi - lo) * i / (size - 1));
		lut[i] = Color(t);
	}
	return lut;
}
} // namespace blackbody
