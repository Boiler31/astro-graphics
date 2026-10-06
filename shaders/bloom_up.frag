#version 410 core

// Bloom upsample: 3x3 tent filter of the smaller level. The pass renders with additive blending,
// so the result is added onto the larger level's own (downsampled) content.
in vec2 vUV;
out vec4 fragColor;

uniform sampler2D uSrc;
uniform vec2 uTexelSize; // 1 / source (smaller level) size

void main()
{
	vec2 t = uTexelSize;
	vec3 s = texture(uSrc, vUV + t * vec2(-1.0, 1.0)).rgb
	       + texture(uSrc, vUV + t * vec2(0.0, 1.0)).rgb * 2.0
	       + texture(uSrc, vUV + t * vec2(1.0, 1.0)).rgb
	       + texture(uSrc, vUV + t * vec2(-1.0, 0.0)).rgb * 2.0
	       + texture(uSrc, vUV).rgb * 4.0
	       + texture(uSrc, vUV + t * vec2(1.0, 0.0)).rgb * 2.0
	       + texture(uSrc, vUV + t * vec2(-1.0, -1.0)).rgb
	       + texture(uSrc, vUV + t * vec2(0.0, -1.0)).rgb * 2.0
	       + texture(uSrc, vUV + t * vec2(1.0, -1.0)).rgb;
	fragColor = vec4(s / 16.0, 1.0);
}
