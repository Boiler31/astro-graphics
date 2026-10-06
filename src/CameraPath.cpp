#include "CameraPath.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>

namespace
{
constexpr int kChannels = 9; // x y z yaw pitch fov boost roll look

std::array<float, kChannels> Pack(const Keyframe& k)
{
	return {k.position.x, k.position.y, k.position.z, k.yaw, k.pitch, k.fov, k.boost, k.roll, k.look};
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
	k.look = std::clamp(v[8], 0.f, 1.f);
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
	out << "# camera path v1: key time x y z yaw pitch fov boost [roll look]\n";
	out << "observer " << observerModel << "\n";
	for (const Keyframe& k : keys) {
		out << "key " << k.time << " " << k.position.x << " " << k.position.y << " " << k.position.z << " " << k.yaw
		    << " " << k.pitch << " " << k.fov << " " << k.boost << " " << k.roll << " " << k.look << "\n";
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
				if (ss >> r) k.roll = r; // roll and look are optional (older files omit them)
				if (ss >> r) k.look = r;
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
	// A hyperbolic fly-by of the black hole: the camera falls in along a conic orbit
	//     r(phi) = p / (1 + e cos(phi)),   p = r_p (1 + e),
	// reaches its closest approach r_p at phi = 0, whips past, and flies back out the other side,
	// aimed at the hole the whole time. Keyframes are generated from that orbit so the spline
	// follows a smooth arc; time is weighted so the far parts go by briskly and the encounter
	// (where things look best) gets the screen time.
	const float rPeri = 7.5f;                       // closest approach, in M (ISCO is 6M)
	const float ecc = 1.5f;                         // eccentricity > 1: unbound, swings past
	const float p = rPeri * (1.f + ecc);
	const float rStart = 220.f;                     // where the shot begins and ends
	const float phiMax = glm::degrees(std::acos((p / rStart - 1.f) / ecc)); // ~127.6 degrees
	const float totalTime = 44.f;                   // seconds
	const int n = 31;                               // keyframes (odd, so one lands exactly on periapsis)

	struct Pt { float phi, r, t; };
	std::vector<Pt> pts(n);
	float acc = 0.f;
	for (int i = 0; i < n; i++) {
		pts[i].phi = -phiMax + 2.f * phiMax * static_cast<float>(i) / (n - 1);
		pts[i].r = p / (1.f + ecc * std::cos(glm::radians(pts[i].phi)));
		if (i > 0) {
			// Time per degree grows with distance (r^0.7): between the real law (angular rate ~ 1/r^2,
			// far too slow when far away) and a constant angular rate.
			float rMid = 0.5f * (pts[i].r + pts[i - 1].r);
			acc += std::pow(rMid, 0.7f) * (pts[i].phi - pts[i - 1].phi);
		}
		pts[i].t = acc;
	}

	CameraPath path;
	for (const Pt& pt : pts) {
		const float az = glm::radians(pt.phi);
		// Elevation above the disk plane: higher during the pass so the camera clears the disk.
		const float el = glm::radians(9.f + 6.f * std::exp(-(pt.phi / 45.f) * (pt.phi / 45.f)));
		const float lr = std::log(pt.r / rPeri); // 0 at periapsis
		Keyframe k;
		k.time = pt.t / acc * totalTime;
		k.position = pt.r * glm::vec3(std::cos(el) * std::sin(az), std::sin(el), std::cos(el) * std::cos(az));
		LookAtOrigin(k.position, k.yaw, k.pitch);
		k.look = 1.f; // keep the hole centered (main compensates for aberration when the camera is fast)
		k.fov = 55.f + 45.f * std::exp(-(lr / 0.9f) * (lr / 0.9f)); // 100 degrees at periapsis keeps the hole in frame
		k.boost = std::exp(-(lr / 0.5f) * (lr / 0.5f));              // full orbital speed at periapsis
		path.keys.push_back(k);
	}
	path.Normalize();
	return path;
}
