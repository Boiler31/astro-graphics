#include "Screenshot.h"

#include <GL/glew.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <iostream>
#include <vector>

bool SaveScreenshot(const std::string& path, int width, int height)
{
	std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 3);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
	glPixelStorei(GL_PACK_ALIGNMENT, 4);

	stbi_flip_vertically_on_write(1); // GL origin is bottom-left, PNG top-left
	if (!stbi_write_png(path.c_str(), width, height, 3, pixels.data(), width * 3)) {
		std::cerr << "Failed to write " << path << std::endl;
		return false;
	}
	std::cout << "Saved " << path << std::endl;
	return true;
}
