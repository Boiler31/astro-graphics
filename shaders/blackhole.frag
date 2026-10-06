#version 410 core

// M0 placeholder: UV gradient. The black hole tracer replaces this in M1-M4.
in vec2 vUV;
out vec4 fragColor;

uniform float uTime;

void main()
{
	// Gradient with a slow pulse so you can tell the frame is live.
	vec3 col = vec3(vUV, 0.5 + 0.5 * sin(uTime));
	fragColor = vec4(col, 1.0);
}
