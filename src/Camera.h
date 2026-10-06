#pragma once
#include <glm/glm.hpp>

struct GLFWwindow;

// Free-fly camera. Positions are in units of M (G = c = M = 1, see PLAN.md), so a camera at
// z = 50 sits 50 gravitational radii (GM/c^2) from a black hole at the origin.
class Camera
{
public:
	glm::vec3 position = glm::vec3(0.f, 0.f, 50.f);
	float fovDeg = 60.f;   // vertical field of view
	float baseSpeed = 10.f; // M per second at distance ~ 30M (scaled with distance, see Update)
	float rollDeg = 0.f;   // rotation about the viewing direction (positive = clockwise as seen by the camera)

	Camera() { Reset(); }
	void Reset();

	glm::vec3 Forward() const;
	glm::vec3 Right() const;
	glm::vec3 Up() const;
	float TanHalfFov() const;

	void SetOrientation(float yawDeg, float pitchDeg);
	float YawDeg() const { return yaw; }
	float PitchDeg() const { return pitch; }

	// Mouse-look: dx/dy in pixels of cursor movement.
	void Rotate(float dx, float dy);
	// WASD move, Q/E down/up, Z/C roll left/right (X levels the horizon), Shift = 4x speed.
	void Update(GLFWwindow* window, float dt);

private:
	float yaw = -90.f;  // degrees; -90 looks down -z
	float pitch = 0.f;
};
