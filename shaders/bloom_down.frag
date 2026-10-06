#version 410 core

// Bloom downsample: 13 bilinear taps arranged so each output texel is a smooth, energy-
// preserving blur of a 4x4 source area (Jimenez, "Next Generation Post Processing in Call of
// Duty: Advanced Warfare", 2014). The first level uses Karis averaging: each group of taps is
// weighted by 1 / (1 + luma) so a lone pixel that is 1000x brighter than its neighbours cannot
// dominate (and flicker as the camera moves).
in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uSrc;
uniform vec2 uTexelSize; // 1 / source size
uniform int uFirst;      // 1 on the first level (apply Karis averaging)

float luma(vec3 c)
{
	return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

vec3 karis(vec3 c)
{
	return c / (1.0 + luma(c));
}

void main()
{
	vec2 t = uTexelSize;
	vec3 a = texture(uSrc, vUV + t * vec2(-2.0, 2.0)).rgb;
	vec3 b = texture(uSrc, vUV + t * vec2(0.0, 2.0)).rgb;
	vec3 c = texture(uSrc, vUV + t * vec2(2.0, 2.0)).rgb;
	vec3 d = texture(uSrc, vUV + t * vec2(-2.0, 0.0)).rgb;
	vec3 e = texture(uSrc, vUV).rgb;
	vec3 f = texture(uSrc, vUV + t * vec2(2.0, 0.0)).rgb;
	vec3 g = texture(uSrc, vUV + t * vec2(-2.0, -2.0)).rgb;
	vec3 h = texture(uSrc, vUV + t * vec2(0.0, -2.0)).rgb;
	vec3 i = texture(uSrc, vUV + t * vec2(2.0, -2.0)).rgb;
	vec3 j = texture(uSrc, vUV + t * vec2(-1.0, 1.0)).rgb;
	vec3 k = texture(uSrc, vUV + t * vec2(1.0, 1.0)).rgb;
	vec3 l = texture(uSrc, vUV + t * vec2(-1.0, -1.0)).rgb;
	vec3 m = texture(uSrc, vUV + t * vec2(1.0, -1.0)).rgb;

	vec3 result;
	if (uFirst == 1) {
		vec3 g0 = (a + b + d + e) * 0.25;
		vec3 g1 = (b + c + e + f) * 0.25;
		vec3 g2 = (d + e + g + h) * 0.25;
		vec3 g3 = (e + f + h + i) * 0.25;
		vec3 g4 = (j + k + l + m) * 0.25;
		result = (karis(g0) + karis(g1) + karis(g2) + karis(g3)) * 0.125 + karis(g4) * 0.5;
	} else {
		result = e * 0.125 + (a + c + g + i) * 0.03125 + (b + d + f + h) * 0.0625 + (j + k + l + m) * 0.125;
	}
	fragColor = vec4(result, 1.0);
}
