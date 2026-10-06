#include "Program.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#ifndef BH_SHADER_DIR
#define BH_SHADER_DIR "shaders/"
#endif

Program::~Program()
{
	if (programID) {
		glDeleteProgram(programID);
	}
}

void Program::SetShaderFiles(const std::string& v, const std::string& f)
{
	vertName = v;
	fragName = f;
}

// Reads a shader file and expands `#include "file"` lines. `#line` directives keep compiler
// error line numbers pointing at the right file (the "source string number" is the file's
// index in load order).
std::string Program::ReadShader(const std::string& name, int depth, int& fileCounter)
{
	if (depth > 8) {
		return "#error include depth exceeded (cyclic #include?)\n";
	}
	std::ifstream ifs(std::string(BH_SHADER_DIR) + name);
	if (!ifs) {
		return "#error cannot open shader file: " + name + "\n";
	}
	const int fileId = fileCounter++;
	std::stringstream out;
	std::string line;
	int lineNo = 0;
	while (std::getline(ifs, line)) {
		lineNo++;
		size_t p = line.find_first_not_of(" \t");
		if (p != std::string::npos && line.compare(p, 8, "#include") == 0) {
			size_t a = line.find('"', p);
			size_t b = (a == std::string::npos) ? a : line.find('"', a + 1);
			if (b != std::string::npos) {
				out << ReadShader(line.substr(a + 1, b - a - 1), depth + 1, fileCounter);
				out << "#line " << (lineNo + 1) << " " << fileId << "\n";
				continue;
			}
		}
		out << line << "\n";
	}
	return out.str();
}

GLuint Program::Compile(GLenum type, const std::string& file, std::string& errors)
{
	int fileCounter = 0;
	std::string src = ReadShader(file, 0, fileCounter);
	const char* text = src.c_str();

	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &text, nullptr);
	glCompileShader(shader);

	GLint ok = GL_FALSE;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		GLint len = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
		std::vector<char> buf(len > 1 ? len : 1);
		glGetShaderInfoLog(shader, len, nullptr, buf.data());
		errors += "[" + file + "]\n" + buf.data() + "\n";
		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

bool Program::Load()
{
	std::string errors;
	GLuint vs = Compile(GL_VERTEX_SHADER, vertName, errors);
	GLuint fs = Compile(GL_FRAGMENT_SHADER, fragName, errors);

	GLuint prog = 0;
	if (vs && fs) {
		prog = glCreateProgram();
		glAttachShader(prog, vs);
		glAttachShader(prog, fs);
		glLinkProgram(prog);
		GLint ok = GL_FALSE;
		glGetProgramiv(prog, GL_LINK_STATUS, &ok);
		if (!ok) {
			GLint len = 0;
			glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
			std::vector<char> buf(len > 1 ? len : 1);
			glGetProgramInfoLog(prog, len, nullptr, buf.data());
			errors += std::string("[link]\n") + buf.data() + "\n";
			glDeleteProgram(prog);
			prog = 0;
		}
	}
	if (vs) glDeleteShader(vs);
	if (fs) glDeleteShader(fs);

	if (!prog) {
		log = errors;
		std::cerr << "Shader build failed (" << vertName << ", " << fragName << "):\n" << errors << std::endl;
		return false;
	}
	if (programID) {
		glDeleteProgram(programID);
	}
	programID = prog;
	log.clear();
	return true;
}

void Program::SendUniformData(int v, const char* name) const
{
	glUniform1i(glGetUniformLocation(programID, name), v);
}

void Program::SendUniformData(float v, const char* name) const
{
	glUniform1f(glGetUniformLocation(programID, name), v);
}

void Program::SendUniformData(const glm::vec2& v, const char* name) const
{
	glUniform2f(glGetUniformLocation(programID, name), v.x, v.y);
}

void Program::SendUniformData(const glm::vec3& v, const char* name) const
{
	glUniform3f(glGetUniformLocation(programID, name), v.x, v.y, v.z);
}

void Program::SendUniformData(const glm::mat4& m, const char* name) const
{
	glUniformMatrix4fv(glGetUniformLocation(programID, name), 1, GL_FALSE, &m[0][0]);
}
