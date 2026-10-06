#include "Framebuffer.h"

#include <iostream>
#include <utility>

Framebuffer::~Framebuffer()
{
	Destroy();
}

Framebuffer::Framebuffer(Framebuffer&& o) noexcept : fbo(o.fbo), tex(o.tex), w(o.w), h(o.h)
{
	o.fbo = o.tex = 0;
	o.w = o.h = 0;
}

Framebuffer& Framebuffer::operator=(Framebuffer&& o) noexcept
{
	if (this != &o) {
		Destroy();
		fbo = o.fbo;
		tex = o.tex;
		w = o.w;
		h = o.h;
		o.fbo = o.tex = 0;
		o.w = o.h = 0;
	}
	return *this;
}

void Framebuffer::Destroy()
{
	if (fbo) glDeleteFramebuffers(1, &fbo);
	if (tex) glDeleteTextures(1, &tex);
	fbo = tex = 0;
	w = h = 0;
}

bool Framebuffer::Resize(int width, int height)
{
	if (fbo && width == w && height == h) {
		return true;
	}
	Destroy();
	w = width;
	h = height;

	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glGenFramebuffers(1, &fbo);
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
	if (!ok) {
		std::cerr << "Framebuffer " << w << "x" << h << " is incomplete" << std::endl;
	}
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	return ok;
}

void Framebuffer::Bind() const
{
	glBindFramebuffer(GL_FRAMEBUFFER, fbo);
	glViewport(0, 0, w, h);
}
