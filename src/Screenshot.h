#pragma once
#include <string>

// Reads back the current framebuffer (call before swapping, after drawing) and writes a PNG.
bool SaveScreenshot(const std::string& path, int width, int height);
