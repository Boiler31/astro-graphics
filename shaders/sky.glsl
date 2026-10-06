// Background sky, looked up by ray direction. Included by blackhole.frag.
//
// Two sources:
//   proceduralSky(d, pixAngle): hash-based star field + faint Milky Way band, no assets needed.
//   textureSky(d): equirectangular image (resources/sky/sky.jpg), if present.
//
// NOTE on color: until the HDR/tonemap pass (M6) the output is treated as display-referred,
// so the star brightnesses here are tuned to look right directly on screen.

uniform sampler2D uSky;

// ---------------------------------------------------------------------------------------
// Hash / noise helpers (no sin(), stable across GPUs)
// ---------------------------------------------------------------------------------------
float hash13(vec3 p3)
{
	p3 = fract(p3 * 0.1031);
	p3 += dot(p3, p3.zyx + 31.32);
	return fract((p3.x + p3.y) * p3.z);
}

float valueNoise(vec3 p)
{
	vec3 i = floor(p);
	vec3 f = fract(p);
	f = f * f * (3.0 - 2.0 * f);
	float n000 = hash13(i);
	float n100 = hash13(i + vec3(1, 0, 0));
	float n010 = hash13(i + vec3(0, 1, 0));
	float n110 = hash13(i + vec3(1, 1, 0));
	float n001 = hash13(i + vec3(0, 0, 1));
	float n101 = hash13(i + vec3(1, 0, 1));
	float n011 = hash13(i + vec3(0, 1, 1));
	float n111 = hash13(i + vec3(1, 1, 1));
	return mix(mix(mix(n000, n100, f.x), mix(n010, n110, f.x), f.y),
	           mix(mix(n001, n101, f.x), mix(n011, n111, f.x), f.y), f.z);
}

float fbm(vec3 p)
{
	float a = 0.5, s = 0.0;
	for (int i = 0; i < 5; i++) {
		s += a * valueNoise(p);
		p = p * 2.03 + vec3(7.1, 3.7, 1.3);
		a *= 0.5;
	}
	return s;
}

// ---------------------------------------------------------------------------------------
// Procedural stars
// ---------------------------------------------------------------------------------------
// The sphere is mapped onto the 6 faces of a cube; each face is an N x N grid and each cell
// holds at most one star placed away from the cell border, so a pixel only ever has to look
// at its own cell. Cube cells subtend different solid angles (5x smaller in the corners),
// so the per-cell probability is weighted by that solid angle to keep the sky uniform.
vec3 starLayer(vec3 d, float pixAngle, float N, float density, float gain, float seed)
{
	vec3 a = abs(d);
	vec2 uv;
	float face;
	if (a.x >= a.y && a.x >= a.z) {
		uv = d.yz / a.x;
		face = d.x > 0.0 ? 0.0 : 1.0;
	} else if (a.y >= a.z) {
		uv = d.xz / a.y;
		face = d.y > 0.0 ? 2.0 : 3.0;
	} else {
		uv = d.xy / a.z;
		face = d.z > 0.0 ? 4.0 : 5.0;
	}

	vec2 g = (uv * 0.5 + 0.5) * N;
	vec2 id = min(floor(g), vec2(N - 1.0));
	vec2 f = g - id;
	vec3 key = vec3(id, face) + seed;

	vec2 cc = ((id + 0.5) / N) * 2.0 - 1.0;
	float solidAngleWeight = pow(1.0 + dot(cc, cc), -1.5);
	if (hash13(key) > density * solidAngleWeight) {
		return vec3(0.0);
	}

	vec2 sp = mix(vec2(0.25), vec2(0.75), vec2(hash13(key + 1.3), hash13(key + 2.7)));
	float mag = hash13(key + 4.1);
	float temp = hash13(key + 5.9);

	// Pixel footprint in cell units. uv spans 2 units per face, and d(angle)/d(uv) shrinks
	// away from the face center, hence the (1 + r^2)^0.75 factor.
	float pixUV = pixAngle * pow(1.0 + dot(uv, uv), 0.75);
	float pixCell = pixUV * N * 0.5;
	float wanted = pixCell * 0.7;
	float sigma = min(wanted, 0.1); // stay inside the cell margin (star is >= 0.25 from edge)
	float amp = gain * pow(mag, 3.5) * (wanted > sigma ? (sigma * sigma) / (wanted * wanted) : 1.0);

	vec2 dv = f - sp;
	float s = amp * exp(-dot(dv, dv) / (2.0 * sigma * sigma));

	vec3 warm = vec3(1.0, 0.72, 0.5);
	vec3 cool = vec3(0.7, 0.82, 1.0);
	vec3 col = mix(mix(warm, vec3(1.0), 0.45), cool, temp);
	return col * s;
}

// Galactic plane orientation (arbitrary tilt) and Milky Way density at direction d.
const vec3 GAL_NORTH = vec3(0.30, 0.86, 0.41);

float milkyBand(vec3 d)
{
	float lat = dot(d, normalize(GAL_NORTH));
	float width = 0.16 + 0.06 * fbm(d * 2.0);
	float band = exp(-lat * lat / (2.0 * width * width));
	float clumps = fbm(d * 5.0);
	float lanes = smoothstep(0.35, 0.65, fbm(d * 9.0 + 11.0)); // dark dust lanes
	return band * (0.35 + 1.1 * clumps) * (1.0 - 0.55 * lanes);
}

vec3 proceduralSky(vec3 d, float pixAngle)
{
	float band = milkyBand(d);
	vec3 col = vec3(0.50, 0.46, 0.55) * band * 0.26;

	float boost = 1.0 + 2.5 * band; // more faint stars inside the Milky Way
	col += starLayer(d, pixAngle, 40.0, 0.14, 1.60, 0.0);
	col += starLayer(d, pixAngle, 90.0, 0.16 * boost, 1.10, 17.0);
	col += starLayer(d, pixAngle, 200.0, 0.18 * boost, 0.80, 53.0);
	col += starLayer(d, pixAngle, 400.0, 0.16 * boost, 0.60, 91.0);
	return col;
}

// ---------------------------------------------------------------------------------------
// Equirectangular texture
// ---------------------------------------------------------------------------------------
vec3 textureSky(vec3 d)
{
	vec2 uv = vec2(atan(d.z, d.x) * 0.15915494 + 0.5, acos(clamp(d.y, -1.0, 1.0)) * 0.31830989);
	// atan() jumps at the +-pi seam, which would give a garbage derivative (and a visible line)
	// there. A copy of u shifted by half a turn is continuous at the seam, so use whichever of
	// the two has the smaller derivative. All derivatives are taken outside any branch.
	vec2 uv2 = vec2(fract(uv.x + 0.5) - 0.5, uv.y);
	vec2 dx1 = dFdx(uv), dy1 = dFdy(uv);
	vec2 dx2 = dFdx(uv2), dy2 = dFdy(uv2);
	bool useShifted = (abs(dx1.x) + abs(dy1.x)) > (abs(dx2.x) + abs(dy2.x));
	return textureGrad(uSky, uv, useShifted ? dx2 : dx1, useShifted ? dy2 : dy1).rgb;
}
