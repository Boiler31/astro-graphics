// Thin accretion disk in the y = 0 plane, from the ISCO out to uDiskOuter. Included by
// blackhole.frag after geodesic.glsl.
//
// Physics (PLAN.md section 1.4):
//   * Temperature: thin-disk (Novikov-Thorne / Page-Thorne, simplest form)
//         F(r) ~ r^-3 (1 - sqrt(r_in / r)),   T = T_peak (F / F_peak)^(1/4)
//     zero at the ISCO, peaking near 8M, falling off outward.
//   * Gas moves on circular Keplerian geodesics: Omega = r^-3/2, u^t = 1 / sqrt(1 - 3M/r).
//   * Frequency shift g = nu_obs / nu_emit for a static camera at r_cam:
//         g = 1 / ( sqrt(1 - 2M/r_cam) * u^t * (1 - Omega * b_y) )
//     where b_y = L_y / E is the photon's conserved angular momentum about the disk axis per unit
//     energy. It splits exactly into three factors, each toggleable for the class:
//         gravitational:        sqrt( (1 - 2M/r) / (1 - 2M/r_cam) )
//         transverse Doppler:   sqrt( (r - 3M) / (r - 2M) )        (time dilation of the orbiting gas)
//         Doppler (line of sight): 1 / (1 - Omega * b_y)           (approaching side blue, receding red)
//   * Observed color temperature T_obs = g T; observed brightness scales as g^4 (beaming).
//
// ARTISTIC LIBERTY #1: real stellar-mass black hole disks are ~10^7 K (X-rays, invisible to the
// eye). T_peak is a slider defaulting to a few thousand K so the disk glows orange-white.

const float R_ISCO = 6.0; // innermost stable circular orbit, 6M: the disk's inner edge
const float TWO_PI = 6.28318530718;
const float FLUX_PEAK = 0.056653; // max of u^-3 (1 - u^-1/2), u = r / R_ISCO, reached at u = 49/36 (r ~ 8.2M)

uniform float uDiskOuter;
uniform float uDiskOpacity;
uniform float uDiskTPeak;      // Kelvin at the hottest ring
uniform float uDiskIntensity;  // overall brightness scale
uniform float uDiskRotation;   // +1 or -1: sense of the orbital motion about +y
uniform int uDiskDebug;        // 1 = debug checkerboard instead of physical shading
uniform int uFxGravity;        // 1 = apply each redshift factor
uniform int uFxTimeDilation;
uniform int uFxDoppler;
uniform int uFxColorShift;     // 1 = color from T_obs = g T (else from T)
uniform int uFxBeaming;        // 1 = brightness scaled by g^uBeamExp
uniform float uBeamExp;        // 4 = physical (bolometric beaming); lower = artistic

uniform sampler1D uBlackbody;  // blackbody chromaticity, ln(T)-spaced (src/Blackbody.cpp)
uniform vec2 uLutTRange;       // (Tmin, Tmax) in Kelvin covered by the table

// Cubic Hermite interpolation of a ray segment: positions x0, x1 with velocities v0, v1 over a
// step of length dt; s in [0, 1] is the fractional position along the step. Much more accurate
// than a straight line through the end points when the step is long and the path is curved.
vec3 hermiteSegment(vec3 x0, vec3 v0, vec3 x1, vec3 v1, float dt, float s)
{
	float s2 = s * s, s3 = s2 * s;
	float h00 = 2.0 * s3 - 3.0 * s2 + 1.0;
	float h10 = s3 - 2.0 * s2 + s;
	float h01 = -2.0 * s3 + 3.0 * s2;
	float h11 = s3 - s2;
	return h00 * x0 + h10 * dt * v0 + h01 * x1 + h11 * dt * v1;
}

// Radiated flux relative to its peak value; also equals (T / T_peak)^4.
float diskFlux(float r)
{
	float u = r / R_ISCO;
	return max(pow(u, -3.0) * (1.0 - inversesqrt(u)), 0.0) / FLUX_PEAK;
}

vec3 blackbodyChroma(float kelvin)
{
	float lo = log(uLutTRange.x), hi = log(uLutTRange.y);
	float t = (log(max(kelvin, 1.0)) - lo) / (hi - lo);
	return texture(uBlackbody, clamp(t, 0.0, 1.0)).rgb;
}

// Debug view: checkerboard in (r, phi), makes the lensing geometry easy to read.
vec4 diskDebugShade(float r, float phi)
{
	float ring = floor((r - R_ISCO) / 2.0);
	float sector = floor(phi / TWO_PI * 24.0);
	float parity = mod(ring + sector, 2.0);
	vec3 col = (parity < 0.5) ? vec3(1.00, 0.55, 0.15) : vec3(0.90, 0.92, 1.00);
	return vec4(col, uDiskOpacity);
}

// Emission (rgb, linear HDR) and opacity (a) of the disk at orbital radius r and azimuth phi.
//   by    = b_y for this photon, already divided by sqrt(1 - 2M/r_cam) (see staticObserverVelocity)
//   fCam  = 1 - 2M/r_cam
vec4 diskShade(float r, float phi, float by, float fCam)
{
	if (uDiskDebug == 1) {
		return diskDebugShade(r, phi);
	}

	float flux = diskFlux(r);
	float T = uDiskTPeak * sqrt(sqrt(flux));

	float omega = uDiskRotation * pow(r, -1.5);
	float g = 1.0;
	if (uFxGravity == 1)      g *= sqrt((1.0 - R_HORIZON / r) / fCam);
	if (uFxTimeDilation == 1) g *= sqrt((r - 3.0) / (r - 2.0));
	if (uFxDoppler == 1)      g /= max(1.0 - omega * by, 0.05);

	float Tcolor = (uFxColorShift == 1) ? g * T : T;
	float intensity = flux * ((uFxBeaming == 1) ? pow(g, uBeamExp) : 1.0);

	vec3 rgb = blackbodyChroma(Tcolor) * intensity * uDiskIntensity;

	// Soft outer edge so the disk fades out instead of ending at a hard line.
	float edge = 1.0 - smoothstep(uDiskOuter - 4.0, uDiskOuter, r);
	return vec4(rgb, uDiskOpacity * edge);
}
