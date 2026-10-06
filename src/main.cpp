#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <cstdlib>
#include <cstring>
#include <iostream>

#include "Program.h"

static bool g_reloadRequested = false;

static void ErrorCallback(int error, const char* description)
{
	std::cerr << "GLFW error " << error << ": " << description << std::endl;
}

static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	// ImGui chains this callback; ignore keys while a text box has focus.
	if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard) {
		return;
	}
	if (action != GLFW_PRESS) {
		return;
	}
	if (key == GLFW_KEY_ESCAPE) {
		glfwSetWindowShouldClose(window, GLFW_TRUE);
	} else if (key == GLFW_KEY_R) {
		g_reloadRequested = true;
	}
}

int main(int argc, char** argv)
{
	// `--frames N` exits after N frames (used for automated smoke tests).
	int maxFrames = -1;
	for (int i = 1; i < argc; i++) {
		if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
			maxFrames = std::atoi(argv[++i]);
		}
	}

	glfwSetErrorCallback(ErrorCallback);
	if (!glfwInit()) {
		return -1;
	}
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

	GLFWwindow* window = glfwCreateWindow(1280, 720, "Black Hole", nullptr, nullptr);
	if (!window) {
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	glfwSetKeyCallback(window, KeyCallback);

	glewExperimental = GL_TRUE;
	if (glewInit() != GLEW_OK) {
		std::cerr << "Failed to initialize GLEW" << std::endl;
		return -1;
	}
	glGetError(); // GLEW can leave a spurious GL_INVALID_ENUM in core profiles.
	std::cout << "OpenGL " << glGetString(GL_VERSION) << " | " << glGetString(GL_RENDERER) << std::endl;

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init("#version 410 core");

	Program program;
	program.SetShaderFiles("fullscreen.vert", "blackhole.frag");
	if (!program.Load()) {
		std::cerr << "Initial shader build failed; fix the shader and press R." << std::endl;
	}

	// Core profile needs a VAO bound even when the vertex shader reads no attributes.
	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	int frame = 0;
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		if (g_reloadRequested) {
			g_reloadRequested = false;
			std::cout << "Reloading shaders..." << std::endl;
			if (program.Load()) {
				std::cout << "Shaders reloaded." << std::endl;
			}
		}

		int fbw, fbh;
		glfwGetFramebufferSize(window, &fbw, &fbh);
		if (fbw == 0 || fbh == 0) {
			continue; // minimized
		}
		glViewport(0, 0, fbw, fbh);
		glClearColor(0.f, 0.f, 0.f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT);

		if (program.GetPID() != 0) {
			program.Bind();
			program.SendUniformData(static_cast<float>(glfwGetTime()), "uTime");
			glDrawArrays(GL_TRIANGLES, 0, 3);
			Program::Unbind();
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
		ImGui::Begin("Black Hole");
		ImGui::Text("%.1f FPS (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
		ImGui::Text("R: reload shaders   Esc: quit");
		if (!program.GetLog().empty()) {
			ImGui::Separator();
			ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Shader error (showing last good shader):");
			ImGui::TextWrapped("%s", program.GetLog().c_str());
		}
		ImGui::End();
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		glfwSwapBuffers(window);

		if (maxFrames > 0 && ++frame >= maxFrames) {
			break;
		}
	}

	glDeleteVertexArrays(1, &vao);
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
