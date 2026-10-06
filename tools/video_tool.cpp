// VideoTool: re-encodes an mp4 at a lower bitrate using Windows' built-in Media Foundation codecs
// (no ffmpeg needed). Windows only.
//
//   VideoTool compress IN.mp4 OUT.mp4 --target-mb 50     aim for a file under 50 MB (retries lower if over)
//   VideoTool compress IN.mp4 OUT.mp4 --bitrate 12       fixed video bitrate in Mbit/s
//
// Decoded frames go straight to VideoWriter (the same encoder the renderer uses). Audio is dropped
// (the renders have none).
#ifndef _WIN32
#include <iostream>
int main()
{
	std::cerr << "VideoTool is only implemented for Windows." << std::endl;
	return 1;
}
#else

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "VideoWriter.h"

using Microsoft::WRL::ComPtr;

namespace
{
std::wstring Widen(const std::string& s)
{
	int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
	std::wstring w(static_cast<size_t>(n), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), n);
	return w;
}

struct VideoInfo
{
	int width = 0, height = 0;
	int fps = 30;
	double durationSec = 0.0;
};

// Opens a decoder that delivers top-down RGB32 frames (Media Foundation converts from H.264/YUV).
bool OpenReader(const std::string& file, ComPtr<IMFSourceReader>& reader, VideoInfo& info)
{
	ComPtr<IMFAttributes> attrs;
	MFCreateAttributes(&attrs, 1);
	attrs->SetUINT32(MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING, TRUE);
	if (FAILED(MFCreateSourceReaderFromURL(Widen(file).c_str(), attrs.Get(), &reader))) {
		std::cerr << "Cannot open " << file << std::endl;
		return false;
	}
	reader->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE);
	reader->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM, TRUE);

	ComPtr<IMFMediaType> want;
	MFCreateMediaType(&want);
	want->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	want->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
	if (FAILED(reader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, nullptr, want.Get()))) {
		std::cerr << "The decoder cannot produce RGB frames for " << file << std::endl;
		return false;
	}

	ComPtr<IMFMediaType> actual;
	reader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM, &actual);
	UINT32 w = 0, h = 0, num = 0, den = 1;
	MFGetAttributeSize(actual.Get(), MF_MT_FRAME_SIZE, &w, &h);
	MFGetAttributeRatio(actual.Get(), MF_MT_FRAME_RATE, &num, &den);
	info.width = static_cast<int>(w);
	info.height = static_cast<int>(h);
	info.fps = den ? static_cast<int>(std::lround(static_cast<double>(num) / den)) : 30;

	PROPVARIANT var;
	PropVariantInit(&var);
	if (SUCCEEDED(reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &var)) &&
	    var.vt == VT_UI8) {
		info.durationSec = static_cast<double>(var.uhVal.QuadPart) / 1.0e7;
	}
	PropVariantClear(&var);
	return info.width > 0 && info.height > 0;
}

// Decodes `in` and encodes it to `out` at `bitrateKbps`. Returns the number of frames written (0 on failure).
int Transcode(const std::string& in, const std::string& out, int bitrateKbps, const VideoInfo& expect)
{
	ComPtr<IMFSourceReader> reader;
	VideoInfo info;
	if (!OpenReader(in, reader, info)) return 0;

	VideoWriter writer;
	if (!writer.Open(out, info.width & ~1, info.height & ~1, info.fps, bitrateKbps)) return 0;

	const int w = info.width & ~1, h = info.height & ~1;
	std::vector<unsigned char> rgb(static_cast<size_t>(w) * h * 3); // bottom-up RGB24 for VideoWriter
	int frames = 0;
	const int totalGuess = static_cast<int>(std::lround(expect.durationSec * info.fps));
	for (;;) {
		DWORD streamIndex = 0, flags = 0;
		LONGLONG ts = 0;
		ComPtr<IMFSample> sample;
		if (FAILED(reader->ReadSample(MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0, &streamIndex, &flags, &ts, &sample))) {
			std::cerr << "Decode error at frame " << frames << std::endl;
			break;
		}
		if (flags & MF_SOURCE_READERF_ENDOFSTREAM) break;
		if (!sample) continue;

		ComPtr<IMFMediaBuffer> buffer;
		if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) continue;

		// Prefer the 2D interface: it gives the first row and a signed pitch (negative = bottom-up).
		BYTE* scan0 = nullptr;
		LONG pitch = 0;
		BYTE* raw = nullptr;
		ComPtr<IMF2DBuffer> buf2d;
		bool locked2d = SUCCEEDED(buffer.As(&buf2d)) && SUCCEEDED(buf2d->Lock2D(&scan0, &pitch));
		if (!locked2d) {
			if (FAILED(buffer->Lock(&raw, nullptr, nullptr))) continue;
			scan0 = raw;
			pitch = info.width * 4;
		}
		for (int y = 0; y < h; y++) {
			const BYTE* src = scan0 + static_cast<ptrdiff_t>(y) * pitch; // row y from the top
			unsigned char* dst = rgb.data() + static_cast<size_t>(h - 1 - y) * w * 3;
			for (int x = 0; x < w; x++) {
				dst[3 * x + 0] = src[4 * x + 2]; // R (source is B,G,R,x)
				dst[3 * x + 1] = src[4 * x + 1];
				dst[3 * x + 2] = src[4 * x + 0];
			}
		}
		if (locked2d) buf2d->Unlock2D(); else buffer->Unlock();

		if (!writer.AddFrame(rgb.data())) {
			std::cerr << "Encode error at frame " << frames << std::endl;
			break;
		}
		frames++;
		if (frames % 120 == 0) {
			std::cout << "  frame " << frames << (totalGuess ? "/" + std::to_string(totalGuess) : std::string()) << std::endl;
		}
	}
	writer.Close();
	return frames;
}
} // namespace

int main(int argc, char** argv)
{
	if (argc < 4 || std::strcmp(argv[1], "compress") != 0) {
		std::cerr << "usage: VideoTool compress IN.mp4 OUT.mp4 [--target-mb N | --bitrate MBPS]" << std::endl;
		return 2;
	}
	const std::string in = argv[2], out = argv[3];
	double targetMb = 0.0, bitrateMbps = 0.0;
	for (int i = 4; i + 1 < argc; i += 2) {
		if (std::strcmp(argv[i], "--target-mb") == 0) targetMb = std::atof(argv[i + 1]);
		else if (std::strcmp(argv[i], "--bitrate") == 0) bitrateMbps = std::atof(argv[i + 1]);
	}
	if (targetMb <= 0.0 && bitrateMbps <= 0.0) targetMb = 50.0;
	if (std::filesystem::absolute(in) == std::filesystem::absolute(out)) {
		std::cerr << "Output must be a different file from the input." << std::endl;
		return 2;
	}

	CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(MFStartup(MF_VERSION))) {
		std::cerr << "MFStartup failed" << std::endl;
		return 1;
	}

	VideoInfo info;
	{
		ComPtr<IMFSourceReader> probe;
		if (!OpenReader(in, probe, info)) {
			MFShutdown();
			return 1;
		}
	}
	std::cout << "Input: " << info.width << "x" << info.height << " @" << info.fps << " fps, " << info.durationSec << " s, "
	          << static_cast<double>(std::filesystem::file_size(in)) / 1048576.0 << " MB" << std::endl;

	// Pick the video bitrate. With --target-mb: leave ~8% for container overhead and rate-control
	// wobble; if the result is still over the target, retry lower (up to 4 attempts).
	double kbps = bitrateMbps > 0.0 ? bitrateMbps * 1000.0
	                                : (targetMb * 1048576.0 * 8.0 * 0.92) / std::max(info.durationSec, 0.1) / 1000.0;
	int status = 1;
	for (int attempt = 1; attempt <= (bitrateMbps > 0.0 ? 1 : 4); attempt++) {
		std::cout << "Attempt " << attempt << ": encoding at " << kbps / 1000.0 << " Mbit/s..." << std::endl;
		int frames = Transcode(in, out, static_cast<int>(kbps), info);
		if (frames == 0) break;
		const double mb = static_cast<double>(std::filesystem::file_size(out)) / 1048576.0;
		std::cout << "  -> " << frames << " frames, " << mb << " MB" << std::endl;
		if (targetMb <= 0.0 || mb <= targetMb) {
			status = 0;
			break;
		}
		kbps *= std::max(0.6, 0.93 * targetMb / mb); // overshot: scale the bitrate down and retry
	}
	MFShutdown();
	return status;
}
#endif
