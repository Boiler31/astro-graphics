#version 410 core

#include "sky.glsl"

// M1: flat-space ray tracer. Every pixel gets a straight ray from the camera and looks up the
// sky in that direction. M2 will bend the rays around the black hole before the lookup.
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

void main()
{
	vec2 ndc = vUV * 2.0 - 1.0;
	ndc.x *= uResolution.x / uResolution.y;
	vec3 dir = normalize(uCamForward + ndc.x * uTanHalfFov * uCamRight + ndc.y * uTanHalfFov * uCamUp);

	// Approximate angle covered by one pixel (at screen center).
	float pixAngle = 2.0 * uTanHalfFov / uResolution.y;

	vec3 col = (uSkyMode == 1) ? textureSky(dir) : proceduralSky(dir, pixAngle);
	fragColor = vec4(col * uExposure, 1.0);
}
