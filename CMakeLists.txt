CMAKE_MINIMUM_REQUIRED(VERSION 3.5)

# Name of the project
PROJECT(BlackHole)

# Sources: ours + vendored Dear ImGui (core + GLFW/OpenGL3 backends).
FILE(GLOB_RECURSE SOURCES "src/*.cpp")
FILE(GLOB_RECURSE HEADERS "src/*.h")
FILE(GLOB_RECURSE GLSL "shaders/*.glsl" "shaders/*.vert" "shaders/*.frag")
SET(IMGUI_DIR "${CMAKE_SOURCE_DIR}/third_party/imgui")
SET(IMGUI_SOURCES
	${IMGUI_DIR}/imgui.cpp
	${IMGUI_DIR}/imgui_draw.cpp
	${IMGUI_DIR}/imgui_tables.cpp
	${IMGUI_DIR}/imgui_widgets.cpp
	${IMGUI_DIR}/imgui_demo.cpp
	${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
	${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)

# Set the executable.
ADD_EXECUTABLE(${CMAKE_PROJECT_NAME} ${SOURCES} ${HEADERS} ${GLSL} ${IMGUI_SOURCES})
TARGET_INCLUDE_DIRECTORIES(${CMAKE_PROJECT_NAME} PRIVATE
	${IMGUI_DIR}
	${IMGUI_DIR}/backends
	${CMAKE_SOURCE_DIR}/third_party/stb
)

# Shaders are read straight from the source tree (not copied next to the exe) so that
# editing a .glsl file and pressing R in the app picks up the change immediately.
TARGET_COMPILE_DEFINITIONS(${CMAKE_PROJECT_NAME} PRIVATE
	BH_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders/"
	BH_RESOURCE_DIR="${CMAKE_SOURCE_DIR}/resources/"
	GLEW_STATIC
)

# Setup GLM
SET(GLM_INCLUDE_DIR "$ENV{GLM_INCLUDE_DIR}")
if(NOT GLM_INCLUDE_DIR)
	message(FATAL_ERROR "Please point the environment variable GLM_INCLUDE_DIR to your GLM directory.")
endif()
INCLUDE_DIRECTORIES(${GLM_INCLUDE_DIR})

# Get the GLFW environment variable. There should be a CMakeLists.txt in the
# specified directory.
set(GLFW_DIR "$ENV{GLFW_DIR}")
if(NOT GLFW_DIR)
	message(FATAL_ERROR "Please point the environment variable GLFW_DIR to the root directory of your GLFW3 installation.")
endif()
option(GLFW_BUILD_EXAMPLES "GLFW_BUILD_EXAMPLES" OFF)
option(GLFW_BUILD_TESTS "GLFW_BUILD_TESTS" OFF)
option(GLFW_BUILD_DOCS "GLFW_BUILD_DOCS" OFF)
# Build GLFW inside our own build tree so we never write into the class folder.
add_subdirectory(${GLFW_DIR} ${CMAKE_BINARY_DIR}/glfw)
include_directories(${GLFW_DIR}/include)
target_link_libraries(${CMAKE_PROJECT_NAME} glfw ${GLFW_LIBRARIES})

# Get the GLEW environment variable.
set(GLEW_DIR "$ENV{GLEW_DIR}")
if(NOT GLEW_DIR)
	message(FATAL_ERROR "Please point the environment variable GLEW_DIR to the root directory of your GLEW installation.")
endif()
include_directories(${GLEW_DIR}/include)
IF(WIN32)
	# With prebuilt binaries
	# Check for 32 vs 64 bit generator
	IF(NOT CMAKE_CL_64)
		MESSAGE(STATUS "Using 32Bit")
		TARGET_LINK_LIBRARIES(${CMAKE_PROJECT_NAME} ${GLEW_DIR}/lib/Release/Win32/glew32s.lib)
	ELSE()
		MESSAGE(STATUS "Using 64Bit")
		TARGET_LINK_LIBRARIES(${CMAKE_PROJECT_NAME} ${GLEW_DIR}/lib/Release/x64/glew32s.lib)
	ENDIF()
ELSE()
	TARGET_LINK_LIBRARIES(${CMAKE_PROJECT_NAME} ${GLEW_DIR}/lib/libGLEW.a)
ENDIF()

# Use c++17
SET_TARGET_PROPERTIES(${CMAKE_PROJECT_NAME} PROPERTIES CXX_STANDARD 17)
SET_TARGET_PROPERTIES(${CMAKE_PROJECT_NAME} PROPERTIES LINKER_LANGUAGE CXX)

# OS specific options and libraries
IF(WIN32)
	# -Wall produces way too many warnings.
	# -pedantic is not supported.
	# Disable warning 4996.
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4996")
	TARGET_LINK_LIBRARIES(${CMAKE_PROJECT_NAME} opengl32.lib)
	# Windows Media Foundation: used by VideoWriter for built-in H.264 .mp4 export (no ffmpeg needed).
	TARGET_LINK_LIBRARIES(${CMAKE_PROJECT_NAME} mfplat.lib mfreadwrite.lib mfuuid.lib ole32.lib)
	SET_PROPERTY(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${CMAKE_PROJECT_NAME})
ELSE()
	# Enable all pedantic warnings.
	SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic")
	IF(APPLE)
		# Add required frameworks for GLFW.
		TARGET_LINK_LIBRARIES(${CMAKE_PROJECT_NAME} "-framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo")
	ELSE()
		#Link the Linux OpenGL library
		TARGET_LINK_LIBRARIES(${CMAKE_PROJECT_NAME} "GL")
	ENDIF()
ENDIF()

# ---------------------------------------------------------------------------------------
# Physics validation (no OpenGL): `ctest -C Release` or run build/Release/GeodesicTests.exe
# ---------------------------------------------------------------------------------------
ADD_EXECUTABLE(GeodesicTests tests/geodesic_tests.cpp src/Geodesic.cpp src/Blackbody.cpp src/Observer.cpp)
TARGET_INCLUDE_DIRECTORIES(GeodesicTests PRIVATE ${CMAKE_SOURCE_DIR}/src)
SET_TARGET_PROPERTIES(GeodesicTests PROPERTIES CXX_STANDARD 17)
ENABLE_TESTING()
ADD_TEST(NAME geodesic COMMAND GeodesicTests)

# ---------------------------------------------------------------------------------------
# VideoTool: re-encode / compress an mp4 with Windows' built-in codecs (no ffmpeg needed).
#   build\Release\VideoTool.exe compress renders\in.mp4 renders\out.mp4 --target-mb 50
# ---------------------------------------------------------------------------------------
IF(WIN32)
	ADD_EXECUTABLE(VideoTool tools/video_tool.cpp src/VideoWriter.cpp)
	TARGET_INCLUDE_DIRECTORIES(VideoTool PRIVATE ${CMAKE_SOURCE_DIR}/src)
	SET_TARGET_PROPERTIES(VideoTool PROPERTIES CXX_STANDARD 17)
	TARGET_LINK_LIBRARIES(VideoTool mfplat.lib mfreadwrite.lib mfuuid.lib ole32.lib)
ENDIF()
