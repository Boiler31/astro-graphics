#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "Blackbody.h"
#include "Camera.h"
#include "Geodesic.h"
#include "Program.h"
#include "Screenshot.h"
#include "Texture.h"

#ifndef BH_RESOURCE_DIR
#define BH_RESOURCE_DIR "resources/"
#endif

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
	Camera camera;

	// Command line (all optional, mostly for automated testing):
	//   --frames N            exit after N frames
	//   --screenshot FILE     render a few frames, save FILE (no GUI), exit
	//   --cam X Y Z YAW PITCH start camera pose (degrees)
	//   --nogr                start with light bending off (straight rays)
	//   --novsync             disable vsync (use with --frames to benchmark)
	//   --size W H            window size (default 1280 720)
	//   --fx LETTERS          enabled disk effects: g gravity, t time dilation, d Doppler,
	//                         c color shift, b beaming (default "gtdcb"; "-" for none)
	//   --checker             start with the debug checkerboard disk
	//   --tpeak K             disk peak temperature in Kelvin
	//   --bright X            disk brightness scale
	//   --beamexp N           beaming exponent (4 = physical)
	std::string fxLetters = "gtdcb";
	bool startChecker = false;
	float startTPeak = -1.f, startBright = -1.f, startBeamExp = -1.f;
	int maxFrames = -1;
	bool startWithoutGR = false;
	bool vsync = true;
	int winW = 1280, winH = 720;
	std::string screenshotPath;
	for (int i = 1; i < argc; i++) {
		if (std::strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
			maxFrames = std::atoi(argv[++i]);
		} else if (std::strcmp(argv[i], "--nogr") == 0) {
			startWithoutGR = true;
		} else if (std::strcmp(argv[i], "--novsync") == 0) {
			vsync = false;
		} else if (std::strcmp(argv[i], "--fx") == 0 && i + 1 < argc) {
			fxLetters = argv[++i];
		} else if (std::strcmp(argv[i], "--checker") == 0) {
			startChecker = true;
		} else if (std::strcmp(argv[i], "--bright") == 0 && i + 1 < argc) {
			startBright = static_cast<float>(std::atof(argv[++i]));
		} else if (std::strcmp(argv[i], "--beamexp") == 0 && i + 1 < argc) {
			startBeamExp = static_cast<float>(std::atof(argv[++i]));
		} else if (std::strcmp(argv[i], "--tpeak") == 0 && i + 1 < argc) {
			startTPeak = static_cast<float>(std::atof(argv[++i]));
		} else if (std::strcmp(argv[i], "--size") == 0 && i + 2 < argc) {
			winW = std::atoi(argv[i + 1]);
			winH = std::atoi(argv[i + 2]);
			i += 2;
		} else if (std::strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
			screenshotPath = argv[++i];
		} else if (std::strcmp(argv[i], "--cam") == 0 && i + 5 < argc) {
			camera.position = glm::vec3(std::atof(argv[i + 1]), std::atof(argv[i + 2]), std::atof(argv[i + 3]));
			camera.SetOrientation(static_cast<float>(std::atof(argv[i + 4])), static_cast<float>(std::atof(argv[i + 5])));
			i += 5;
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

	GLFWwindow* window = glfwCreateWindow(winW, winH, "Black Hole", nullptr, nullptr);
	if (!window) {
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(vsync ? 1 : 0);
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

	// Optional equirectangular sky map. Without one we fall back to the procedural star field.
	GLuint skyTex = LoadTexture2D(std::string(BH_RESOURCE_DIR) + "sky/sky.jpg");
	if (!skyTex) skyTex = LoadTexture2D(std::string(BH_RESOURCE_DIR) + "sky/sky.png");
	int skyMode = skyTex ? 1 : 0; // 0 = procedural, 1 = texture
	float exposure = 1.0f;

	// Light bending (see shaders/geodesic.glsl). 0.25 keeps angular momentum drift ~1e-6.
	bool lensing = !startWithoutGR;
	int maxSteps = 400;
	float stepScale = 0.25f;
	const float kMinCameraRadius = 2.1f; // keep the static observer outside the horizon

	// Accretion disk (thin, in the y = 0 plane; inner edge fixed at the ISCO, 6M).
	bool diskEnabled = true;
	float diskOuter = 25.f;
	float diskOpacity = 0.85f;
	float diskTPeak = 5500.f;      // Kelvin at the hottest ring (artistic: real ones are ~1e7 K)
	float diskIntensity = 3.0f;
	float beamExp = 4.0f;          // 4 = physical beaming (g^4); lower = artistic
	bool diskReverse = false;      // sense of rotation about +y
	bool diskDebug = startChecker; // checkerboard instead of physical shading
	if (startTPeak > 0.f) diskTPeak = startTPeak;
	if (startBright > 0.f) diskIntensity = startBright;
	if (startBeamExp >= 0.f) beamExp = startBeamExp;
	auto fxOn = [&](char c) { return fxLetters.find(c) != std::string::npos; };
	bool fxGravity = fxOn('g'), fxTimeDilation = fxOn('t'), fxDoppler = fxOn('d'), fxColorShift = fxOn('c'),
	     fxBeaming = fxOn('b');

	// Blackbody chromaticity table (ln(T)-spaced), sampled by the disk shader on texture unit 1.
	const double kLutTMin = 1000.0, kLutTMax = 40000.0;
	GLuint blackbodyLut = CreateColorLut1D(blackbody::BuildLut(1024, kLutTMin, kLutTMax));

	// Core profile needs a VAO bound even when the vertex shader reads no attributes.
	GLuint vao = 0;
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	bool mouseLook = false;
	double lastX = 0.0, lastY = 0.0;
	double lastTime = glfwGetTime();
	const double loopStart = lastTime;
	int frame = 0;

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		double now = glfwGetTime();
		float dt = static_cast<float>(std::min(now - lastTime, 0.1));
		lastTime = now;

		ImGuiIO& io = ImGui::GetIO();

		if (g_reloadRequested) {
			g_reloadRequested = false;
			std::cout << "Reloading shaders..." << std::endl;
			if (program.Load()) {
				std::cout << "Shaders reloaded." << std::endl;
			}
		}

		// Hold the right mouse button to look around (cursor is hidden/locked while held).
		bool rmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
		if (rmb && !mouseLook && !io.WantCaptureMouse) {
			mouseLook = true;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			if (glfwRawMouseMotionSupported()) {
				glfwSetInputMode(window, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
			}
			glfwGetCursorPos(window, &lastX, &lastY);
		} else if (!rmb && mouseLook) {
			mouseLook = false;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
		if (mouseLook) {
			double x, y;
			glfwGetCursorPos(window, &x, &y);
			camera.Rotate(static_cast<float>(x - lastX), static_cast<float>(y - lastY));
			lastX = x;
			lastY = y;
		}
		if (!io.WantCaptureKeyboard) {
			camera.Update(window, dt);
		}
		if (lensing) {
			float r = glm::length(camera.position);
			if (r < kMinCameraRadius) {
				camera.position = (r > 1e-6f ? camera.position / r : glm::vec3(0.f, 0.f, 1.f)) * kMinCameraRadius;
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
			program.SendUniformData(glm::vec2(static_cast<float>(fbw), static_cast<float>(fbh)), "uResolution");
			program.SendUniformData(camera.position, "uCamPos");
			program.SendUniformData(camera.Right(), "uCamRight");
			program.SendUniformData(camera.Up(), "uCamUp");
			program.SendUniformData(camera.Forward(), "uCamForward");
			program.SendUniformData(camera.TanHalfFov(), "uTanHalfFov");
			program.SendUniformData(skyMode, "uSkyMode");
			program.SendUniformData(exposure, "uExposure");
			program.SendUniformData(lensing ? 1 : 0, "uGR");
			program.SendUniformData(maxSteps, "uMaxSteps");
			program.SendUniformData(stepScale, "uStepScale");
			program.SendUniformData(diskEnabled ? 1 : 0, "uDisk");
			program.SendUniformData(diskOuter, "uDiskOuter");
			program.SendUniformData(diskOpacity, "uDiskOpacity");
			program.SendUniformData(diskTPeak, "uDiskTPeak");
			program.SendUniformData(diskIntensity, "uDiskIntensity");
			program.SendUniformData(diskReverse ? -1.f : 1.f, "uDiskRotation");
			program.SendUniformData(diskDebug ? 1 : 0, "uDiskDebug");
			program.SendUniformData(fxGravity ? 1 : 0, "uFxGravity");
			program.SendUniformData(fxTimeDilation ? 1 : 0, "uFxTimeDilation");
			program.SendUniformData(fxDoppler ? 1 : 0, "uFxDoppler");
			program.SendUniformData(fxColorShift ? 1 : 0, "uFxColorShift");
			program.SendUniformData(fxBeaming ? 1 : 0, "uFxBeaming");
			program.SendUniformData(beamExp, "uBeamExp");
			program.SendUniformData(glm::vec2(static_cast<float>(kLutTMin), static_cast<float>(kLutTMax)), "uLutTRange");
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_1D, blackbodyLut);
			program.SendUniformData(1, "uBlackbody");
			if (skyTex) {
				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, skyTex);
				program.SendUniformData(0, "uSky");
			}
			glDrawArrays(GL_TRIANGLES, 0, 3);
			Program::Unbind();
		}

		// Screenshot mode: grab the scene (without the GUI) after a couple of warm-up frames.
		if (!screenshotPath.empty() && ++frame >= 3) {
			SaveScreenshot(screenshotPath, fbw, fbh);
			break;
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(330, 0), ImGuiCond_FirstUseEver);
		ImGui::Begin("Black Hole");
		ImGui::Text("%.1f FPS (%.2f ms)", io.Framerate, 1000.0f / io.Framerate);
		ImGui::TextDisabled("WASD/QE move, Shift fast, RMB-drag look");
		ImGui::TextDisabled("R reload shaders, Esc quit");

		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("pos  %.1f %.1f %.1f", camera.position.x, camera.position.y, camera.position.z);
			ImGui::Text("r = %.1f M   yaw %.0f  pitch %.0f", glm::length(camera.position), camera.YawDeg(), camera.PitchDeg());
			ImGui::SliderFloat("FOV", &camera.fovDeg, 20.f, 120.f, "%.0f deg");
			ImGui::SliderFloat("Speed", &camera.baseSpeed, 1.f, 100.f, "%.0f", ImGuiSliderFlags_Logarithmic);
			if (ImGui::Button("Reset camera")) {
				camera.Reset();
			}
		}
		if (ImGui::CollapsingHeader("Light bending", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("General relativity (lensing)", &lensing);
			ImGui::SliderInt("Max steps", &maxSteps, 50, 1500);
			ImGui::SliderFloat("Step scale", &stepScale, 0.05f, 0.6f, "%.2f");
			if (lensing) {
				double rCam = glm::length(camera.position);
				double alpha = geo::ShadowAngularRadius(rCam);
				double pixels = std::tan(alpha) / camera.TanHalfFov() * (0.5 * fbh);
				ImGui::Text("Shadow radius: %.2f deg (%.0f px)", glm::degrees(alpha), pixels);
				ImGui::TextDisabled("sin(a) = 3*sqrt(3)*M/r * sqrt(1-2M/r)");
			}
		}
		if (ImGui::CollapsingHeader("Accretion disk", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("Show disk", &diskEnabled);
			ImGui::SliderFloat("Outer radius (M)", &diskOuter, 8.f, 60.f, "%.0f");
			ImGui::SliderFloat("Opacity", &diskOpacity, 0.05f, 1.f, "%.2f");
			ImGui::TextDisabled("inner edge = ISCO = 6M (needs lensing on)");
			ImGui::Checkbox("Debug checkerboard", &diskDebug);
			ImGui::SliderFloat("Peak temperature (K)", &diskTPeak, 2000.f, 30000.f, "%.0f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("Brightness", &diskIntensity, 0.1f, 8.f, "%.2f", ImGuiSliderFlags_Logarithmic);
			ImGui::Checkbox("Reverse rotation", &diskReverse);
			ImGui::SeparatorText("Relativistic effects");
			ImGui::TextDisabled("The first three change the frequency shift g;");
			ImGui::TextDisabled("color shift and beaming decide how g is shown.");
			ImGui::Checkbox("Doppler shift (motion)", &fxDoppler);
			ImGui::Checkbox("Transverse Doppler (time dilation)", &fxTimeDilation);
			ImGui::Checkbox("Gravitational redshift", &fxGravity);
			ImGui::Checkbox("Color shift (T_obs = g T)", &fxColorShift);
			ImGui::Checkbox("Beaming (brightness x g^n)", &fxBeaming);
			ImGui::SliderFloat("Beaming strength n", &beamExp, 0.f, 4.f, "%.1f");
			ImGui::TextDisabled("n = 4 is physical; less is an artistic softening");
		}
		if (ImGui::CollapsingHeader("Sky", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::RadioButton("Procedural stars", &skyMode, 0);
			if (!skyTex) ImGui::BeginDisabled();
			ImGui::SameLine();
			ImGui::RadioButton("Texture", &skyMode, 1);
			if (!skyTex) {
				ImGui::EndDisabled();
				ImGui::TextDisabled("(add resources/sky/sky.jpg for a texture)");
			}
			ImGui::SliderFloat("Exposure", &exposure, 0.1f, 4.f, "%.2f", ImGuiSliderFlags_Logarithmic);
		}
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
			glFinish();
			double secs = glfwGetTime() - loopStart;
			std::cout << "Rendered " << frame << " frames in " << secs << " s ("
			          << 1000.0 * secs / frame << " ms/frame, " << frame / secs << " fps)" << std::endl;
			break;
		}
	}

	glDeleteVertexArrays(1, &vao);
	if (skyTex) glDeleteTextures(1, &skyTex);
	glDeleteTextures(1, &blackbodyLut);
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
