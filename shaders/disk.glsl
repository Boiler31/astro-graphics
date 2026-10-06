// Thin accretion disk in the y = 0 plane, from the ISCO out to uDiskOuter. Included by
// blackhole.frag after geodesic.glsl.
//
// M3: debug checkerboard in (r, phi) so the lensing geometry is easy to read. M4 replaces
// diskShade() with the physical emission (temperature profile, Doppler/gravitational redshift).

const float R_ISCO = 6.0; // innermost stable circular orbit, 6M: the disk's inner edge
const float TWO_PI = 6.28318530718;

uniform float uDiskOuter;
uniform float uDiskOpacity;

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

// Emission (rgb) and opacity (a) of the disk at orbital radius r and azimuth phi.
vec4 diskShade(float r, float phi)
{
	float ring = floor((r - R_ISCO) / 2.0);
	float sector = floor(phi / TWO_PI * 24.0);
	float parity = mod(ring + sector, 2.0);
	vec3 col = (parity < 0.5) ? vec3(1.00, 0.55, 0.15) : vec3(0.90, 0.92, 1.00);
	return vec4(col, uDiskOpacity);
}
