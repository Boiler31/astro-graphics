#include "CameraPath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
constexpr int kChannels = 8; // x y z yaw pitch fov boost roll

std::array<float, kChannels> Pack(const Keyframe& k)
{
	return {k.position.x, k.position.y, k.position.z, k.yaw, k.pitch, k.fov, k.boost, k.roll};
}

Keyframe Unpack(const std::array<float, kChannels>& v, float t)
{
	Keyframe k;
	k.time = t;
	k.position = glm::vec3(v[0], v[1], v[2]);
	k.yaw = v[3];
	k.pitch = std::clamp(v[4], -89.f, 89.f);
	k.fov = std::clamp(v[5], 10.f, 150.f);
	k.boost = std::clamp(v[6], 0.f, 1.f);
	k.roll = v[7];
	return k;
}
} // namespace

int CameraPath::Add(const Keyframe& k)
{
	keys.push_back(k);
	// Keep a stable identity for the new key through the sort: it is the one we just appended.
	Keyframe added = keys.back();
	Normalize();
	for (size_t i = 0; i < keys.size(); i++) {
		if (keys[i].position == added.position && keys[i].fov == added.fov && keys[i].boost == added.boost) {
			return static_cast<int>(i);
		}
	}
	return static_cast<int>(keys.size()) - 1;
}

void CameraPath::Remove(int index)
{
	if (index >= 0 && index < static_cast<int>(keys.size())) {
		keys.erase(keys.begin() + index);
		Normalize();
	}
}

void CameraPath::Normalize()
{
	std::stable_sort(keys.begin(), keys.end(), [](const Keyframe& a, const Keyframe& b) { return a.time < b.time; });
	for (size_t i = 1; i < keys.size(); i++) {
		if (keys[i].time < keys[i - 1].time + 0.01f) {
			keys[i].time = keys[i - 1].time + 0.01f;
		}
		// Unwrap yaw so interpolation takes the short way around.
		while (keys[i].yaw - keys[i - 1].yaw > 180.f) keys[i].yaw -= 360.f;
		while (keys[i].yaw - keys[i - 1].yaw < -180.f) keys[i].yaw += 360.f;
		while (keys[i].roll - keys[i - 1].roll > 180.f) keys[i].roll -= 360.f;
		while (keys[i].roll - keys[i - 1].roll < -180.f) keys[i].roll += 360.f;
	}
}

Keyframe CameraPath::Sample(float t) const
{
	if (keys.empty()) {
		return Keyframe();
	}
	if (keys.size() == 1) {
		return keys[0];
	}
	t = std::clamp(t, keys.front().time, keys.back().time);

	// Segment [i, i+1] containing t.
	size_t i = 0;
	while (i + 2 < keys.size() && t > keys[i + 1].time) {
		i++;
	}
	const size_t n = keys.size();
	auto tangent = [&](size_t j) {
		std::array<float, kChannels> m{};
		if (j == 0 || j == n - 1) {
			return m; // zero velocity at the ends: ease in / ease out
		}
		auto a = Pack(keys[j - 1]);
		auto b = Pack(keys[j + 1]);
		float dt = keys[j + 1].time - keys[j - 1].time;
		for (int c = 0; c < kChannels; c++) m[c] = (b[c] - a[c]) / dt;
		return m;
	};

	const float h = keys[i + 1].time - keys[i].time;
	const float u = (t - keys[i].time) / h;
	const float u2 = u * u, u3 = u2 * u;
	const float h00 = 2.f * u3 - 3.f * u2 + 1.f, h10 = u3 - 2.f * u2 + u;
	const float h01 = -2.f * u3 + 3.f * u2, h11 = u3 - u2;

	auto p0 = Pack(keys[i]), p1 = Pack(keys[i + 1]);
	auto m0 = tangent(i), m1 = tangent(i + 1);
	std::array<float, kChannels> out{};
	for (int c = 0; c < kChannels; c++) {
		out[c] = h00 * p0[c] + h10 * h * m0[c] + h01 * p1[c] + h11 * h * m1[c];
	}
	return Unpack(out, t);
}

bool CameraPath::Save(const std::string& file, int observerModel) const
{
	std::ofstream out(file);
	if (!out) {
		std::cerr << "Cannot write " << file << std::endl;
		return false;
	}
	out << "# camera path v1: key time x y z yaw pitch fov boost [roll]\n";
	out << "observer " << observerModel << "\n";
	for (const Keyframe& k : keys) {
		out << "key " << k.time << " " << k.position.x << " " << k.position.y << " " << k.position.z << " " << k.yaw
		    << " " << k.pitch << " " << k.fov << " " << k.boost << " " << k.roll << "\n";
	}
	return true;
}

bool CameraPath::Load(const std::string& file, int* observerModel)
{
	std::ifstream in(file);
	if (!in) {
		return false;
	}
	std::vector<Keyframe> loaded;
	std::string line;
	while (std::getline(in, line)) {
		if (line.empty() || line[0] == '#') continue;
		std::istringstream ss(line);
		std::string tag;
		ss >> tag;
		if (tag == "observer") {
			int m = 0;
			ss >> m;
			if (observerModel) *observerModel = m;
		} else if (tag == "key") {
			Keyframe k;
			ss >> k.time >> k.position.x >> k.position.y >> k.position.z >> k.yaw >> k.pitch >> k.fov >> k.boost;
			if (ss) {
				float r = 0.f;
				if (ss >> r) k.roll = r; // roll is optional (older files omit it)
				loaded.push_back(k);
			}
		}
	}
	keys = loaded;
	Normalize();
	return !keys.empty();
}

void CameraPath::LookAtOrigin(const glm::vec3& pos, float& yawDeg, float& pitchDeg)
{
	glm::vec3 f = glm::normalize(-pos);
	yawDeg = glm::degrees(std::atan2(f.z, f.x));
	pitchDeg = glm::degrees(std::asin(std::clamp(f.y, -1.f, 1.f)));
}

CameraPath CameraPath::DefaultTour()
{
	// (time s, radius M, azimuth deg, elevation deg above the disk plane, fov deg, boost)
	struct Spec { float t, r, az, el, fov, boost; };
	const Spec specs[] = {
		{0.f, 220.f, -25.f, 7.f, 50.f, 0.f},   // far away: a small dark spot, stars bent around it
		{10.f, 120.f, -15.f, 8.f, 55.f, 0.f},
		{20.f, 50.f, 0.f, 10.f, 60.f, 0.f},    // the disk appears, one side much brighter
		{30.f, 28.f, 25.f, 3.f, 62.f, 0.f},    // near edge-on: far side lensed over and under the shadow
		{40.f, 18.f, 70.f, 4.f, 62.f, 0.f},
		{50.f, 14.f, 140.f, 12.f, 66.f, 0.f},  // sweeping around: the bright side flips
		{58.f, 10.f, 200.f, 20.f, 70.f, 0.f},
		{66.f, 6.5f, 250.f, 14.f, 70.f, 0.f},  // descending: clock readout drops
		{72.f, 4.6f, 275.f, 8.f, 72.f, 1.f},   // free fall begins
		{75.f, 3.3f, 285.f, 5.f, 75.f, 1.f},   // plunge toward the photon sphere
	};
	CameraPath path;
	for (const Spec& s : specs) {
		const float az = glm::radians(s.az), el = glm::radians(s.el);
		Keyframe k;
		k.time = s.t;
		k.position = s.r * glm::vec3(std::cos(el) * std::sin(az), std::sin(el), std::cos(el) * std::cos(az));
		LookAtOrigin(k.position, k.yaw, k.pitch);
		k.fov = s.fov;
		k.boost = s.boost;
		path.keys.push_back(k);
	}
	path.Normalize();
	return path;
}
