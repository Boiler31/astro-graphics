#pragma once
#include <GL/glew.h>
#include <string>

// Loads an image file into a mipmapped 2D texture (GL_REPEAT in S, clamped in T, so it suits
// equirectangular sky maps). Returns 0 if the file does not exist or cannot be decoded.
// The data is uploaded as-is (no sRGB conversion); see the note in blackhole.frag.
GLuint LoadTexture2D(const std::string& path, int* outWidth = nullptr, int* outHeight = nullptr);
