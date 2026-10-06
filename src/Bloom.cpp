#include "Bloom.h"

#include <algorithm>

bool Bloom::Load()
{
	down.SetShaderFiles("fullscreen.vert", "bloom_down.frag");
	up.SetShaderFiles("fullscreen.vert", "bloom_up.frag");
	bool ok = down.Load();
	ok = up.Load() && ok;
	log = down.GetLog() + up.GetLog();
	return ok;
}

GLuint Bloom::Run(GLuint sceneTexture, int w, int h)
{
	if (down.GetPID() == 0 || up.GetPID() == 0) {
		return sceneTexture; // shaders failed to build; fall back to no bloom
	}
	for (int i = 0; i < kLevels; i++) {
		levels[i].Resize(std::max(1, w >> (i + 1)), std::max(1, h >> (i + 1)));
	}

	glDisable(GL_BLEND);
	down.Bind();
	for (int i = 0; i < kLevels; i++) {
		GLuint srcTex = (i == 0) ? sceneTexture : levels[i - 1].Texture();
		int srcW = (i == 0) ? w : levels[i - 1].Width();
		int srcH = (i == 0) ? h : levels[i - 1].Height();
		levels[i].Bind();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, srcTex);
		down.SendUniformData(0, "uSrc");
		down.SendUniformData(glm::vec2(1.0f / srcW, 1.0f / srcH), "uTexelSize");
		down.SendUniformData(i == 0 ? 1 : 0, "uFirst");
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}

	// Upsample: add level i+1 (tent-filtered) onto level i, which still holds its downsampled data.
	up.Bind();
	glEnable(GL_BLEND);
	glBlendFunc(GL_ONE, GL_ONE);
	for (int i = kLevels - 2; i >= 0; i--) {
		levels[i].Bind();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, levels[i + 1].Texture());
		up.SendUniformData(0, "uSrc");
		up.SendUniformData(glm::vec2(1.0f / levels[i + 1].Width(), 1.0f / levels[i + 1].Height()), "uTexelSize");
		glDrawArrays(GL_TRIANGLES, 0, 3);
	}
	glDisable(GL_BLEND);
	Program::Unbind();
	return levels[0].Texture();
}
