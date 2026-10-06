#version 410 core

// Full-screen triangle generated from gl_VertexID; no vertex buffer needed.
// Draw with glDrawArrays(GL_TRIANGLES, 0, 3) and any (empty) VAO bound.
out vec2 vUV;

void main()
{
	vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2); // (0,0) (2,0) (0,2)
	vUV = p;                                                // 0..1 across the visible part
	gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
