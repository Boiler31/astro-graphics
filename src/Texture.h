#pragma once
#include <GL/glew.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

// Loads an image file into a mipmapped 2D texture (GL_REPEAT in S, clamped in T, so it suits
// equirectangular sky maps). Returns 0 if the file does not exist or cannot be decoded.
// The image is treated as sRGB-encoded and sampled as linear values.
GLuint LoadTexture2D(const std::string& path, int* outWidth = nullptr, int* outHeight = nullptr);

// Uploads RGB float colors as a 1D lookup texture (RGB16F, linear filtering, clamped).
GLuint CreateColorLut1D(const std::vector<glm::vec3>& colors);
