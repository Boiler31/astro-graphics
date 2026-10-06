#pragma once
#include <string>
#include <vector>

// Reads back the currently bound framebuffer as 8-bit RGB, rows bottom-to-top (OpenGL order).
bool ReadFramebufferRgb(int width, int height, std::vector<unsigned char>& rgb);

// Writes bottom-to-top RGB pixels (as returned by ReadFramebufferRgb) to a PNG.
bool SavePng(const std::string& path, int width, int height, const std::vector<unsigned char>& rgb);

// Reads back the current framebuffer (call before swapping, after drawing) and writes a PNG.
bool SaveScreenshot(const std::string& path, int width, int height);
