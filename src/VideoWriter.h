#pragma once
#include <memory>
#include <string>

// Writes an H.264 .mp4 using the video encoder built into Windows (Media Foundation), so no
// external tools (ffmpeg) are needed. On an NVIDIA/AMD/Intel GPU the hardware encoder is used.
// Not available on other platforms: Open() then fails with a message.
class VideoWriter
{
public:
	VideoWriter();
	~VideoWriter();
	VideoWriter(const VideoWriter&) = delete;
	VideoWriter& operator=(const VideoWriter&) = delete;

	// width and height must be even (H.264 4:2:0). bitrateKbps ~ 20000-60000 is plenty for 1080p.
	bool Open(const std::string& file, int width, int height, int fps, int bitrateKbps);
	// `rgb` is width*height*3 bytes of 8-bit RGB, rows bottom-to-top as returned by glReadPixels.
	bool AddFrame(const unsigned char* rgb);
	// Flushes and closes the file. Called by the destructor if still open.
	bool Close();

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
