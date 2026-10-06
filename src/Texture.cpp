#include "Texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include <iostream>

GLuint LoadTexture2D(const std::string& path, int* outWidth, int* outHeight)
{
	int w = 0, h = 0, n = 0;
	stbi_uc* data = stbi_load(path.c_str(), &w, &h, &n, 3);
	if (!data) {
		return 0; // missing file is normal (procedural sky is the fallback)
	}

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // RGB rows are not 4-byte aligned in general
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
	glGenerateMipmap(GL_TEXTURE_2D);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
	stbi_image_free(data);

	std::cout << "Loaded " << path << " (" << w << "x" << h << ")" << std::endl;
	if (outWidth) *outWidth = w;
	if (outHeight) *outHeight = h;
	return tex;
}
