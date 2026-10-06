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
#include "Bloom.h"
#include "Camera.h"
#include "CameraPath.h"
#include "Framebuffer.h"
#include "Geodesic.h"
#include "Observer.h"
#include "Program.h"
#include "Screenshot.h"
#include "Texture.h"

#ifndef BH_RESOURCE_DIR
#define BH_RESOURCE_DIR "resources/"
#endif

static bool g_reloadRequested = false;
static bool g_addKeyframe = false;  // K
static bool g_togglePlay = false;   // Space
static bool g_toggleHud = false;    // H
static bool g_togglePanel = false;  // F1

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
	} else if (key == GLFW_KEY_K) {
		g_addKeyframe = true;
	} else if (key == GLFW_KEY_SPACE) {
		g_togglePlay = true;
	} else if (key == GLFW_KEY_H) {
		g_toggleHud = true;
	} else if (key == GLFW_KEY_F1) {
		g_togglePanel = true;
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
	//   --scale S             internal render scale (0.25..2; >1 supersamples)
	//   --exposure E          exposure multiplier
	//   --bloom B             bloom strength (0 = off)
	//   --skygain G           sky brightness scale
	//   --time T              start the simulation clock at T (units of M) and pause it
	//   --turb X              disk turbulence 0..1
	//   --tpeak K             disk peak temperature in Kelvin
	//   --bright X            disk brightness scale
	//   --beamexp N           beaming exponent (4 = physical)
	//   --observer N          0 static, 1 free fall, 2 circular orbit, 3 along view
	//   --boost B             observer boost 0..1 (fraction of the model's speed)
	//   --tour                load the built-in camera tour
	//   --path FILE           load a camera path file
	//   --pathtime T          put the camera at time T on the path (and pause there)
	//   --savepath FILE       write the loaded path (e.g. with --tour) to FILE and exit
	//   --hud                 show the HUD (also in screenshots)
	//   --panel               show the control panel in screenshots
	std::string fxLetters = "gtdcb";
	int startObserver = 0;
	float startBoost = 0.f;
	bool startTour = false, startHud = false, startPanel = false;
	std::string startPathFile, savePathFile;
	float startPathTime = -1.f;
	bool startChecker = false;
	float startTPeak = -1.f, startBright = -1.f, startBeamExp = -1.f, startTurb = -1.f;
	float startTime = 0.f;
	bool timeGiven = false;
	float startScale = 1.f, startExposure = 1.f, startBloom = 0.07f, startSkyGain = 0.3f;
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
		} else if (std::strcmp(argv[i], "--observer") == 0 && i + 1 < argc) {
			startObserver = std::atoi(argv[++i]);
		} else if (std::strcmp(argv[i], "--boost") == 0 && i + 1 < argc) {
			startBoost = static_cast<float>(std::atof(argv[++i]));
		} else if (std::strcmp(argv[i], "--tour") == 0) {
			startTour = true;
		} else if (std::strcmp(argv[i], "--path") == 0 && i + 1 < argc) {
			startPathFile = argv[++i];
		} else if (std::strcmp(argv[i], "--savepath") == 0 && i + 1 < argc) {
			savePathFile = argv[++i];
		} else if (std::strcmp(argv[i], "--pathtime") == 0 && i + 1 < argc) {
			startPathTime = static_cast<float>(std::atof(argv[++i]));
		} else if (std::strcmp(argv[i], "--hud") == 0) {
			startHud = true;
		} else if (std::strcmp(argv[i], "--panel") == 0) {
			startPanel = true;
		} else if (std::strcmp(argv[i], "--bright") == 0 && i + 1 < argc) {
			startBright = static_cast<float>(std::atof(argv[++i]));
		} else if (std::strcmp(argv[i], "--beamexp") == 0 && i + 1 < argc) {
			startBeamExp = static_cast<float>(std::atof(argv[++i]));
		} else if (std::strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
			startScale = static_cast<float>(std::atof(argv[++i]));
		} else if (std::strcmp(argv[i], "--exposure") == 0 && i + 1 < argc) {
			startExposure = static_cast<float>(std::atof(argv[++i]));
		} else if (std::strcmp(argv[i], "--bloom") == 0 && i + 1 < argc) {
			startBloom = static_cast<float>(std::atof(argv[++i]));
		} else if (std::strcmp(argv[i], "--skygain") == 0 && i + 1 < argc) {
			startSkyGain = static_cast<float>(std::atof(argv[++i]));
		} else if (std::strcmp(argv[i], "--time") == 0 && i + 1 < argc) {
			startTime = static_cast<float>(std::atof(argv[++i]));
			timeGiven = true;
		} else if (std::strcmp(argv[i], "--turb") == 0 && i + 1 < argc) {
			startTurb = static_cast<float>(std::atof(argv[++i]));
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

	// Post-processing: the scene renders into an HDR target (at an adjustable internal scale),
	// then bloom is computed from it and everything is tonemapped to the screen.
	Program tonemap;
	tonemap.SetShaderFiles("fullscreen.vert", "tonemap.frag");
	tonemap.Load();
	Bloom bloom;
	bloom.Load();
	Framebuffer sceneFbo;
	float renderScale = std::clamp(startScale, 0.25f, 2.0f);
	float exposure = startExposure;
	float bloomStrength = startBloom;

	// Optional equirectangular sky map. Without one we fall back to the procedural star field.
	GLuint skyTex = LoadTexture2D(std::string(BH_RESOURCE_DIR) + "sky/sky.jpg");
	if (!skyTex) skyTex = LoadTexture2D(std::string(BH_RESOURCE_DIR) + "sky/sky.png");
	int skyMode = skyTex ? 1 : 0; // 0 = procedural, 1 = texture
	float skyGain = startSkyGain;
	bool skyBlueshift = true;

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
	float diskIntensity = 1.1f;
	float beamExp = 4.0f;          // 4 = physical beaming (g^4); lower = artistic
	float diskTurbulence = startTurb >= 0.f ? startTurb : 0.7f;
	bool diskReverse = false;      // sense of rotation about +y

	// Simulation clock, in units of M (G = c = 1). The ISCO orbital period is 2 pi 6^1.5 ~ 92 M.
	double simTime = startTime;
	float simSpeed = 20.f;         // M per real second
	bool simRunning = !timeGiven;  // --time pins the clock for reproducible screenshots
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

	// Observer: how the camera moves relative to a hovering one (aberration + Doppler), and a
	// keyframed camera path (Space plays it, K adds a keyframe from the current view).
	ObserverModel observerModel = static_cast<ObserverModel>(std::clamp(startObserver, 0, 3));
	float observerBoost = startBoost; // 0 = static, 1 = full model speed
	float viewSpeed = 0.8f;           // speed for the "along view" model
	CameraPath path;
	bool pathPlaying = false, pathLoop = false;
	float pathTime = 0.f;
	int selectedKey = -1;
	char pathFile[256] = "resources/paths/tour.txt";
	bool showHud = startHud;
	bool showPanel = screenshotPath.empty() || startPanel;
	float bhMassSolar = 10.f; // only used to translate M into kilometres on the HUD

	auto applyKeyframe = [&](const Keyframe& k) {
		camera.position = k.position;
		camera.SetOrientation(k.yaw, k.pitch);
		camera.fovDeg = k.fov;
		observerBoost = k.boost;
	};
	auto loadTour = [&]() {
		path = CameraPath::DefaultTour();
		observerModel = ObserverModel::FreeFall; // the tour ends with a plunge
		pathTime = 0.f;
		selectedKey = -1;
	};
	if (!startPathFile.empty()) {
		int model = -1;
		if (path.Load(startPathFile, &model)) {
			if (model >= 0 && model <= 3) observerModel = static_cast<ObserverModel>(model);
		} else {
			std::cerr << "Could not load camera path " << startPathFile << std::endl;
		}
	} else if (startTour) {
		loadTour();
		if (startObserver != 0) observerModel = static_cast<ObserverModel>(std::clamp(startObserver, 0, 3));
	}
	if (startPathTime >= 0.f && !path.keys.empty()) {
		pathTime = std::clamp(startPathTime, 0.f, path.Duration());
		applyKeyframe(path.Sample(pathTime));
		simTime = pathTime * simSpeed; // keep the disk animation consistent with the path time
		simRunning = false;
	}

	if (!savePathFile.empty()) {
		bool saved = path.Save(savePathFile, static_cast<int>(observerModel));
		std::cout << (saved ? "Saved " : "Failed to save ") << savePathFile << std::endl;
		glfwTerminate();
		return saved ? 0 : 1;
	}

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
		if (simRunning) {
			simTime += dt * simSpeed;
		}

		if (g_reloadRequested) {
			g_reloadRequested = false;
			std::cout << "Reloading shaders..." << std::endl;
			bool ok = program.Load();
			ok = tonemap.Load() && ok;
			ok = bloom.Load() && ok;
			if (ok) {
				std::cout << "Shaders reloaded." << std::endl;
			}
		}

		// Key shortcuts handled here so they run in the main loop.
		if (g_togglePlay) {
			g_togglePlay = false;
			if (!path.keys.empty()) {
				pathPlaying = !pathPlaying;
				if (pathPlaying && pathTime >= path.Duration()) pathTime = 0.f;
			}
		}
		if (g_toggleHud) {
			g_toggleHud = false;
			showHud = !showHud;
		}
		if (g_togglePanel) {
			g_togglePanel = false;
			showPanel = !showPanel;
		}
		if (g_addKeyframe) {
			g_addKeyframe = false;
			Keyframe k;
			k.time = path.keys.empty() ? 0.f : path.Duration() + 5.f;
			k.position = camera.position;
			k.yaw = camera.YawDeg();
			k.pitch = camera.PitchDeg();
			k.fov = camera.fovDeg;
			k.boost = observerBoost;
			selectedKey = path.Add(k);
		}

		// Camera path playback drives the camera (and the disk clock, so scrubbing is repeatable).
		if (pathPlaying && !path.keys.empty()) {
			pathTime += dt;
			if (pathTime >= path.Duration()) {
				if (pathLoop) {
					pathTime = std::fmod(pathTime, std::max(path.Duration(), 0.01f));
				} else {
					pathTime = path.Duration();
					pathPlaying = false;
				}
			}
			applyKeyframe(path.Sample(pathTime));
			simTime = pathTime * simSpeed;
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
			if (!pathPlaying) {
				camera.Rotate(static_cast<float>(x - lastX), static_cast<float>(y - lastY));
			}
			lastX = x;
			lastY = y;
		}
		if (!io.WantCaptureKeyboard && !pathPlaying) {
			camera.Update(window, dt);
		}
		if (lensing) {
			float r = glm::length(camera.position);
			if (r < kMinCameraRadius) {
				camera.position = (r > 1e-6f ? camera.position / r : glm::vec3(0.f, 0.f, 1.f)) * kMinCameraRadius;
			}
		}

		// Camera velocity relative to a hovering observer at this spot (zero for the static model).
		const glm::vec3 camBeta =
		    ObserverVelocity(observerModel, camera.position, camera.Forward(), observerBoost, viewSpeed);

		int fbw, fbh;
		glfwGetFramebufferSize(window, &fbw, &fbh);
		if (fbw == 0 || fbh == 0) {
			continue; // minimized
		}

		// 1. Scene -> HDR target at the internal resolution.
		const int sceneW = std::max(1, static_cast<int>(fbw * renderScale + 0.5f));
		const int sceneH = std::max(1, static_cast<int>(fbh * renderScale + 0.5f));
		sceneFbo.Resize(sceneW, sceneH);
		sceneFbo.Bind();
		glClearColor(0.f, 0.f, 0.f, 1.f);
		glClear(GL_COLOR_BUFFER_BIT);

		if (program.GetPID() != 0) {
			program.Bind();
			program.SendUniformData(glm::vec2(static_cast<float>(sceneW), static_cast<float>(sceneH)), "uResolution");
			program.SendUniformData(camera.position, "uCamPos");
			program.SendUniformData(camera.Right(), "uCamRight");
			program.SendUniformData(camera.Up(), "uCamUp");
			program.SendUniformData(camera.Forward(), "uCamForward");
			program.SendUniformData(camera.TanHalfFov(), "uTanHalfFov");
			program.SendUniformData(camBeta, "uCamBeta");
			program.SendUniformData(skyMode, "uSkyMode");
			program.SendUniformData(skyGain, "uSkyGain");
			program.SendUniformData(skyBlueshift ? 1 : 0, "uSkyBlueshift");
			program.SendUniformData(lensing ? 1 : 0, "uGR");
			program.SendUniformData(maxSteps, "uMaxSteps");
			program.SendUniformData(stepScale, "uStepScale");
			program.SendUniformData(diskEnabled ? 1 : 0, "uDisk");
			program.SendUniformData(diskOuter, "uDiskOuter");
			program.SendUniformData(diskOpacity, "uDiskOpacity");
			program.SendUniformData(diskTPeak, "uDiskTPeak");
			program.SendUniformData(diskIntensity, "uDiskIntensity");
			program.SendUniformData(diskReverse ? -1.f : 1.f, "uDiskRotation");
			program.SendUniformData(static_cast<float>(simTime), "uSimTime");
			program.SendUniformData(diskTurbulence, "uDiskTurbulence");
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

		// 2. Bloom from the HDR scene.
		GLuint bloomTex = sceneFbo.Texture();
		if (bloomStrength > 0.f) {
			bloomTex = bloom.Run(sceneFbo.Texture(), sceneW, sceneH);
		}

		// 3. Tonemap (+ bloom) to the screen. The linear filter on the scene texture resamples
		// it when the internal scale is not 1.
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, fbw, fbh);
		if (tonemap.GetPID() != 0) {
			tonemap.Bind();
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, sceneFbo.Texture());
			tonemap.SendUniformData(0, "uScene");
			glActiveTexture(GL_TEXTURE1);
			glBindTexture(GL_TEXTURE_2D, bloomTex);
			tonemap.SendUniformData(1, "uBloom");
			tonemap.SendUniformData(exposure, "uExposure");
			tonemap.SendUniformData(bloomStrength, "uBloomStrength");
			glDrawArrays(GL_TRIANGLES, 0, 3);
			Program::Unbind();
		}

		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplGlfw_NewFrame();
		ImGui::NewFrame();
		if (showPanel) {
		ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowSize(ImVec2(330, 0), ImGuiCond_FirstUseEver);
		ImGui::Begin("Black Hole");
		ImGui::Text("%.1f FPS (%.2f ms)", io.Framerate, 1000.0f / io.Framerate);
		ImGui::TextDisabled("WASD/QE move, Shift fast, RMB-drag look");
		ImGui::TextDisabled("R reload shaders, Space play path, K add keyframe");
		ImGui::TextDisabled("H HUD, F1 hide this panel, Esc quit");

		if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Text("pos  %.1f %.1f %.1f", camera.position.x, camera.position.y, camera.position.z);
			ImGui::Text("r = %.1f M   yaw %.0f  pitch %.0f", glm::length(camera.position), camera.YawDeg(), camera.PitchDeg());
			ImGui::SliderFloat("FOV", &camera.fovDeg, 20.f, 120.f, "%.0f deg");
			ImGui::SliderFloat("Speed", &camera.baseSpeed, 1.f, 100.f, "%.0f", ImGuiSliderFlags_Logarithmic);
			if (ImGui::Button("Reset camera")) {
				camera.Reset();
			}
		}
		if (ImGui::CollapsingHeader("Observer", ImGuiTreeNodeFlags_DefaultOpen)) {
			int model = static_cast<int>(observerModel);
			if (ImGui::BeginCombo("Motion", ObserverModelName(observerModel))) {
				for (int m = 0; m < 4; m++) {
					if (ImGui::Selectable(ObserverModelName(static_cast<ObserverModel>(m)), model == m)) {
						observerModel = static_cast<ObserverModel>(m);
					}
				}
				ImGui::EndCombo();
			}
			ImGui::SliderFloat("Boost", &observerBoost, 0.f, 1.f, "%.2f");
			if (observerModel == ObserverModel::AlongView) {
				ImGui::SliderFloat("Speed (c)", &viewSpeed, 0.f, 0.99f, "%.2f");
			}
			ImGui::Text("v = %.3f c (vs. a hovering observer)", glm::length(camBeta));
			ImGui::TextDisabled("Moving = aberration (sky bunches ahead) + Doppler");
		}
		if (ImGui::CollapsingHeader("Camera path", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (ImGui::Button(pathPlaying ? "Pause" : "Play")) {
				g_togglePlay = true;
			}
			ImGui::SameLine();
			ImGui::Checkbox("Loop", &pathLoop);
			ImGui::SameLine();
			if (ImGui::Button("Add key (K)")) {
				g_addKeyframe = true;
			}
			if (!path.keys.empty()) {
				float dur = path.Duration();
				if (ImGui::SliderFloat("Time (s)", &pathTime, 0.f, dur, "%.1f")) {
					applyKeyframe(path.Sample(pathTime));
					simTime = pathTime * simSpeed;
				}
			}
			ImGui::Text("%d keyframes, %.0f s", static_cast<int>(path.keys.size()), path.Duration());
			ImGui::BeginChild("keys", ImVec2(0, 110), true);
			for (int i = 0; i < static_cast<int>(path.keys.size()); i++) {
				const Keyframe& k = path.keys[i];
				char label[96];
				std::snprintf(label, sizeof(label), "%5.1fs  r=%.1f  fov %.0f  boost %.1f", k.time, glm::length(k.position),
				              k.fov, k.boost);
				if (ImGui::Selectable(label, selectedKey == i)) {
					selectedKey = i;
					pathTime = k.time;
					applyKeyframe(k);
					simTime = pathTime * simSpeed;
				}
			}
			ImGui::EndChild();
			if (selectedKey >= 0 && selectedKey < static_cast<int>(path.keys.size())) {
				if (ImGui::Button("Update from view")) {
					Keyframe& k = path.keys[selectedKey];
					k.position = camera.position;
					k.yaw = camera.YawDeg();
					k.pitch = camera.PitchDeg();
					k.fov = camera.fovDeg;
					k.boost = observerBoost;
					path.Normalize();
				}
				ImGui::SameLine();
				if (ImGui::Button("Delete")) {
					path.Remove(selectedKey);
					selectedKey = -1;
				}
				ImGui::SameLine();
				if (ImGui::Button("Time = now")) {
					path.keys[selectedKey].time = pathTime;
					path.Normalize();
				}
			}
			if (ImGui::Button("Load tour")) {
				loadTour();
			}
			ImGui::SameLine();
			if (ImGui::Button("Clear")) {
				path.Clear();
				pathPlaying = false;
				selectedKey = -1;
			}
			ImGui::InputText("File", pathFile, sizeof(pathFile));
			if (ImGui::Button("Save")) {
				path.Save(pathFile, static_cast<int>(observerModel));
			}
			ImGui::SameLine();
			if (ImGui::Button("Load")) {
				int model = -1;
				if (path.Load(pathFile, &model) && model >= 0 && model <= 3) {
					observerModel = static_cast<ObserverModel>(model);
				}
				selectedKey = -1;
			}
		}
		if (ImGui::CollapsingHeader("HUD", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::Checkbox("Show HUD (H)", &showHud);
			ImGui::SliderFloat("Black hole mass (Msun)", &bhMassSolar, 1.f, 1.0e7f, "%.0f", ImGuiSliderFlags_Logarithmic);
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
			ImGui::SliderFloat("Turbulence", &diskTurbulence, 0.f, 1.f, "%.2f");
			ImGui::Checkbox("Reverse rotation", &diskReverse);
			ImGui::SeparatorText("Time");
			ImGui::Checkbox("Run", &simRunning);
			ImGui::SameLine();
			if (ImGui::Button("Reset time")) {
				simTime = 0.0;
			}
			ImGui::SliderFloat("Speed (M/s)", &simSpeed, 0.f, 200.f, "%.0f", ImGuiSliderFlags_Logarithmic);
			// Geometric units -> seconds: 1 M = G M_sun / c^3 = 4.925 microseconds per solar mass.
			const double iscoPeriod = 2.0 * 3.14159265358979 * std::pow(6.0, 1.5);
			ImGui::Text("t = %.0f M     ISCO orbit = %.0f M", simTime, iscoPeriod);
			ImGui::TextDisabled("= %.1f ms for 10 Msun, %.0f min for Sgr A*", iscoPeriod * 4.925e-6 * 10.0 * 1e3,
			                    iscoPeriod * 4.925e-6 * 4.3e6 / 60.0);
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
			ImGui::SliderFloat("Sky brightness", &skyGain, 0.f, 3.f, "%.2f");
			ImGui::Checkbox("Blueshift sky near the hole", &skyBlueshift);
		}
		if (ImGui::CollapsingHeader("Post-processing", ImGuiTreeNodeFlags_DefaultOpen)) {
			ImGui::SliderFloat("Exposure", &exposure, 0.1f, 8.f, "%.2f", ImGuiSliderFlags_Logarithmic);
			ImGui::SliderFloat("Bloom", &bloomStrength, 0.f, 1.f, "%.2f");
			ImGui::SliderFloat("Render scale", &renderScale, 0.25f, 2.f, "%.2f");
			ImGui::TextDisabled("%dx%d internal (>1 supersamples)", sceneW, sceneH);
		}
		std::string shaderLog = program.GetLog() + tonemap.GetLog() + bloom.GetLog();
		if (!shaderLog.empty()) {
			ImGui::Separator();
			ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "Shader error (showing last good shader):");
			ImGui::TextWrapped("%s", shaderLog.c_str());
		}
		ImGui::End();
		} // showPanel

		// HUD: where the camera is and how its clock runs relative to a distant observer.
		if (showHud) {
			const double r = glm::length(camera.position);
			const double beta = glm::length(camBeta);
			const double rate = ClockRate(r, beta);
			const double km = r * 1.4766 * bhMassSolar; // GM/c^2 = 1.4766 km per solar mass
			char dist[64];
			if (km >= 1.0e6) {
				std::snprintf(dist, sizeof(dist), "%.2f million km", km / 1.0e6);
			} else {
				std::snprintf(dist, sizeof(dist), "%.0f km", km);
			}
			ImGui::SetNextWindowPos(ImVec2(24.f, io.DisplaySize.y - 24.f), ImGuiCond_Always, ImVec2(0.f, 1.f));
			ImGui::SetNextWindowBgAlpha(0.35f);
			ImGui::Begin("##hud", nullptr,
			             ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings |
			                 ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs);
			ImGui::SetWindowFontScale(1.4f);
			ImGui::Text("distance   %.1f M   (%s from a %.0f Msun hole)", r, dist, bhMassSolar);
			ImGui::Text("clock rate %.3f   1 hour here = %.2f hours far away", rate, rate > 1e-6 ? 1.0 / rate : 0.0);
			if (beta > 0.001) {
				ImGui::Text("speed      %.2f c", beta);
			}
			ImGui::End();
		}

		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// Screenshot mode: grab the frame (scene + whatever GUI is enabled) after a few warm-up frames.
		if (!screenshotPath.empty() && ++frame >= 3) {
			SaveScreenshot(screenshotPath, fbw, fbh);
			break;
		}

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
