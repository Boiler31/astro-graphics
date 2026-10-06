#pragma once
#include <glm/glm.hpp>
#include <vector>

// Blackbody radiator colors, computed on the CPU (no OpenGL, so it is unit-testable).
//
// Planck's law is integrated against the CIE 1931 color matching functions (using the analytic
// multi-lobe fit of Wyman, Sloan & Shirley, "Simple Analytic Approximations to the CIE XYZ Color
// Matching Functions", JCGT 2013, so no data tables are needed), converted XYZ -> linear sRGB.
namespace blackbody
{
// Linear sRGB chromaticity of a blackbody at temperature T (Kelvin), scaled so the largest
// channel is 1. Brightness is applied separately (T^4, redshift factor g^4; see PLAN.md 1.4).
// Out-of-gamut negative channels are clamped to 0.
glm::vec3 Color(double kelvin);

// Colors for `size` temperatures spaced evenly in ln(T) between tMin and tMax. The shader maps
// T back to a texture coordinate with the same ln(T) rule.
std::vector<glm::vec3> BuildLut(int size, double tMin, double tMax);
} // namespace blackbody
