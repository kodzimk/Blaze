#include "PostProcess2D.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Renderer/Shader.h"
#include "glm.hpp"
#include <glad/glad.h>
#include <cmath>

namespace Blaze
{
	static Shader s_Shader;
	static bool s_Ready = false;
	static unsigned int s_Vao = 0;
	static unsigned int s_Vbo = 0;

	static unsigned int s_PingFbo = 0;
	static unsigned int s_PingTex = 0;
	static int s_PingW = 0;
	static int s_PingH = 0;

	static unsigned int s_SceneFbo = 0;
	static unsigned int s_SceneTex = 0;
	static int s_SceneW = 0;
	static int s_SceneH = 0;

	static const char* s_Vert = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;
out vec2 vUV;
void main()
{
	vUV = aUV;
	gl_Position = vec4(aPos, 0.0, 1.0);
}
)";

	static const char* s_Frag = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D u_Texture;
uniform int u_Grade;
uniform float u_Brightness;
uniform float u_Contrast;
uniform float u_Saturation;
uniform float u_Vignette;
uniform vec3 u_Tint;

void main()
{
	vec3 color = texture(u_Texture, vUV).rgb;
	if (u_Grade != 0)
	{
		color += u_Brightness;
		color = (color - 0.5) * u_Contrast + 0.5;
		float luma = dot(color, vec3(0.2126, 0.7152, 0.0722));
		color = mix(vec3(luma), color, u_Saturation);
		color *= u_Tint;
		float d = distance(vUV, vec2(0.5));
		float vig = 1.0 - u_Vignette * smoothstep(0.35, 0.95, d);
		color *= vig;
	}
	FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
)";

	static void InitShaderAndQuad()
	{
		if (s_Ready)
			return;

		s_Shader.CreateProgram(s_Vert, s_Frag);
		s_Shader.LinkProgram();

		const float verts[] = {
			-1.0f, -1.0f, 0.0f, 0.0f,
			 1.0f, -1.0f, 1.0f, 0.0f,
			 1.0f,  1.0f, 1.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, 1.0f, 1.0f,
			-1.0f,  1.0f, 0.0f, 1.0f,
		};

		glGenVertexArrays(1, &s_Vao);
		glGenBuffers(1, &s_Vbo);
		glBindVertexArray(s_Vao);
		glBindBuffer(GL_ARRAY_BUFFER, s_Vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
		glBindVertexArray(0);

		s_Ready = true;
	}

	void PostProcess2D::EnsureResources(int width, int height)
	{
		InitShaderAndQuad();
		if (width <= 0 || height <= 0)
			return;

		if (s_PingFbo == 0 || s_PingW != width || s_PingH != height)
		{
			if (s_PingTex)
				glDeleteTextures(1, &s_PingTex);
			if (s_PingFbo)
				glDeleteFramebuffers(1, &s_PingFbo);

			glGenFramebuffers(1, &s_PingFbo);
			glGenTextures(1, &s_PingTex);
			glBindTexture(GL_TEXTURE_2D, s_PingTex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindFramebuffer(GL_FRAMEBUFFER, s_PingFbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_PingTex, 0);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glBindTexture(GL_TEXTURE_2D, 0);
			s_PingW = width;
			s_PingH = height;
		}
	}

	void PostProcess2D::DrawFullscreen(unsigned int srcTexture, bool grade, GameObject* settings)
	{
		InitShaderAndQuad();
		s_Shader.Use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, srcTexture);
		s_Shader.SetUniform1i(0, "u_Texture");
		s_Shader.SetUniform1i(grade ? 1 : 0, "u_Grade");
		if (grade && settings)
		{
			s_Shader.SetUniform1f(settings->GetPostProcessBrightness(), "u_Brightness");
			s_Shader.SetUniform1f(settings->GetPostProcessContrast(), "u_Contrast");
			s_Shader.SetUniform1f(settings->GetPostProcessSaturation(), "u_Saturation");
			s_Shader.SetUniform1f(settings->GetPostProcessVignette(), "u_Vignette");
			s_Shader.SetUniform3fv(settings->GetPostProcessTint(), "u_Tint");
		}
		else
		{
			s_Shader.SetUniform1f(0.0f, "u_Brightness");
			s_Shader.SetUniform1f(1.0f, "u_Contrast");
			s_Shader.SetUniform1f(1.0f, "u_Saturation");
			s_Shader.SetUniform1f(0.0f, "u_Vignette");
			s_Shader.SetUniform3fv(glm::vec3(1.0f), "u_Tint");
		}

		GLboolean depthWas = glIsEnabled(GL_DEPTH_TEST);
		GLboolean blendWas = glIsEnabled(GL_BLEND);
		GLboolean scissorWas = glIsEnabled(GL_SCISSOR_TEST);
		glDisable(GL_DEPTH_TEST);
		glDisable(GL_BLEND);
		glDisable(GL_SCISSOR_TEST);
		glBindVertexArray(s_Vao);
		glDrawArrays(GL_TRIANGLES, 0, 6);
		glBindVertexArray(0);
		glBindTexture(GL_TEXTURE_2D, 0);
		if (depthWas) glEnable(GL_DEPTH_TEST);
		if (blendWas) glEnable(GL_BLEND);
		if (scissorWas) glEnable(GL_SCISSOR_TEST);
	}

	void PostProcess2D::BlitTexture(unsigned int srcTexture, unsigned int destFramebuffer, int width, int height)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, destFramebuffer);
		glViewport(0, 0, width, height);
		DrawFullscreen(srcTexture, false, nullptr);
	}

	GameObject* PostProcess2D::FindActive(const std::vector<GameObject*>& objects)
	{
		for (GameObject* object : objects)
		{
			if (object && object->IsPostProcess() && object->IsActive()
				&& object->IsPostProcessEnabled())
				return object;
		}
		return nullptr;
	}

	void PostProcess2D::Apply(const std::vector<GameObject*>& objects,
		unsigned int srcTexture, unsigned int destFramebuffer, int width, int height)
	{
		if (!srcTexture || width <= 0 || height <= 0)
			return;

		GameObject* settings = FindActive(objects);
		if (!settings)
			return;

		EnsureResources(width, height);

		glBindFramebuffer(GL_FRAMEBUFFER, s_PingFbo);
		glViewport(0, 0, width, height);
		DrawFullscreen(srcTexture, false, nullptr);

		glBindFramebuffer(GL_FRAMEBUFFER, destFramebuffer);
		glViewport(0, 0, width, height);
		DrawFullscreen(s_PingTex, true, settings);
		glBindFramebuffer(GL_FRAMEBUFFER, destFramebuffer);
	}

	void PostProcess2D::BeginSceneCapture(int width, int height)
	{
		InitShaderAndQuad();
		if (width <= 0 || height <= 0)
			return;

		if (s_SceneFbo == 0 || s_SceneW != width || s_SceneH != height)
		{
			if (s_SceneTex)
				glDeleteTextures(1, &s_SceneTex);
			if (s_SceneFbo)
				glDeleteFramebuffers(1, &s_SceneFbo);

			glGenFramebuffers(1, &s_SceneFbo);
			glGenTextures(1, &s_SceneTex);
			glBindTexture(GL_TEXTURE_2D, s_SceneTex);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindFramebuffer(GL_FRAMEBUFFER, s_SceneFbo);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_SceneTex, 0);
			glBindTexture(GL_TEXTURE_2D, 0);
			s_SceneW = width;
			s_SceneH = height;
		}

		glBindFramebuffer(GL_FRAMEBUFFER, s_SceneFbo);
		glViewport(0, 0, width, height);
		glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
	}

	void PostProcess2D::EndSceneCapturePresent(const std::vector<GameObject*>& objects,
		int windowWidth, int windowHeight)
	{
		if (!s_SceneFbo || !s_SceneTex)
			return;

		GameObject* settings = FindActive(objects);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, windowWidth, windowHeight);

		if (settings)
		{
			EnsureResources(s_SceneW, s_SceneH);
			glBindFramebuffer(GL_FRAMEBUFFER, s_PingFbo);
			glViewport(0, 0, s_SceneW, s_SceneH);
			DrawFullscreen(s_SceneTex, false, nullptr);

			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glViewport(0, 0, windowWidth, windowHeight);
			DrawFullscreen(s_PingTex, true, settings);
		}
		else
		{
			BlitTexture(s_SceneTex, 0, windowWidth, windowHeight);
		}
	}

	void PostProcess2D::Shutdown()
	{
		if (s_PingTex) { glDeleteTextures(1, &s_PingTex); s_PingTex = 0; }
		if (s_PingFbo) { glDeleteFramebuffers(1, &s_PingFbo); s_PingFbo = 0; }
		if (s_SceneTex) { glDeleteTextures(1, &s_SceneTex); s_SceneTex = 0; }
		if (s_SceneFbo) { glDeleteFramebuffers(1, &s_SceneFbo); s_SceneFbo = 0; }
		if (s_Vbo) { glDeleteBuffers(1, &s_Vbo); s_Vbo = 0; }
		if (s_Vao) { glDeleteVertexArrays(1, &s_Vao); s_Vao = 0; }
		s_PingW = s_PingH = 0;
		s_SceneW = s_SceneH = 0;
		s_Ready = false;
	}
}
