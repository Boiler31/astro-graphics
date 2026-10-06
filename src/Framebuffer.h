#pragma once
#include <GL/glew.h>

// An offscreen RGBA16F (HDR) render target.
class Framebuffer
{
public:
	Framebuffer() = default;
	~Framebuffer();
	Framebuffer(const Framebuffer&) = delete;
	Framebuffer& operator=(const Framebuffer&) = delete;
	Framebuffer(Framebuffer&& o) noexcept;
	Framebuffer& operator=(Framebuffer&& o) noexcept;

	// (Re)creates the target if the size changed. Returns false if the framebuffer is incomplete.
	bool Resize(int width, int height);
	// Binds the framebuffer and sets the viewport to its full size.
	void Bind() const;

	GLuint Texture() const { return tex; }
	GLuint Fbo() const { return fbo; }
	int Width() const { return w; }
	int Height() const { return h; }

private:
	void Destroy();

	GLuint fbo = 0, tex = 0;
	int w = 0, h = 0;
};
