#version 410 core

// Final pass: HDR scene + bloom -> exposure -> ACES filmic tonemap -> sRGB -> screen.
//
// ARTISTIC LIBERTY: bloom and tonemapping imitate a camera / the eye. They are not part of the
// physics, but they are needed because the disk spans many orders of magnitude in brightness.
in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uScene;
uniform sampler2D uBloom;
uniform float uExposure;
uniform float uBloomStrength;
uniform float uFade; // 1 = normal, 0 = black (fade in / out when recording)

// Narkowicz's fit of the ACES filmic curve (input: linear, output: ~display-linear 0..1).
vec3 acesFilm(vec3 x)
{
	return clamp((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0);
}

vec3 srgbEncode(vec3 c)
{
	vec3 lo = c * 12.92;
	vec3 hi = 1.055 * pow(c, vec3(1.0 / 2.4)) - 0.055;
	return mix(lo, hi, step(vec3(0.0031308), c));
}

float hash12(vec2 p)
{
	vec3 p3 = fract(vec3(p.xyx) * 0.1031);
	p3 += dot(p3, p3.yzx + 33.33);
	return fract((p3.x + p3.y) * p3.z);
}

void main()
{
	vec3 hdr = texture(uScene, vUV).rgb + uBloomStrength * texture(uBloom, vUV).rgb;
	vec3 ldr = srgbEncode(acesFilm(hdr * uExposure));
	// +-0.5/255 triangular dither hides banding in the dark gradients around the hole.
	float dither = hash12(gl_FragCoord.xy) + hash12(gl_FragCoord.xy + 17.0) - 1.0;
	fragColor = vec4((ldr + dither / 255.0) * uFade, 1.0);
}
