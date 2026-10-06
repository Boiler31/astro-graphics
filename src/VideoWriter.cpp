#include "VideoWriter.h"

#include <iostream>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <codecapi.h>
#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

struct VideoWriter::Impl
{
	ComPtr<IMFSinkWriter> writer;
	DWORD stream = 0;
	int width = 0, height = 0, fps = 30;
	long long frameIndex = 0;
	bool open = false;
	bool mfStarted = false;
	std::vector<unsigned char> bgra; // top-down BGRA scratch frame
};

namespace
{
bool Check(HRESULT hr, const char* what)
{
	if (FAILED(hr)) {
		std::cerr << "VideoWriter: " << what << " failed (HRESULT 0x" << std::hex << static_cast<unsigned long>(hr)
		          << std::dec << ")" << std::endl;
		return false;
	}
	return true;
}
} // namespace

VideoWriter::VideoWriter() : impl(new Impl) {}

VideoWriter::~VideoWriter()
{
	Close();
	if (impl && impl->mfStarted) {
		MFShutdown();
	}
}

bool VideoWriter::Open(const std::string& file, int width, int height, int fps, int bitrateKbps)
{
	if (width % 2 != 0 || height % 2 != 0) {
		std::cerr << "VideoWriter: width and height must be even" << std::endl;
		return false;
	}
	impl->width = width;
	impl->height = height;
	impl->fps = fps;
	impl->frameIndex = 0;
	impl->bgra.assign(static_cast<size_t>(width) * height * 4, 0);

	CoInitializeEx(nullptr, COINIT_MULTITHREADED); // S_FALSE / RPC_E_CHANGED_MODE are fine here
	if (!Check(MFStartup(MF_VERSION), "MFStartup")) return false;
	impl->mfStarted = true;

	ComPtr<IMFAttributes> attrs;
	if (!Check(MFCreateAttributes(&attrs, 2), "MFCreateAttributes")) return false;
	attrs->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, TRUE); // use the GPU encoder when present
	attrs->SetUINT32(MF_SINK_WRITER_DISABLE_THROTTLING, TRUE);       // we are offline, never drop frames

	int wlen = MultiByteToWideChar(CP_UTF8, 0, file.c_str(), -1, nullptr, 0);
	std::wstring wfile(static_cast<size_t>(wlen), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, file.c_str(), -1, wfile.data(), wlen);
	if (!Check(MFCreateSinkWriterFromURL(wfile.c_str(), nullptr, attrs.Get(), &impl->writer), "MFCreateSinkWriterFromURL"))
		return false;

	// Output: H.264, BT.709 color.
	ComPtr<IMFMediaType> out;
	if (!Check(MFCreateMediaType(&out), "MFCreateMediaType(out)")) return false;
	out->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	out->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264);
	out->SetUINT32(MF_MT_AVG_BITRATE, static_cast<UINT32>(bitrateKbps) * 1000u);
	out->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	out->SetUINT32(MF_MT_MPEG2_PROFILE, eAVEncH264VProfile_High);
	out->SetUINT32(MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_BT709);
	out->SetUINT32(MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_709);
	out->SetUINT32(MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT709);
	MFSetAttributeSize(out.Get(), MF_MT_FRAME_SIZE, width, height);
	MFSetAttributeRatio(out.Get(), MF_MT_FRAME_RATE, fps, 1);
	MFSetAttributeRatio(out.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	if (!Check(impl->writer->AddStream(out.Get(), &impl->stream), "AddStream")) return false;

	// Input: top-down 32-bit RGB; Media Foundation converts to YUV for the encoder.
	ComPtr<IMFMediaType> in;
	if (!Check(MFCreateMediaType(&in), "MFCreateMediaType(in)")) return false;
	in->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	in->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
	in->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	in->SetUINT32(MF_MT_DEFAULT_STRIDE, static_cast<UINT32>(width * 4)); // positive = top-down
	MFSetAttributeSize(in.Get(), MF_MT_FRAME_SIZE, width, height);
	MFSetAttributeRatio(in.Get(), MF_MT_FRAME_RATE, fps, 1);
	MFSetAttributeRatio(in.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	if (!Check(impl->writer->SetInputMediaType(impl->stream, in.Get(), nullptr), "SetInputMediaType")) return false;

	if (!Check(impl->writer->BeginWriting(), "BeginWriting")) return false;
	impl->open = true;
	return true;
}

bool VideoWriter::AddFrame(const unsigned char* rgb)
{
	if (!impl->open) return false;
	const int w = impl->width, h = impl->height;

	// Bottom-up RGB24 -> top-down BGRA32.
	for (int y = 0; y < h; y++) {
		const unsigned char* src = rgb + static_cast<size_t>(h - 1 - y) * w * 3;
		unsigned char* dst = impl->bgra.data() + static_cast<size_t>(y) * w * 4;
		for (int x = 0; x < w; x++) {
			dst[4 * x + 0] = src[3 * x + 2];
			dst[4 * x + 1] = src[3 * x + 1];
			dst[4 * x + 2] = src[3 * x + 0];
			dst[4 * x + 3] = 255;
		}
	}

	const DWORD bytes = static_cast<DWORD>(impl->bgra.size());
	ComPtr<IMFMediaBuffer> buffer;
	if (!Check(MFCreateMemoryBuffer(bytes, &buffer), "MFCreateMemoryBuffer")) return false;
	BYTE* data = nullptr;
	if (!Check(buffer->Lock(&data, nullptr, nullptr), "buffer Lock")) return false;
	std::memcpy(data, impl->bgra.data(), bytes);
	buffer->Unlock();
	buffer->SetCurrentLength(bytes);

	ComPtr<IMFSample> sample;
	if (!Check(MFCreateSample(&sample), "MFCreateSample")) return false;
	sample->AddBuffer(buffer.Get());
	const LONGLONG duration = 10000000LL / impl->fps;
	sample->SetSampleTime(impl->frameIndex * 10000000LL / impl->fps);
	sample->SetSampleDuration(duration);
	if (!Check(impl->writer->WriteSample(impl->stream, sample.Get()), "WriteSample")) return false;
	impl->frameIndex++;
	return true;
}

bool VideoWriter::Close()
{
	if (!impl || !impl->open) return true;
	impl->open = false;
	bool ok = Check(impl->writer->Finalize(), "Finalize");
	impl->writer.Reset();
	return ok;
}

#else // !_WIN32

struct VideoWriter::Impl {};
VideoWriter::VideoWriter() : impl(new Impl) {}
VideoWriter::~VideoWriter() = default;
bool VideoWriter::Open(const std::string&, int, int, int, int)
{
	std::cerr << "VideoWriter: MP4 export is only implemented for Windows; use --recordpng instead." << std::endl;
	return false;
}
bool VideoWriter::AddFrame(const unsigned char*) { return false; }
bool VideoWriter::Close() { return true; }

#endif
