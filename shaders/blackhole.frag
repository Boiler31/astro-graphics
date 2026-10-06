#version 410 core

#include "sky.glsl"
#include "geodesic.glsl"
#include "disk.glsl"

// Schwarzschild black hole renderer. Every pixel's camera ray is marched along the bent light
// path around the black hole (at the origin) until it falls in (black) or escapes (sky lookup by
// the final direction). On the way it can cross the accretion disk (the y = 0 plane), possibly
// several times: the extra crossings are the lensed images of the disk's far side.
in vec2 vUV;
out vec4 fragColor;

uniform vec2 uResolution;
uniform vec3 uCamPos;
uniform vec3 uCamRight;
uniform vec3 uCamUp;
uniform vec3 uCamForward;
uniform float uTanHalfFov;
uniform int uSkyMode;    // 0 = procedural stars, 1 = texture
uniform float uExposure;

uniform int uGR;         // 1 = bend light around the black hole, 0 = straight rays
uniform int uMaxSteps;   // integration step budget per ray
uniform float uStepScale; // dt = uStepScale * (r - 2M)
uniform int uDisk;       // 1 = draw the accretion disk

void main()
{
	vec2 ndc = vUV * 2.0 - 1.0;
	ndc.x *= uResolution.x / uResolution.y;
	vec3 rayDir = normalize(uCamForward + ndc.x * uTanHalfFov * uCamRight + ndc.y * uTanHalfFov * uCamUp);

	// Angle covered by one pixel (at screen center).
	float pixAngle = 2.0 * uTanHalfFov / uResolution.y;

	vec3 dir = rayDir;
	bool captured = false;
	vec3 diskColor = vec3(0.0); // light picked up from the disk along the ray
	float transmittance = 1.0;  // fraction of background light that still gets through

	if (uGR == 1) {
		vec3 x = uCamPos;
		float rCam = length(x);
		if (rCam <= CAPTURE_RADIUS) {
			captured = true; // inside the horizon: static observers don't exist there
		}
		vec3 v = staticObserverVelocity(x, rayDir);
		vec3 hvec = cross(x, v);
		float h2 = dot(hvec, hvec);
		float rFar = max(5000.0, 4.0 * rCam);

		bool done = captured;
		for (int i = 0; i < uMaxSteps && !done; i++) {
			float r = length(x);
			if (r < CAPTURE_RADIUS) {
				captured = true;
				done = true;
			} else if (r > rFar && dot(x, v) > 0.0) {
				done = true; // escaped; v is now the asymptotic direction
			} else {
				vec3 x0 = x, v0 = v;
				float dt = uStepScale * (r - R_HORIZON);
				rk4Step(x, v, h2, dt);

				// Did this step cross the disk plane?
				if (uDisk == 1 && x0.y * x.y < 0.0) {
					float s = x0.y / (x0.y - x.y);
					vec3 p = hermiteSegment(x0, v0, x, v, dt, s);
					float rd = length(p.xz);
					if (rd > R_ISCO && rd < uDiskOuter) {
						vec4 e = diskShade(rd, atan(p.z, p.x));
						diskColor += transmittance * e.a * e.rgb;
						transmittance *= 1.0 - e.a;
						if (transmittance < 0.01) {
							done = true; // effectively opaque: nothing behind it is visible
						}
					}
				}
			}
		}
		if (!done) {
			captured = true; // out of steps: only rays skimming the photon sphere end up here
		}
		dir = normalize(v);
	}

	// Lensing stretches and squeezes the sky, so one pixel can cover many (or few) sky pixels.
	// Estimate the footprint from how fast the final direction changes between neighbors, so
	// stars stay smooth instead of sparkling. (Derivatives are taken after the loop, outside any
	// branch, so they are defined for every pixel in the quad.)
	float footprint = pixAngle;
	if (uGR == 1) {
		footprint = clamp(max(length(dFdx(dir)), length(dFdy(dir))), pixAngle, 24.0 * pixAngle);
	}

	vec3 col = diskColor;
	if (!captured && transmittance > 0.01) {
		vec3 sky = (uSkyMode == 1) ? textureSky(dir) : proceduralSky(dir, footprint);
		col += transmittance * sky;
	}
	fragColor = vec4(col * uExposure, 1.0);
}
