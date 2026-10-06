#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

// One camera keyframe: where the camera is, where it looks, its field of view, and how much of the
// observer model's motion it has ("boost" in [0, 1], see Observer.h).
struct Keyframe
{
	float time = 0.f; // seconds
	glm::vec3 position{0.f, 0.f, 50.f};
	float yaw = -90.f;   // degrees (unwrapped along the path so it interpolates the short way)
	float pitch = 0.f;   // degrees
	float fov = 60.f;    // degrees
	float boost = 0.f;
	float roll = 0.f;    // degrees about the view direction (unwrapped along the path)
	float look = 0.f;    // 0..1: how strongly the camera is aimed at the black hole (overrides yaw/pitch)
};

// A smooth camera flight through keyframes: every channel (position, yaw, pitch, fov, boost) is
// interpolated with a cubic Hermite spline whose tangents are the Catmull-Rom finite differences
// (aware of uneven time spacing). The end tangents are zero, so the camera eases in at the start
// and eases out at the end.
class CameraPath
{
public:
	std::vector<Keyframe> keys; // sorted by time

	void Clear() { keys.clear(); }
	// Inserts a keyframe in time order (keyframes closer than 10 ms are nudged apart). Returns its index.
	int Add(const Keyframe& k);
	void Remove(int index);
	// Re-sorts, de-duplicates times and unwraps yaw. Call after editing `keys` directly.
	void Normalize();

	float Duration() const { return keys.empty() ? 0.f : keys.back().time; }
	Keyframe Sample(float t) const;

	// Plain-text format, one keyframe per line. `observerModel` is stored with the path.
	bool Save(const std::string& file, int observerModel) const;
	bool Load(const std::string& file, int* observerModel = nullptr);

	// The ~44 s tour: a hyperbolic fly-by. Distant lensing, a fast approach, a close pass at 7.5M
	// at about 0.4c with the hole kept centered, then flying back out with the hole still framed.
	static CameraPath DefaultTour();

	// Yaw/pitch (degrees) that make a camera at `pos` look at the origin.
	static void LookAtOrigin(const glm::vec3& pos, float& yawDeg, float& pitchDeg);
};
