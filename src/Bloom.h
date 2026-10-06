#pragma once
#include <GL/glew.h>
#include <vector>

#include "Framebuffer.h"
#include "Program.h"

// Bloom: a mip-like chain of progressively half-size HDR targets. The scene is downsampled down
// the chain (13-tap filter, with Karis averaging on the first level so single ultra-bright
// pixels cannot flicker), then upsampled back up, adding each level into the one above it with a
// tent filter. The result is a smooth glow with a very wide falloff, in linear HDR.
// (Dual-filter bloom as used in Call of Duty: Advanced Warfare, Jimenez 2014.)
//
// Needs a VAO bound while running (the passes draw a vertex-buffer-less full-screen triangle).
class Bloom
{
public:
	// Loads the shader pair; returns false on a compile/link error (see GetLog()).
	bool Load();
	// Run on an HDR scene texture of size w x h. Returns the texture holding the final bloom
	// (half the scene's size).
	GLuint Run(GLuint sceneTexture, int w, int h);

	const std::string& GetLog() const { return log; }

private:
	static constexpr int kLevels = 6;
	Program down, up;
	std::vector<Framebuffer> levels = std::vector<Framebuffer>(kLevels);
	std::string log;
};
