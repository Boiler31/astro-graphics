#include "Screenshot.h"

#include <GL/glew.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <iostream>

bool ReadFramebufferRgb(int width, int height, std::vector<unsigned char>& rgb)
{
	rgb.resize(static_cast<size_t>(width) * height * 3);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
	glPixelStorei(GL_PACK_ALIGNMENT, 4);
	return glGetError() == GL_NO_ERROR;
}

bool SavePng(const std::string& path, int width, int height, const std::vector<unsigned char>& rgb)
{
	stbi_flip_vertically_on_write(1); // GL origin is bottom-left, PNG top-left
	stbi_write_png_compression_level = 4; // a bit faster than the default; matters for long sequences
	if (!stbi_write_png(path.c_str(), width, height, 3, rgb.data(), width * 3)) {
		std::cerr << "Failed to write " << path << std::endl;
		return false;
	}
	return true;
}

bool SaveScreenshot(const std::string& path, int width, int height)
{
	std::vector<unsigned char> pixels;
	if (!ReadFramebufferRgb(width, height, pixels) || !SavePng(path, width, height, pixels)) {
		return false;
	}
	std::cout << "Saved " << path << std::endl;
	return true;
}
