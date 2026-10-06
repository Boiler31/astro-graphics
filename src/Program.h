#pragma once
#include <GL/glew.h>
#include <string>
#include <glm/glm.hpp>

// A vertex + fragment shader pair, loaded from files with `#include "x.glsl"` support
// (resolved relative to the shader directory). Load() can be called again at any time to
// hot-reload; if the new source fails to compile, the previous working program is kept.
class Program
{
public:
	Program() = default;
	~Program();
	Program(const Program&) = delete;
	Program& operator=(const Program&) = delete;

	void SetShaderFiles(const std::string& vertName, const std::string& fragName);

	// (Re)build from the files. Returns true on success. On failure the old program (if any)
	// stays active and GetLog() holds the compiler/linker output.
	bool Load();

	void Bind() const { glUseProgram(programID); }
	static void Unbind() { glUseProgram(0); }
	GLuint GetPID() const { return programID; }
	const std::string& GetLog() const { return log; }

	void SendUniformData(int v, const char* name) const;
	void SendUniformData(float v, const char* name) const;
	void SendUniformData(const glm::vec2& v, const char* name) const;
	void SendUniformData(const glm::vec3& v, const char* name) const;
	void SendUniformData(const glm::mat4& m, const char* name) const;

private:
	std::string ReadShader(const std::string& name, int depth, int& fileCounter);
	GLuint Compile(GLenum type, const std::string& file, std::string& errors);

	GLuint programID = 0;
	std::string vertName, fragName;
	std::string log;
};
