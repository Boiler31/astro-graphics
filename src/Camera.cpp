#include "Camera.h"

#include <GLFW/glfw3.h>
#include <glm/gtc/constants.hpp>
#include <algorithm>
#include <cmath>

void Camera::Reset()
{
	position = glm::vec3(0.f, 0.f, 50.f);
	yaw = -90.f;
	pitch = 0.f;
	fovDeg = 60.f;
}

glm::vec3 Camera::Forward() const
{
	float y = glm::radians(yaw), p = glm::radians(pitch);
	return glm::normalize(glm::vec3(std::cos(p) * std::cos(y), std::sin(p), std::cos(p) * std::sin(y)));
}

glm::vec3 Camera::Right() const
{
	return glm::normalize(glm::cross(Forward(), glm::vec3(0.f, 1.f, 0.f)));
}

glm::vec3 Camera::Up() const
{
	return glm::cross(Right(), Forward());
}

float Camera::TanHalfFov() const
{
	return std::tan(glm::radians(fovDeg) * 0.5f);
}

void Camera::SetOrientation(float yawDeg, float pitchDeg)
{
	yaw = yawDeg;
	pitch = std::clamp(pitchDeg, -89.f, 89.f);
}

void Camera::Rotate(float dx, float dy)
{
	const float sensitivity = 0.12f; // degrees per pixel
	yaw += dx * sensitivity;
	pitch = std::clamp(pitch - dy * sensitivity, -89.f, 89.f);
}

void Camera::Update(GLFWwindow* window, float dt)
{
	// Speed grows with distance from the origin so it feels right both far away and close in.
	float dist = glm::length(position);
	float speed = baseSpeed * std::max(dist / 30.f, 0.1f);
	if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
		speed *= 4.f;
	}

	glm::vec3 move(0.f);
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) move += Forward();
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) move -= Forward();
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) move += Right();
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) move -= Right();
	if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) move += glm::vec3(0.f, 1.f, 0.f);
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) move -= glm::vec3(0.f, 1.f, 0.f);
	if (glm::length(move) > 0.f) {
		position += glm::normalize(move) * speed * dt;
	}
}
