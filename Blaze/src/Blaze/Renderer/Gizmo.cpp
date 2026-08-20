#include "Gizmo.h"
#include<glad/glad.h>
#include"Blaze/Renderer/Camera.h"
#include"Blaze/Renderer/GameObject.h"
#include"Blaze/Dep/stb_image.h"
#include<gtc/matrix_transform.hpp>
#include<gtc/constants.hpp>
#include <cmath>
#include <vector>
#include <algorithm>
#include <filesystem>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace Blaze
{
	unsigned int Gizmo2D::s_SharedSoundIconTex = 0;
	unsigned int Gizmo2D::s_SharedParticleIconTex = 0;
	unsigned int Gizmo2D::s_SharedCameraIconTex = 0;
	unsigned int Gizmo2D::s_SharedPointLightIconTex = 0;
	unsigned int Gizmo2D::s_SharedSpotLightIconTex = 0;
	unsigned int Gizmo2D::s_SharedScriptIconTex = 0;
	unsigned int Gizmo2D::s_SharedSaveScriptIconTex = 0;
	unsigned int Gizmo2D::s_SharedMapIconTex = 0;
	unsigned int Gizmo2D::s_SharedFolderIconTex = 0;

	static const char* s_GizmoVertexSource = "#version 330 core\n"
		"layout (location = 0) in vec3 pos;\n"
		"uniform mat4 matrix;\n"
		"uniform mat4 transform;\n"
		"void main()\n"
		"{\n"
		"   gl_Position = matrix * transform * vec4(pos, 1.0f);\n"
		"}\0";

	static const char* s_GizmoFragmentSource = "#version 330 core\n"
		"uniform vec4 color;\n"
		"out vec4 FragColor;\n"
		"void main()\n"
		"{\n"
		"   FragColor = color;\n"
		"}\0";

	static const char* s_IconVertexSource = "#version 330 core\n"
		"layout (location = 0) in vec3 pos;\n"
		"layout (location = 1) in vec2 uv;\n"
		"uniform mat4 matrix;\n"
		"uniform mat4 transform;\n"
		"out vec2 vUV;\n"
		"void main()\n"
		"{\n"
		"   vUV = uv;\n"
		"   gl_Position = matrix * transform * vec4(pos, 1.0f);\n"
		"}\0";

	static const char* s_IconFragmentSource = "#version 330 core\n"
		"in vec2 vUV;\n"
		"uniform sampler2D icon;\n"
		"uniform vec4 tint;\n"
		"out vec4 FragColor;\n"
		"void main()\n"
		"{\n"
		"   vec4 c = texture(icon, vUV);\n"
		"   if (c.a < 0.05) discard;\n"
		"   FragColor = vec4(c.rgb * tint.rgb, c.a * tint.a);\n"
		"}\0";

	static const float s_CenterHalf = 10.0f;
	static const float s_ArmLength = 90.0f;
	static const float s_ArmHalfThickness = 4.0f;
	static const float s_HeadHalfLength = 14.0f;
	static const float s_HeadHalfWidth = 10.0f;
	static const float s_ScaleTipHalf = 10.0f;
	static const float s_RingRadius = 110.0f;
	static const float s_RingHitBand = 14.0f;
	static const float s_HitPadding = 6.0f;

	static const glm::vec4 s_ColorX(0.9f, 0.2f, 0.2f, 1.0f);
	static const glm::vec4 s_ColorY(0.2f, 0.9f, 0.2f, 1.0f);
	static const glm::vec4 s_ColorCenter(0.95f, 0.85f, 0.2f, 1.0f);
	static const glm::vec4 s_ColorRing(0.25f, 0.6f, 1.0f, 1.0f);
	static const glm::vec4 s_ColorCollision(0.95f, 0.85f, 0.15f, 0.95f);
	static const glm::vec4 s_ColorTrigger(0.2f, 0.9f, 0.95f, 0.9f);
	static const glm::vec4 s_ColorCameraRange(0.2f, 0.75f, 1.0f, 0.95f);

	// Load the reference speaker PNG (black on white) and turn it into
	// a white-on-transparent editor icon, same silhouette as Unity/Godot/Material.
	static unsigned int CreateSpeakerIconTexture(int /*size*/)
	{
		namespace fs = std::filesystem;

		std::vector<fs::path> candidates = {
			"content/icons/sound.png",
			"../Blaze/res/icons/sound.png",
			"Blaze/res/icons/sound.png",
			"res/icons/sound.png",
		};

#ifdef _WIN32
		char modulePath[MAX_PATH] = {};
		if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH))
		{
			fs::path exeDir = fs::path(modulePath).parent_path();
			candidates.push_back(exeDir / "content" / "icons" / "sound.png");
			candidates.push_back(exeDir / "icons" / "sound.png");
			candidates.push_back(exeDir / ".." / ".." / ".." / "Blaze" / "res" / "icons" / "sound.png");
			candidates.push_back(exeDir / ".." / ".." / ".." / "Sandbox" / "content" / "icons" / "sound.png");
		}
#endif

		int width = 0, height = 0, channels = 0;
		unsigned char* data = nullptr;
		for (const fs::path& path : candidates)
		{
			std::error_code ec;
			if (!fs::exists(path, ec))
				continue;
			data = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
			if (data)
				break;
		}

		if (!data || width <= 0 || height <= 0)
		{
			// Minimal fallback silhouette if the PNG is missing
			width = 64;
			height = 64;
			std::vector<unsigned char> fallback((size_t)width * height * 4, 0);
			auto put = [&](int x, int y, unsigned char a)
			{
				if (x < 0 || y < 0 || x >= width || y >= height) return;
				size_t i = ((size_t)y * width + x) * 4;
				fallback[i] = 245; fallback[i + 1] = 245; fallback[i + 2] = 245; fallback[i + 3] = a;
			};
			for (int y = 22; y < 42; ++y)
				for (int x = 8; x < 18; ++x)
					put(x, y, 255);
			for (int y = 0; y < height; ++y)
			{
				for (int x = 0; x < width; ++x)
				{
					float t = ((float)x - 18.0f) / 18.0f;
					if (t < 0.0f || t > 1.0f) continue;
					float half = 10.0f + t * 18.0f;
					if (std::fabs((float)y - 32.0f) <= half)
						put(x, y, 255);
				}
			}
			unsigned int tex = 0;
			glGenTextures(1, &tex);
			glBindTexture(GL_TEXTURE_2D, tex);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, fallback.data());
			glBindTexture(GL_TEXTURE_2D, 0);
			return tex;
		}

		std::vector<unsigned char> out((size_t)width * height * 4);
		for (int i = 0; i < width * height; ++i)
		{
			unsigned char r = data[i * 4 + 0];
			unsigned char g = data[i * 4 + 1];
			unsigned char b = data[i * 4 + 2];
			unsigned char a = data[i * 4 + 3];

			// Dark pixels = icon; light pixels = background
			float luminance = (0.299f * r + 0.587f * g + 0.114f * b) / 255.0f;
			float ink = 1.0f - luminance;
			ink = ink * ink; // keep edges crisp
			unsigned char alpha = (unsigned char)std::clamp(ink * (a / 255.0f) * 255.0f, 0.0f, 255.0f);

			out[i * 4 + 0] = 245;
			out[i * 4 + 1] = 245;
			out[i * 4 + 2] = 245;
			out[i * 4 + 3] = alpha;
		}
		stbi_image_free(data);

		unsigned int tex = 0;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, out.data());
		glBindTexture(GL_TEXTURE_2D, 0);
		return tex;
	}

	static void PutIconPixel(std::vector<unsigned char>& px, int size, int x, int y, unsigned char a)
	{
		if (x < 0 || y < 0 || x >= size || y >= size || a == 0)
			return;
		size_t i = ((size_t)y * size + x) * 4;
		if (a > px[i + 3])
		{
			px[i + 0] = 245;
			px[i + 1] = 245;
			px[i + 2] = 245;
			px[i + 3] = a;
		}
	}

	static void PutIconPixelRGB(std::vector<unsigned char>& px, int size, int x, int y,
		unsigned char r, unsigned char g, unsigned char b, unsigned char a)
	{
		if (x < 0 || y < 0 || x >= size || y >= size || a == 0)
			return;
		size_t i = ((size_t)y * size + x) * 4;
		if (a >= px[i + 3])
		{
			px[i + 0] = r;
			px[i + 1] = g;
			px[i + 2] = b;
			px[i + 3] = a;
		}
	}

	static void FillIconCircleRGB(std::vector<unsigned char>& px, int size, float cx, float cy, float radius,
		unsigned char r, unsigned char g, unsigned char b)
	{
		int x0 = (int)std::floor(cx - radius - 1.0f);
		int x1 = (int)std::ceil(cx + radius + 1.0f);
		int y0 = (int)std::floor(cy - radius - 1.0f);
		int y1 = (int)std::ceil(cy + radius + 1.0f);
		for (int y = y0; y <= y1; ++y)
		{
			for (int x = x0; x <= x1; ++x)
			{
				float dx = (float)x + 0.5f - cx;
				float dy = (float)y + 0.5f - cy;
				float d = std::sqrt(dx * dx + dy * dy);
				if (d <= radius)
					PutIconPixelRGB(px, size, x, y, r, g, b, 255);
				else if (d <= radius + 0.75f)
					PutIconPixelRGB(px, size, x, y, r, g, b,
						(unsigned char)(255.0f * (1.0f - (d - radius) / 0.75f)));
			}
		}
	}

	static void FillIconRingRGB(std::vector<unsigned char>& px, int size, float cx, float cy,
		float outerR, float innerR, unsigned char r, unsigned char g, unsigned char b)
	{
		int x0 = (int)std::floor(cx - outerR - 1.0f);
		int x1 = (int)std::ceil(cx + outerR + 1.0f);
		int y0 = (int)std::floor(cy - outerR - 1.0f);
		int y1 = (int)std::ceil(cy + outerR + 1.0f);
		for (int y = y0; y <= y1; ++y)
		{
			for (int x = x0; x <= x1; ++x)
			{
				float dx = (float)x + 0.5f - cx;
				float dy = (float)y + 0.5f - cy;
				float d = std::sqrt(dx * dx + dy * dy);
				if (d <= outerR && d >= innerR)
					PutIconPixelRGB(px, size, x, y, r, g, b, 255);
			}
		}
	}

	static void FillIconLineRGB(std::vector<unsigned char>& px, int size,
		float x0, float y0, float x1, float y1, float thickness,
		unsigned char r, unsigned char g, unsigned char b)
	{
		float dx = x1 - x0;
		float dy = y1 - y0;
		float len = std::sqrt(dx * dx + dy * dy);
		if (len < 1e-4f)
			return;
		dx /= len;
		dy /= len;
		float nx = -dy;
		float ny = dx;
		float half = thickness * 0.5f;
		const int steps = (int)std::ceil(len) + 1;
		for (int i = 0; i <= steps; ++i)
		{
			float t = (float)i / (float)steps;
			float cx = x0 + (x1 - x0) * t;
			float cy = y0 + (y1 - y0) * t;
			for (float w = -half; w <= half; w += 0.5f)
			{
				int x = (int)std::floor(cx + nx * w);
				int y = (int)std::floor(cy + ny * w);
				PutIconPixelRGB(px, size, x, y, r, g, b, 255);
			}
		}
	}

	static void FillIconTriangleRGB(std::vector<unsigned char>& px, int size,
		float x0, float y0, float x1, float y1, float x2, float y2,
		unsigned char r, unsigned char g, unsigned char b)
	{
		float minX = std::min({ x0, x1, x2 });
		float maxX = std::max({ x0, x1, x2 });
		float minY = std::min({ y0, y1, y2 });
		float maxY = std::max({ y0, y1, y2 });
		auto edge = [](float ax, float ay, float bx, float by, float cx, float cy)
		{
			return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
		};
		float area = edge(x0, y0, x1, y1, x2, y2);
		if (std::abs(area) < 1e-6f)
			return;
		for (int y = (int)std::floor(minY); y <= (int)std::ceil(maxY); ++y)
		{
			for (int x = (int)std::floor(minX); x <= (int)std::ceil(maxX); ++x)
			{
				float pxX = (float)x + 0.5f;
				float pxY = (float)y + 0.5f;
				float w0 = edge(x1, y1, x2, y2, pxX, pxY);
				float w1 = edge(x2, y2, x0, y0, pxX, pxY);
				float w2 = edge(x0, y0, x1, y1, pxX, pxY);
				if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0))
					PutIconPixelRGB(px, size, x, y, r, g, b, 255);
			}
		}
	}

	static void FillIconCircle(std::vector<unsigned char>& px, int size, float cx, float cy, float radius)
	{
		int x0 = (int)std::floor(cx - radius - 1.0f);
		int x1 = (int)std::ceil(cx + radius + 1.0f);
		int y0 = (int)std::floor(cy - radius - 1.0f);
		int y1 = (int)std::ceil(cy + radius + 1.0f);
		for (int y = y0; y <= y1; ++y)
		{
			for (int x = x0; x <= x1; ++x)
			{
				float dx = (float)x + 0.5f - cx;
				float dy = (float)y + 0.5f - cy;
				float d = std::sqrt(dx * dx + dy * dy);
				if (d <= radius)
					PutIconPixel(px, size, x, y, 255);
				else if (d <= radius + 0.75f)
					PutIconPixel(px, size, x, y, (unsigned char)(255.0f * (1.0f - (d - radius) / 0.75f)));
			}
		}
	}

	static void FillIconRing(std::vector<unsigned char>& px, int size, float cx, float cy, float outerR, float innerR)
	{
		int x0 = (int)std::floor(cx - outerR - 1.0f);
		int x1 = (int)std::ceil(cx + outerR + 1.0f);
		int y0 = (int)std::floor(cy - outerR - 1.0f);
		int y1 = (int)std::ceil(cy + outerR + 1.0f);
		for (int y = y0; y <= y1; ++y)
		{
			for (int x = x0; x <= x1; ++x)
			{
				float dx = (float)x + 0.5f - cx;
				float dy = (float)y + 0.5f - cy;
				float d = std::sqrt(dx * dx + dy * dy);
				if (d <= outerR && d >= innerR)
					PutIconPixel(px, size, x, y, 255);
			}
		}
	}

	static void FillIconRect(std::vector<unsigned char>& px, int size, float x0, float y0, float x1, float y1)
	{
		for (int y = (int)std::floor(y0); y <= (int)std::ceil(y1); ++y)
			for (int x = (int)std::floor(x0); x <= (int)std::ceil(x1); ++x)
				PutIconPixel(px, size, x, y, 255);
	}

	static void FillIconRoundedRect(std::vector<unsigned char>& px, int size,
		float x0, float y0, float x1, float y1, float radius)
	{
		FillIconRect(px, size, x0 + radius, y0, x1 - radius, y1);
		FillIconRect(px, size, x0, y0 + radius, x1, y1 - radius);
		FillIconCircle(px, size, x0 + radius, y0 + radius, radius);
		FillIconCircle(px, size, x1 - radius, y0 + radius, radius);
		FillIconCircle(px, size, x0 + radius, y1 - radius, radius);
		FillIconCircle(px, size, x1 - radius, y1 - radius, radius);
	}

	static void FillIconTriangle(std::vector<unsigned char>& px, int size,
		float x0, float y0, float x1, float y1, float x2, float y2)
	{
		float minX = std::min({ x0, x1, x2 });
		float maxX = std::max({ x0, x1, x2 });
		float minY = std::min({ y0, y1, y2 });
		float maxY = std::max({ y0, y1, y2 });

		auto edge = [](float ax, float ay, float bx, float by, float cx, float cy)
		{
			return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
		};

		float area = edge(x0, y0, x1, y1, x2, y2);
		if (std::fabs(area) < 1e-4f)
			return;

		for (int y = (int)std::floor(minY); y <= (int)std::ceil(maxY); ++y)
		{
			for (int x = (int)std::floor(minX); x <= (int)std::ceil(maxX); ++x)
			{
				float pxPos = (float)x + 0.5f;
				float pyPos = (float)y + 0.5f;
				float w0 = edge(x1, y1, x2, y2, pxPos, pyPos);
				float w1 = edge(x2, y2, x0, y0, pxPos, pyPos);
				float w2 = edge(x0, y0, x1, y1, pxPos, pyPos);
				if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0))
					PutIconPixel(px, size, x, y, 255);
			}
		}
	}

	static unsigned int UploadIconTexture(const std::vector<unsigned char>& px, int width, int height)
	{
		unsigned int tex = 0;
		glGenTextures(1, &tex);
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
		glBindTexture(GL_TEXTURE_2D, 0);
		return tex;
	}

	static std::vector<std::filesystem::path> IconSearchPaths(const char* fileName)
	{
		namespace fs = std::filesystem;
		std::vector<fs::path> candidates = {
			fs::path("content") / "icons" / fileName,
			fs::path("../Blaze/res/icons") / fileName,
			fs::path("Blaze/res/icons") / fileName,
			fs::path("res/icons") / fileName,
		};
#ifdef _WIN32
		char modulePath[MAX_PATH] = {};
		if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH))
		{
			fs::path exeDir = fs::path(modulePath).parent_path();
			candidates.push_back(exeDir / "content" / "icons" / fileName);
			candidates.push_back(exeDir / "icons" / fileName);
			candidates.push_back(exeDir / ".." / ".." / ".." / "Blaze" / "res" / "icons" / fileName);
			candidates.push_back(exeDir / ".." / ".." / ".." / "Sandbox" / "content" / "icons" / fileName);
		}
#endif
		return candidates;
	}

	static void ClearIconRect(std::vector<unsigned char>& px, int size, float x0, float y0, float x1, float y1)
	{
		const int ix0 = (int)std::floor(x0);
		const int iy0 = (int)std::floor(y0);
		const int ix1 = (int)std::ceil(x1);
		const int iy1 = (int)std::ceil(y1);
		for (int y = iy0; y <= iy1; ++y)
		{
			for (int x = ix0; x <= ix1; ++x)
			{
				if (x < 0 || y < 0 || x >= size || y >= size)
					continue;
				const size_t i = ((size_t)y * size + x) * 4;
				px[i + 0] = 0;
				px[i + 1] = 0;
				px[i + 2] = 0;
				px[i + 3] = 0;
			}
		}
	}

	static void ClearIconCircle(std::vector<unsigned char>& px, int size, float cx, float cy, float radius)
	{
		int x0 = (int)std::floor(cx - radius - 1.0f);
		int x1 = (int)std::ceil(cx + radius + 1.0f);
		int y0 = (int)std::floor(cy - radius - 1.0f);
		int y1 = (int)std::ceil(cy + radius + 1.0f);
		for (int y = y0; y <= y1; ++y)
		{
			for (int x = x0; x <= x1; ++x)
			{
				float dx = (float)x + 0.5f - cx;
				float dy = (float)y + 0.5f - cy;
				if (dx * dx + dy * dy <= radius * radius)
				{
					if (x < 0 || y < 0 || x >= size || y >= size)
						continue;
					const size_t i = ((size_t)y * size + x) * 4;
					px[i + 0] = 0;
					px[i + 1] = 0;
					px[i + 2] = 0;
					px[i + 3] = 0;
				}
			}
		}
	}


	static unsigned int CreateFolderIconTexture(int size)
	{
		std::vector<unsigned char> px((size_t)size * size * 4, 0);
		const float s = (float)size;

		// Tab
		FillIconRoundedRect(px, size, s * 0.14f, s * 0.22f, s * 0.48f, s * 0.40f, s * 0.04f);
		// Body
		FillIconRoundedRect(px, size, s * 0.14f, s * 0.34f, s * 0.86f, s * 0.82f, s * 0.06f);

		return UploadIconTexture(px, size, size);
	}


	static unsigned int CreateScriptIconTexture(int size)
	{
		std::vector<unsigned char> px((size_t)size * size * 4, 0);
		const float s = (float)size;

		FillIconRoundedRect(px, size, s * 0.22f, s * 0.10f, s * 0.78f, s * 0.90f, s * 0.04f);

	
		ClearIconRect(px, size, s * 0.58f, s * 0.10f, s * 0.78f, s * 0.30f);
		FillIconTriangle(px, size,
			s * 0.58f, s * 0.10f,
			s * 0.58f, s * 0.30f,
			s * 0.78f, s * 0.30f);

		const float lineH = s * 0.055f;
		ClearIconRect(px, size, s * 0.32f, s * 0.40f, s * 0.68f, s * 0.40f + lineH);
		ClearIconRect(px, size, s * 0.32f, s * 0.50f, s * 0.62f, s * 0.50f + lineH);
		ClearIconRect(px, size, s * 0.32f, s * 0.60f, s * 0.70f, s * 0.60f + lineH);
		ClearIconRect(px, size, s * 0.32f, s * 0.70f, s * 0.55f, s * 0.70f + lineH);

		return UploadIconTexture(px, size, size);
	}

	static unsigned int CreateSaveScriptIconTexture(int size)
	{
		std::vector<unsigned char> px((size_t)size * size * 4, 0);
		const float s = (float)size;

		// Disk body
		FillIconRoundedRect(px, size, s * 0.16f, s * 0.14f, s * 0.84f, s * 0.88f, s * 0.05f);

		// Top-right write-protect notch cutout
		ClearIconRect(px, size, s * 0.68f, s * 0.14f, s * 0.84f, s * 0.28f);

		// Label area (cutout rectangle near top)
		ClearIconRect(px, size, s * 0.28f, s * 0.22f, s * 0.62f, s * 0.40f);

		// Hub ring + center hole
		ClearIconCircle(px, size, s * 0.50f, s * 0.62f, s * 0.14f);
		FillIconRing(px, size, s * 0.50f, s * 0.62f, s * 0.14f, s * 0.06f);

		// Bottom metal shutter
		ClearIconRect(px, size, s * 0.30f, s * 0.74f, s * 0.70f, s * 0.84f);

		return UploadIconTexture(px, size, size);
	}

	static unsigned int CreateMapIconTexture(int size)
	{
		namespace fs = std::filesystem;
		const unsigned char bgR = 0, bgG = 0, bgB = 0;

		std::vector<fs::path> candidates = IconSearchPaths("map.png");

		int width = 0, height = 0, channels = 0;
		unsigned char* data = nullptr;
		for (const fs::path& path : candidates)
		{
			std::error_code ec;
			if (!fs::exists(path, ec))
				continue;
			data = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
			if (data)
				break;
		}

		if (data && width > 0 && height > 0)
		{
			std::vector<unsigned char> out((size_t)width * height * 4);
			for (int i = 0; i < width * height; ++i)
			{
				unsigned char r = data[i * 4 + 0];
				unsigned char g = data[i * 4 + 1];
				unsigned char b = data[i * 4 + 2];
				unsigned char a = data[i * 4 + 3];
				float luminance = (0.299f * r + 0.587f * g + 0.114f * b) / 255.0f;
				float ink = luminance * (a / 255.0f);
				if (ink > 0.35f)
				{
					out[i * 4 + 0] = 245;
					out[i * 4 + 1] = 245;
					out[i * 4 + 2] = 245;
				}
				else
				{
					out[i * 4 + 0] = 0;
					out[i * 4 + 1] = 0;
					out[i * 4 + 2] = 0;
				}
				out[i * 4 + 3] = 255; // fully opaque  no transparent checker
			}
			stbi_image_free(data);
			return UploadIconTexture(out, width, height);
		}

		
		std::vector<unsigned char> px((size_t)size * size * 4, 0);
		for (int i = 0; i < size * size; ++i)
		{
			px[i * 4 + 0] = bgR;
			px[i * 4 + 1] = bgG;
			px[i * 4 + 2] = bgB;
			px[i * 4 + 3] = 255;
		}

		const float s = (float)size;
		auto putWhite = [&](int x, int y)
		{
			if (x < 0 || y < 0 || x >= size || y >= size) return;
			size_t i = ((size_t)y * size + x) * 4;
			px[i + 0] = 245; px[i + 1] = 245; px[i + 2] = 245; px[i + 3] = 255;
		};

		// Reuse silhouette helpers into a temp alpha buffer, then composite.
		std::vector<unsigned char> mask((size_t)size * size * 4, 0);
		FillIconRoundedRect(mask, size, s * 0.10f, s * 0.62f, s * 0.90f, s * 0.86f, s * 0.04f);
		FillIconTriangle(mask, size,
			s * 0.12f, s * 0.66f,
			s * 0.42f, s * 0.66f,
			s * 0.30f, s * 0.18f);
		FillIconTriangle(mask, size,
			s * 0.28f, s * 0.66f,
			s * 0.58f, s * 0.66f,
			s * 0.42f, s * 0.28f);
		FillIconTriangle(mask, size,
			s * 0.48f, s * 0.66f,
			s * 0.88f, s * 0.66f,
			s * 0.68f, s * 0.34f);
		FillIconRect(mask, size, s * 0.14f, s * 0.58f, s * 0.86f, s * 0.70f);

		for (int y = 0; y < size; ++y)
		{
			for (int x = 0; x < size; ++x)
			{
				if (mask[((size_t)y * size + x) * 4 + 3] > 0)
					putWhite(x, y);
			}
		}

		return UploadIconTexture(px, size, size);
	}


	static unsigned int CreatePointLightIconTexture(int size)
	{
		std::vector<unsigned char> px((size_t)size * size * 4, 0);
		const float s = (float)size;
		const float cx = s * 0.50f;
		const float cy = s * 0.50f;
		const unsigned char r = 255, g = 210, b = 64;

		const int rays = 8;
		for (int i = 0; i < rays; ++i)
		{
			float ang = glm::two_pi<float>() * (float)i / (float)rays;
			float c = std::cos(ang);
			float sn = std::sin(ang);
			FillIconLineRGB(px, size,
				cx + c * s * 0.18f, cy + sn * s * 0.18f,
				cx + c * s * 0.44f, cy + sn * s * 0.44f,
				s * 0.055f, r, g, b);
		}

	
		for (int i = 0; i < rays; ++i)
		{
			float ang = glm::two_pi<float>() * ((float)i + 0.5f) / (float)rays;
			float c = std::cos(ang);
			float sn = std::sin(ang);
			FillIconLineRGB(px, size,
				cx + c * s * 0.22f, cy + sn * s * 0.22f,
				cx + c * s * 0.34f, cy + sn * s * 0.34f,
				s * 0.035f, r, g, b);
		}

		FillIconCircleRGB(px, size, cx, cy, s * 0.16f, r, g, b);
		FillIconRingRGB(px, size, cx, cy, s * 0.20f, s * 0.155f, 255, 235, 140);
		return UploadIconTexture(px, size, size);
	}

	
	static unsigned int CreateParticleIconTexture(int size)
	{
		std::vector<unsigned char> px((size_t)size * size * 4, 0);
		const float s = (float)size;
		const float cx = s * 0.50f;
		const float cy = s * 0.50f;

		FillIconCircleRGB(px, size, cx, cy, s * 0.10f, 255, 230, 120);

		struct Spark { float ang; float dist; float rad; unsigned char r, g, b; };
		const Spark sparks[] = {
			{ 0.20f, 0.34f, 0.055f, 255, 180, 60 },
			{ 1.10f, 0.30f, 0.045f, 255, 140, 40 },
			{ 2.05f, 0.36f, 0.050f, 255, 210, 90 },
			{ 2.90f, 0.28f, 0.040f, 255, 120, 50 },
			{ 3.80f, 0.33f, 0.048f, 255, 200, 70 },
			{ 4.70f, 0.31f, 0.042f, 255, 160, 45 },
			{ 5.50f, 0.35f, 0.052f, 255, 220, 100 },
			{ 0.70f, 0.22f, 0.035f, 255, 240, 160 },
			{ 4.10f, 0.20f, 0.032f, 255, 190, 80 },
		};
		for (const Spark& spark : sparks)
		{
			const float x = cx + std::cos(spark.ang) * spark.dist * s;
			const float y = cy + std::sin(spark.ang) * spark.dist * s;
			FillIconCircleRGB(px, size, x, y, spark.rad * s, spark.r, spark.g, spark.b);
		}

		return UploadIconTexture(px, size, size);
	}


	static unsigned int CreateSpotLightIconTexture(int size)
	{
		std::vector<unsigned char> px((size_t)size * size * 4, 0);
		const float s = (float)size;
		const unsigned char r = 255, g = 210, b = 64;

	
		FillIconCircleRGB(px, size, s * 0.28f, s * 0.50f, s * 0.14f, r, g, b);
		FillIconLineRGB(px, size, s * 0.14f, s * 0.50f, s * 0.28f, s * 0.50f, s * 0.08f, r, g, b);

		
		FillIconRingRGB(px, size, s * 0.36f, s * 0.50f, s * 0.12f, s * 0.07f, 255, 235, 140);

		
		FillIconTriangleRGB(px, size,
			s * 0.40f, s * 0.50f,
			s * 0.88f, s * 0.18f,
			s * 0.88f, s * 0.82f,
			r, g, b);

	
		auto clearTri = [&](float x0, float y0, float x1, float y1, float x2, float y2)
		{
			float minX = std::min({ x0, x1, x2 });
			float maxX = std::max({ x0, x1, x2 });
			float minY = std::min({ y0, y1, y2 });
			float maxY = std::max({ y0, y1, y2 });
			auto edge = [](float ax, float ay, float bx, float by, float cx, float cy)
			{
				return (cx - ax) * (by - ay) - (cy - ay) * (bx - ax);
			};
			for (int y = (int)std::floor(minY); y <= (int)std::ceil(maxY); ++y)
			{
				for (int x = (int)std::floor(minX); x <= (int)std::ceil(maxX); ++x)
				{
					float pxX = (float)x + 0.5f;
					float pxY = (float)y + 0.5f;
					float w0 = edge(x1, y1, x2, y2, pxX, pxY);
					float w1 = edge(x2, y2, x0, y0, pxX, pxY);
					float w2 = edge(x0, y0, x1, y1, pxX, pxY);
					if ((w0 >= 0 && w1 >= 0 && w2 >= 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0))
					{
						size_t i = ((size_t)y * size + x) * 4;
						if (x >= 0 && y >= 0 && x < size && y < size)
							px[i + 3] = 0;
					}
				}
			}
		};
		clearTri(s * 0.48f, s * 0.50f, s * 0.84f, s * 0.30f, s * 0.84f, s * 0.70f);

	
		FillIconLineRGB(px, size, s * 0.40f, s * 0.50f, s * 0.88f, s * 0.18f, s * 0.04f, r, g, b);
		FillIconLineRGB(px, size, s * 0.40f, s * 0.50f, s * 0.88f, s * 0.82f, s * 0.04f, r, g, b);
		FillIconLineRGB(px, size, s * 0.88f, s * 0.18f, s * 0.88f, s * 0.82f, s * 0.035f, 255, 235, 140);

		return UploadIconTexture(px, size, size);
	}

	static unsigned int CreateCameraIconTexture(int size)
	{
		std::vector<unsigned char> px((size_t)size * size * 4, 0);
		const float s = (float)size;


		FillIconRoundedRect(px, size, s * 0.14f, s * 0.48f, s * 0.60f, s * 0.80f, s * 0.05f);


		FillIconCircle(px, size, s * 0.30f, s * 0.36f, s * 0.17f);
		FillIconCircle(px, size, s * 0.50f, s * 0.30f, s * 0.20f);

		// Trapezoid lens facing right
		FillIconTriangle(px, size,
			s * 0.58f, s * 0.52f,
			s * 0.58f, s * 0.76f,
			s * 0.86f, s * 0.46f);
		FillIconTriangle(px, size,
			s * 0.58f, s * 0.76f,
			s * 0.86f, s * 0.46f,
			s * 0.86f, s * 0.82f);

		return UploadIconTexture(px, size, size);
	}

	Gizmo2D::Gizmo2D()
	{
		m_Shader.CreateProgram(s_GizmoVertexSource, s_GizmoFragmentSource);
		m_Shader.LinkProgram();

		m_IconShader.CreateProgram(s_IconVertexSource, s_IconFragmentSource);
		m_IconShader.LinkProgram();

		{
			float vertices[] =
			{
				 1.0f,  1.0f, 0.0f,
				 1.0f, -1.0f, 0.0f,
				-1.0f, -1.0f, 0.0f,
				-1.0f,  1.0f, 0.0f,
			};
			unsigned int indices[] = { 0, 1, 3, 1, 2, 3 };

			glGenBuffers(1, &m_QuadEBO);
			m_QuadVAO.Bind();
			m_QuadVBO.Bind();
			m_QuadVBO.SetFloat(vertices, sizeof(vertices), 3);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_QuadEBO);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
			m_QuadVAO.AttribPointer(m_QuadVBO, 3, 0);
			m_QuadVBO.UnBind();
			m_QuadVAO.UnBind();
		}

		{
			float vertices[] =
			{
				 1.0f,  0.0f, 0.0f,
				-1.0f,  1.0f, 0.0f,
				-1.0f, -1.0f, 0.0f,
			};

			m_TriVAO.Bind();
			m_TriVBO.Bind();
			m_TriVBO.SetFloat(vertices, sizeof(vertices), 3);
			m_TriVAO.AttribPointer(m_TriVBO, 3, 0);
			m_TriVBO.UnBind();
			m_TriVAO.UnBind();
		}

		{
			const int segments = 48;
			std::vector<float> vertices;
			vertices.reserve((segments + 1) * 6);
			for (int i = 0; i <= segments; ++i)
			{
				float a = glm::two_pi<float>() * (float)i / (float)segments;
				float c = glm::cos(a), s = glm::sin(a);
				vertices.push_back(c);         vertices.push_back(s);         vertices.push_back(0.0f);
				vertices.push_back(c * 0.88f); vertices.push_back(s * 0.88f); vertices.push_back(0.0f);
			}
			m_RingVertexCount = (segments + 1) * 2;

			m_RingVAO.Bind();
			m_RingVBO.Bind();
			m_RingVBO.SetFloat(vertices.data(), (uint32_t)(vertices.size() * sizeof(float)), 3);
			m_RingVAO.AttribPointer(m_RingVBO, 3, 0);
			m_RingVBO.UnBind();
			m_RingVAO.UnBind();
		}

		{
			float vertices[] =
			{
				 1.0f,  1.0f, 0.0f, 1.0f, 0.0f,
				 1.0f, -1.0f, 0.0f, 1.0f, 1.0f,
				-1.0f, -1.0f, 0.0f, 0.0f, 1.0f,
				-1.0f,  1.0f, 0.0f, 0.0f, 0.0f,
			};
			unsigned int indices[] = { 0, 1, 3, 1, 2, 3 };

			glGenBuffers(1, &m_IconEBO);
			m_IconVAO.Bind();
			m_IconVBO.Bind();
			m_IconVBO.SetFloat(vertices, sizeof(vertices), 5);
			glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_IconEBO);
			glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
			m_IconVAO.AttribPointer(m_IconVBO, 3, 0);
			m_IconVAO.AttribPointer(m_IconVBO, 2, 3);
			m_IconVBO.UnBind();
			m_IconVAO.UnBind();
		}

		EnsureSoundIcon();
		EnsureParticleIcon();
		EnsureCameraIcon();
		EnsureLightIcons();
	}

	Gizmo2D::~Gizmo2D()
	{
		glDeleteBuffers(1, &m_QuadEBO);
		if (m_IconEBO)
			glDeleteBuffers(1, &m_IconEBO);
	}

	void Gizmo2D::EnsureSoundIcon()
	{
		if (s_SharedSoundIconTex == 0)
			s_SharedSoundIconTex = CreateSpeakerIconTexture(s_SoundIconSize);
		m_SoundIconTex = s_SharedSoundIconTex;
	}

	void Gizmo2D::EnsureParticleIcon()
	{
		if (s_SharedParticleIconTex == 0)
			s_SharedParticleIconTex = CreateParticleIconTexture(s_ParticleIconSize);
		m_ParticleIconTex = s_SharedParticleIconTex;
	}

	void Gizmo2D::EnsureCameraIcon()
	{
		if (s_SharedCameraIconTex == 0)
			s_SharedCameraIconTex = CreateCameraIconTexture(s_CameraIconSize);
		m_CameraIconTex = s_SharedCameraIconTex;
	}

	void Gizmo2D::EnsureLightIcons()
	{
		if (s_SharedPointLightIconTex == 0)
			s_SharedPointLightIconTex = CreatePointLightIconTexture(s_LightIconSize);
		if (s_SharedSpotLightIconTex == 0)
			s_SharedSpotLightIconTex = CreateSpotLightIconTexture(s_LightIconSize);
		m_PointLightIconTex = s_SharedPointLightIconTex;
		m_SpotLightIconTex = s_SharedSpotLightIconTex;
	}

	unsigned int Gizmo2D::GetSoundIconTextureID()
	{
		if (s_SharedSoundIconTex == 0)
			s_SharedSoundIconTex = CreateSpeakerIconTexture(s_SoundIconSize);
		return s_SharedSoundIconTex;
	}

	unsigned int Gizmo2D::GetParticleIconTextureID()
	{
		if (s_SharedParticleIconTex == 0)
			s_SharedParticleIconTex = CreateParticleIconTexture(s_ParticleIconSize);
		return s_SharedParticleIconTex;
	}

	unsigned int Gizmo2D::GetCameraIconTextureID()
	{
		if (s_SharedCameraIconTex == 0)
			s_SharedCameraIconTex = CreateCameraIconTexture(s_CameraIconSize);
		return s_SharedCameraIconTex;
	}

	unsigned int Gizmo2D::GetPointLightIconTextureID()
	{
		if (s_SharedPointLightIconTex == 0)
			s_SharedPointLightIconTex = CreatePointLightIconTexture(s_LightIconSize);
		return s_SharedPointLightIconTex;
	}

	unsigned int Gizmo2D::GetSpotLightIconTextureID()
	{
		if (s_SharedSpotLightIconTex == 0)
			s_SharedSpotLightIconTex = CreateSpotLightIconTexture(s_LightIconSize);
		return s_SharedSpotLightIconTex;
	}

	unsigned int Gizmo2D::GetScriptIconTextureID()
	{
		if (s_SharedScriptIconTex == 0)
			s_SharedScriptIconTex = CreateScriptIconTexture(s_ScriptIconSize);
		return s_SharedScriptIconTex;
	}

	unsigned int Gizmo2D::GetSaveScriptIconTextureID()
	{
		if (s_SharedSaveScriptIconTex == 0)
			s_SharedSaveScriptIconTex = CreateSaveScriptIconTexture(s_ScriptIconSize);
		return s_SharedSaveScriptIconTex;
	}

	unsigned int Gizmo2D::GetMapIconTextureID()
	{
		if (s_SharedMapIconTex == 0)
			s_SharedMapIconTex = CreateMapIconTexture(s_MapIconSize);
		return s_SharedMapIconTex;
	}

	unsigned int Gizmo2D::GetFolderIconTextureID()
	{
		if (s_SharedFolderIconTex == 0)
			s_SharedFolderIconTex = CreateFolderIconTexture(s_FolderIconSize);
		return s_SharedFolderIconTex;
	}

	int Gizmo2D::GetSoundIconSize()
	{
		return s_SoundIconSize;
	}

	int Gizmo2D::GetParticleIconSize()
	{
		return s_ParticleIconSize;
	}

	int Gizmo2D::GetCameraIconSize()
	{
		return s_CameraIconSize;
	}

	int Gizmo2D::GetLightIconSize()
	{
		return s_LightIconSize;
	}

	int Gizmo2D::GetScriptIconSize()
	{
		return s_ScriptIconSize;
	}

	int Gizmo2D::GetMapIconSize()
	{
		return s_MapIconSize;
	}

	int Gizmo2D::GetFolderIconSize()
	{
		return s_FolderIconSize;
	}

	void Gizmo2D::Draw(const Camera& camera, const GameObject& object)
	{
		CameraProp prop = camera.GetCameraProp();
		float sx = prop.projection[0][0];
		float sy = prop.projection[1][1];
		glm::vec2 pos(object.GetPosition());

		m_Shader.Use();
		m_Shader.SetUniformMatrix4fv(prop.matrix, "matrix");

		if (m_Mode == GizmoMode::Rotate)
		{
			DrawRing(pos, glm::vec2(s_RingRadius * sx, s_RingRadius * sy), s_ColorRing);
			DrawQuad(pos, glm::vec2(4.0f * sx, 4.0f * sy), s_ColorRing);
			return;
		}

		float armCenter = s_CenterHalf + s_ArmLength * 0.5f;
		DrawQuad(glm::vec2(pos.x + armCenter * sx, pos.y),
			glm::vec2(s_ArmLength * 0.5f * sx, s_ArmHalfThickness * sy), s_ColorX);
		DrawQuad(glm::vec2(pos.x, pos.y + armCenter * sy),
			glm::vec2(s_ArmHalfThickness * sx, s_ArmLength * 0.5f * sy), s_ColorY);

		float tip = s_CenterHalf + s_ArmLength;
		if (m_Mode == GizmoMode::Translate)
		{
			DrawArrowHead(glm::vec2(pos.x + (tip + s_HeadHalfLength) * sx, pos.y),
				glm::vec2(s_HeadHalfLength * sx, 0.0f), glm::vec2(0.0f, s_HeadHalfWidth * sy), s_ColorX);
			DrawArrowHead(glm::vec2(pos.x, pos.y + (tip + s_HeadHalfLength) * sy),
				glm::vec2(0.0f, s_HeadHalfLength * sy), glm::vec2(-s_HeadHalfWidth * sx, 0.0f), s_ColorY);
		}
		else
		{
			DrawQuad(glm::vec2(pos.x + (tip + s_ScaleTipHalf) * sx, pos.y),
				glm::vec2(s_ScaleTipHalf * sx, s_ScaleTipHalf * sy), s_ColorX);
			DrawQuad(glm::vec2(pos.x, pos.y + (tip + s_ScaleTipHalf) * sy),
				glm::vec2(s_ScaleTipHalf * sx, s_ScaleTipHalf * sy), s_ColorY);
		}

		DrawQuad(pos, glm::vec2(s_CenterHalf * sx, s_CenterHalf * sy), s_ColorCenter);
	}

	void Gizmo2D::DrawSelection(const Camera& camera, const GameObject& object)
	{
		CameraProp prop = camera.GetCameraProp();
		glm::vec2 mn, mx;
		object.GetWorldAABB(prop, mn, mx);
		const float padX = 4.0f * std::abs(prop.projection[0][0]);
		const float padY = 4.0f * std::abs(prop.projection[1][1]);
		mn.x -= padX; mn.y -= padY;
		mx.x += padX; mx.y += padY;

		m_Shader.Use();
		m_Shader.SetUniformMatrix4fv(prop.matrix, "matrix");

		const glm::vec4 color(1.0f, 0.55f, 0.15f, 0.95f);
		const float t = 1.5f * std::abs(prop.projection[0][0]);
		const float cx = (mn.x + mx.x) * 0.5f;
		const float cy = (mn.y + mx.y) * 0.5f;
		const float hw = (mx.x - mn.x) * 0.5f;
		const float hh = (mx.y - mn.y) * 0.5f;
		DrawQuad(glm::vec2(cx, mn.y), glm::vec2(hw, t), color);
		DrawQuad(glm::vec2(cx, mx.y), glm::vec2(hw, t), color);
		DrawQuad(glm::vec2(mn.x, cy), glm::vec2(t, hh), color);
		DrawQuad(glm::vec2(mx.x, cy), glm::vec2(t, hh), color);
	}

	void Gizmo2D::DrawTilemapGrid(const Camera& camera, const GameObject& tilemap, int hoverX, int hoverY)
	{
		if (!tilemap.IsTilemap())
			return;

		const int cols = tilemap.GetTilemapColumns();
		const int rows = tilemap.GetTilemapRows();
		if (cols <= 0 || rows <= 0)
			return;

		CameraProp prop = camera.GetCameraProp();
		const float sx = prop.projection[0][0];
		const float sy = prop.projection[1][1];
		const float cell = glm::max(tilemap.GetTileCellSize(), 1.0f);
		const float mapW = (float)cols * cell;
		const float mapH = (float)rows * cell;
		const glm::vec2 worldScale = tilemap.GetWorldScale();
		const float worldRot = tilemap.GetRotation();
		const glm::vec3 worldPos = tilemap.GetWorldPosition();

		auto toWorld = [&](float localX, float localY) -> glm::vec2
		{
			glm::vec2 v(localX * worldScale.x, localY * worldScale.y);
			const float c = glm::cos(glm::radians(worldRot));
			const float s = glm::sin(glm::radians(worldRot));
			glm::vec2 r(c * v.x - s * v.y, s * v.x + c * v.y);
			return glm::vec2(worldPos.x + r.x * sx, worldPos.y + r.y * sy);
		};

		m_Shader.Use();
		m_Shader.SetUniformMatrix4fv(prop.matrix, "matrix");

		const glm::vec4 gridColor(0.35f, 0.75f, 1.0f, 0.55f);
		const glm::vec4 hoverColor(1.0f, 0.85f, 0.2f, 0.45f);
		const float lineHalf = 1.0f;

		for (int x = 0; x <= cols; ++x)
		{
			const float lx = (float)x * cell - mapW * 0.5f;
			glm::vec2 a = toWorld(lx, -mapH * 0.5f);
			glm::vec2 b = toWorld(lx, mapH * 0.5f);
			glm::vec2 mid = (a + b) * 0.5f;
			glm::vec2 diff = b - a;
			const float len = glm::length(diff) * 0.5f + lineHalf * sx;
			const float angle = glm::degrees(glm::atan(diff.y, diff.x));
			DrawQuad(mid, glm::vec2(len, lineHalf * sy), gridColor, angle);
		}
		for (int y = 0; y <= rows; ++y)
		{
			const float ly = (float)y * cell - mapH * 0.5f;
			glm::vec2 a = toWorld(-mapW * 0.5f, ly);
			glm::vec2 b = toWorld(mapW * 0.5f, ly);
			glm::vec2 mid = (a + b) * 0.5f;
			glm::vec2 diff = b - a;
			const float len = glm::length(diff) * 0.5f + lineHalf * sx;
			const float angle = glm::degrees(glm::atan(diff.y, diff.x));
			DrawQuad(mid, glm::vec2(len, lineHalf * sy), gridColor, angle);
		}

		if (hoverX >= 0 && hoverY >= 0 && hoverX < cols && hoverY < rows)
		{
			const float lx = ((float)hoverX + 0.5f) * cell - mapW * 0.5f;
			const float ly = ((float)hoverY + 0.5f) * cell - mapH * 0.5f;
			glm::vec2 center = toWorld(lx, ly);
			const float halfCellX = cell * 0.5f * glm::abs(worldScale.x) * sx;
			const float halfCellY = cell * 0.5f * glm::abs(worldScale.y) * sy;
			DrawQuad(center, glm::vec2(halfCellX, halfCellY), hoverColor, worldRot);
		}
	}

	void Gizmo2D::DrawCollision(const Camera& camera, const GameObject& object)
	{
		if (!object.HasCollision())
			return;

		CameraProp prop = camera.GetCameraProp();
		float sx = prop.projection[0][0];
		float sy = prop.projection[1][1];
		glm::vec2 center(object.GetPosition());

		m_Shader.Use();
		m_Shader.SetUniformMatrix4fv(prop.matrix, "matrix");

		ColliderShape shape = object.GetColliderShape();
		if (object.GetCollisionType() == CollisionType::Circle)
			shape = ColliderShape::Circle;

		glm::vec2 thickness(2.5f * sx, 2.5f * sy);
		const glm::vec4& color = object.IsTrigger() ? s_ColorTrigger : s_ColorCollision;

		auto drawSeg = [&](const glm::vec2& a, const glm::vec2& b)
		{
			glm::vec2 delta = b - a;
			float len = glm::length(delta);
			if (len < 1e-4f)
				return;
			glm::vec2 mid = (a + b) * 0.5f;
			float angle = glm::degrees(glm::atan(delta.y, delta.x));
			DrawQuad(mid, glm::vec2(len * 0.5f, thickness.y), color, angle);
		};

		if (shape == ColliderShape::Circle)
		{
			float radius = object.GetCollisionRadius();
			DrawRing(center, glm::vec2(radius * sx, radius * sy), color);
		}
		else if (shape == ColliderShape::Capsule)
		{
			float radius = object.GetCollisionRadius();
			float height = object.GetCapsuleHeight();
			float halfLen = glm::max(height * 0.5f - radius, 0.0f);
			glm::vec2 top(center.x, center.y + halfLen * sy);
			glm::vec2 bot(center.x, center.y - halfLen * sy);
			DrawRing(top, glm::vec2(radius * sx, radius * sy), color);
			DrawRing(bot, glm::vec2(radius * sx, radius * sy), color);
			drawSeg(glm::vec2(top.x - radius * sx, top.y), glm::vec2(bot.x - radius * sx, bot.y));
			drawSeg(glm::vec2(top.x + radius * sx, top.y), glm::vec2(bot.x + radius * sx, bot.y));
		}
		else if (shape == ColliderShape::Polygon)
		{
			const auto& verts = object.GetColliderVertices();
			if (verts.size() >= 3)
			{
				for (size_t i = 0; i < verts.size(); ++i)
				{
					glm::vec2 a = center + glm::vec2(verts[i].x * sx, verts[i].y * sy);
					glm::vec2 b = center + glm::vec2(verts[(i + 1) % verts.size()].x * sx, verts[(i + 1) % verts.size()].y * sy);
					drawSeg(a, b);
				}
			}
		}
		else
		{
			glm::vec2 half = object.GetCollisionSize() * 0.5f;
			half.x *= sx;
			half.y *= sy;

			DrawQuad(glm::vec2(center.x, center.y + half.y), glm::vec2(half.x + thickness.x, thickness.y), color);
			DrawQuad(glm::vec2(center.x, center.y - half.y), glm::vec2(half.x + thickness.x, thickness.y), color);
			DrawQuad(glm::vec2(center.x - half.x, center.y), glm::vec2(thickness.x, half.y + thickness.y), color);
			DrawQuad(glm::vec2(center.x + half.x, center.y), glm::vec2(thickness.x, half.y + thickness.y), color);
		}
	}

	void Gizmo2D::DrawCameraRange(const Camera& camera, const GameObject& cameraObject)
	{
		CameraProp prop = camera.GetCameraProp();
		m_Shader.Use();
		m_Shader.SetUniformMatrix4fv(prop.matrix, "matrix");

		float sx = prop.projection[0][0];
		float sy = prop.projection[1][1];
		glm::vec2 center(cameraObject.GetPosition());
		const float rotation = cameraObject.GetRotation();

		float z = glm::max(cameraObject.GetZoom(), 1e-4f);
		glm::vec2 half(1.0f / z, 1.0f / z);
		glm::vec2 thickness(2.5f * sx, 2.5f * sy);

		const float rad = glm::radians(rotation);
		const float c = glm::cos(rad);
		const float s = glm::sin(rad);
		auto rotateLocal = [&](float lx, float ly)
		{
			return center + glm::vec2(lx * c - ly * s, lx * s + ly * c);
		};

		DrawQuad(rotateLocal(0.0f, half.y), glm::vec2(half.x + thickness.x, thickness.y), s_ColorCameraRange, rotation);
		DrawQuad(rotateLocal(0.0f, -half.y), glm::vec2(half.x + thickness.x, thickness.y), s_ColorCameraRange, rotation);
		DrawQuad(rotateLocal(-half.x, 0.0f), glm::vec2(thickness.x, half.y + thickness.y), s_ColorCameraRange, rotation);
		DrawQuad(rotateLocal(half.x, 0.0f), glm::vec2(thickness.x, half.y + thickness.y), s_ColorCameraRange, rotation);
	}

	void Gizmo2D::DrawCameraIcon(const Camera& camera, const GameObject& cameraObject)
	{
		EnsureCameraIcon();
		if (!m_CameraIconTex)
			return;

		CameraProp prop = camera.GetCameraProp();
		float sx = prop.projection[0][0];
		float sy = prop.projection[1][1];
		glm::vec2 center(cameraObject.GetPosition());

		const float halfPx = 22.0f;
		DrawTexturedIcon(prop.matrix, center, glm::vec2(halfPx * sx, halfPx * sy), m_CameraIconTex, cameraObject.GetRotation());
	}

	void Gizmo2D::DrawSoundIcon(const Camera& camera, const GameObject& soundObject)
	{
		EnsureSoundIcon();
		CameraProp prop = camera.GetCameraProp();
		float sx = prop.projection[0][0];
		float sy = prop.projection[1][1];
		glm::vec2 center(soundObject.GetPosition());

		if (soundObject.GetAudioFalloff() && glm::abs(sx) > 1e-8f && glm::abs(sy) > 1e-8f)
		{
			m_Shader.Use();
			m_Shader.SetUniformMatrix4fv(prop.matrix, "matrix");
			const float minDist = glm::max(soundObject.GetAudioMinDistance(), 0.0f);
			const float maxDist = glm::max(soundObject.GetAudioMaxDistance(), minDist);
			DrawRing(center, glm::vec2(minDist * sx, minDist * sy), glm::vec4(0.35f, 0.85f, 1.0f, 0.7f));
			DrawRing(center, glm::vec2(maxDist * sx, maxDist * sy), glm::vec4(0.2f, 0.45f, 0.9f, 0.45f));
		}

		if (!m_SoundIconTex)
			return;

		const float halfPx = 22.0f;
		DrawTexturedIcon(prop.matrix, center, glm::vec2(halfPx * sx, halfPx * sy), m_SoundIconTex);
	}

	void Gizmo2D::DrawParticleEmitterIcon(const Camera& camera, const GameObject& object)
	{
		EnsureParticleIcon();
		if (!m_ParticleIconTex)
			return;

		CameraProp prop = camera.GetCameraProp();
		float sx = prop.projection[0][0];
		float sy = prop.projection[1][1];
		glm::vec2 center(object.GetWorldPosition());

		const float halfPx = 22.0f;
		DrawTexturedIcon(prop.matrix, center, glm::vec2(halfPx * sx, halfPx * sy), m_ParticleIconTex);
	}

	void Gizmo2D::DrawLightIcon(const Camera& camera, const GameObject& lightObject)
	{
		EnsureLightIcons();
		const bool spot = lightObject.GetLightKind() == LightKind::Spot;
		unsigned int tex = spot ? m_SpotLightIconTex : m_PointLightIconTex;
		if (!tex)
			return;

		CameraProp prop = camera.GetCameraProp();
		float sx = prop.projection[0][0];
		float sy = prop.projection[1][1];
		glm::vec2 center(lightObject.GetWorldPosition());

		glm::vec4 tint = lightObject.GetColor();
		tint.a = lightObject.IsLightEnabled() ? 1.0f : 0.35f;
		// Keep icons readable even on very dark light colors.
		float luma = 0.2126f * tint.r + 0.7152f * tint.g + 0.0722f * tint.b;
		if (luma < 0.25f)
		{
			float boost = 0.25f / glm::max(luma, 0.01f);
			tint.r = glm::min(tint.r * boost, 1.0f);
			tint.g = glm::min(tint.g * boost, 1.0f);
			tint.b = glm::min(tint.b * boost, 1.0f);
		}

		const float halfPx = 22.0f;
		float rotation = spot ? lightObject.GetWorldRotation() : 0.0f;
		DrawTexturedIcon(prop.matrix, center, glm::vec2(halfPx * sx, halfPx * sy), tex, rotation, tint);
	}

	void Gizmo2D::DrawLightGizmo(const Camera& camera, const GameObject& lightObject)
	{
		DrawLightIcon(camera, lightObject);

		CameraProp prop = camera.GetCameraProp();
		float sx = prop.projection[0][0];
		float sy = prop.projection[1][1];
		if (glm::abs(sx) < 1e-8f || glm::abs(sy) < 1e-8f)
			return;

		m_Shader.Use();
		m_Shader.SetUniformMatrix4fv(prop.matrix, "matrix");

		const glm::vec2 center(lightObject.GetWorldPosition());
		const glm::vec4 c = lightObject.GetColor();
		const float alpha = lightObject.IsLightEnabled() ? 0.85f : 0.35f;
		const glm::vec4 ringColor(c.r, c.g, c.b, alpha);
		const float radius = glm::max(lightObject.GetLightRadius(), 1.0f);

		if (lightObject.GetLightKind() == LightKind::Spot)
		{
			const float aimDeg = lightObject.GetWorldRotation();
			const float outer = glm::clamp(lightObject.GetLightOuterAngle(), 0.5f, 179.0f);
			const float inner = glm::clamp(lightObject.GetLightInnerAngle(), 0.5f, outer);
			const glm::vec2 dir = lightObject.GetLightDirection();

			auto edgePoint = [&](float angleDeg)
			{
				const float rad = glm::radians(angleDeg);
				return center + glm::vec2(glm::cos(rad) * radius * sx, glm::sin(rad) * radius * sy);
			};

			const glm::vec2 tip = center + glm::vec2(dir.x * radius * sx, dir.y * radius * sy);
			const glm::vec2 left = edgePoint(aimDeg + outer);
			const glm::vec2 right = edgePoint(aimDeg - outer);
			const glm::vec2 leftInner = edgePoint(aimDeg + inner);
			const glm::vec2 rightInner = edgePoint(aimDeg - inner);

			auto drawSeg = [&](const glm::vec2& a, const glm::vec2& b, const glm::vec4& col, float thicknessPx)
			{
				glm::vec2 delta = b - a;
				float len = glm::length(delta);
				if (len < 1e-4f)
					return;
				glm::vec2 mid = (a + b) * 0.5f;
				float angle = glm::degrees(glm::atan(delta.y, delta.x));
				DrawQuad(mid, glm::vec2(len * 0.5f, thicknessPx * 0.5f * glm::abs(sx)), col, angle);
			};

			drawSeg(center, left, ringColor, 2.0f);
			drawSeg(center, right, ringColor, 2.0f);
			drawSeg(center, tip, glm::vec4(c.r, c.g, c.b, alpha), 2.5f);
			drawSeg(leftInner, rightInner, glm::vec4(1.0f, 1.0f, 0.55f, alpha * 0.65f), 1.5f);
			drawSeg(left, right, glm::vec4(c.r, c.g, c.b, alpha * 0.55f), 1.5f);

			const int segments = 16;
			glm::vec2 prev = left;
			for (int i = 1; i <= segments; ++i)
			{
				float t = (float)i / (float)segments;
				float ang = (aimDeg + outer) + (aimDeg - outer - (aimDeg + outer)) * t;
				glm::vec2 p = edgePoint(ang);
				drawSeg(prev, p, glm::vec4(c.r, c.g, c.b, alpha * 0.65f), 1.5f);
				prev = p;
			}
		}
		else
		{
			DrawRing(center, glm::vec2(radius * sx, radius * sy), ringColor);
		}
	}

	void Gizmo2D::DrawTexturedIcon(const glm::mat4& viewMatrix, const glm::vec2& center, const glm::vec2& halfSize,
		unsigned int textureID, float rotationDegrees, const glm::vec4& tint)
	{
		GLboolean blendWas = glIsEnabled(GL_BLEND);
		GLint srcRGB = 0, dstRGB = 0;
		glGetIntegerv(GL_BLEND_SRC_RGB, &srcRGB);
		glGetIntegerv(GL_BLEND_DST_RGB, &dstRGB);

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDepthMask(GL_FALSE);

		m_IconShader.Use();
		m_IconShader.SetUniformMatrix4fv(viewMatrix, "matrix");
		m_IconShader.SetUniform4fv(tint, "tint");

		glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(center, 0.0f));
		transform = glm::rotate(transform, glm::radians(rotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
		transform = glm::scale(transform, glm::vec3(halfSize, 1.0f));
		m_IconShader.SetUniformMatrix4fv(transform, "transform");
		m_IconShader.SetUniform1i(0, "icon");

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, textureID);
		m_IconVAO.Bind();
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		m_IconVAO.UnBind();
		glBindTexture(GL_TEXTURE_2D, 0);

		glDepthMask(GL_TRUE);
		if (!blendWas)
			glDisable(GL_BLEND);
		glBlendFunc((GLenum)srcRGB, (GLenum)dstRGB);
	}

	GizmoAxis Gizmo2D::HitTest(const CameraProp& prop, const GameObject& object, const glm::vec2& worldPoint) const
	{
		float sx = prop.projection[0][0];
		float sy = prop.projection[1][1];
		glm::vec2 pos(object.GetPosition());

		float dx = (worldPoint.x - pos.x) / sx;
		float dy = (worldPoint.y - pos.y) / sy;

		if (m_Mode == GizmoMode::Rotate)
		{
			float r = glm::sqrt(dx * dx + dy * dy);
			if (glm::abs(r - s_RingRadius) <= s_RingHitBand)
				return GizmoAxis::XY;
			return GizmoAxis::None;
		}

		if (glm::abs(dx) <= s_CenterHalf + s_HitPadding && glm::abs(dy) <= s_CenterHalf + s_HitPadding)
			return GizmoAxis::XY;

		float armEnd = s_CenterHalf + s_ArmLength + 2.0f * s_HeadHalfLength + s_HitPadding;
		float armBand = s_HeadHalfWidth + s_HitPadding;

		if (dx >= s_CenterHalf && dx <= armEnd && glm::abs(dy) <= armBand)
			return GizmoAxis::X;
		if (dy >= s_CenterHalf && dy <= armEnd && glm::abs(dx) <= armBand)
			return GizmoAxis::Y;

		return GizmoAxis::None;
	}

	void Gizmo2D::DrawDebugLine(const Camera& camera, const glm::vec2& a, const glm::vec2& b,
		const glm::vec4& color, float thicknessPx)
	{
		CameraProp prop = camera.GetCameraProp();
		float sx = prop.projection[0][0];
		float sy = prop.projection[1][1];
		if (glm::abs(sx) < 1e-8f) sx = 1.0f;
		if (glm::abs(sy) < 1e-8f) sy = 1.0f;

		m_Shader.Use();
		m_Shader.SetUniformMatrix4fv(prop.matrix, "matrix");

		glm::vec2 delta = b - a;
		float len = glm::length(delta);
		if (len < 1e-4f)
			return;
		glm::vec2 mid = (a + b) * 0.5f;
		float angle = glm::degrees(glm::atan(delta.y, delta.x));
		glm::vec2 half(len * 0.5f, thicknessPx * 0.5f * glm::abs(sx));
		DrawQuad(mid, half, color, angle);
	}

	void Gizmo2D::DrawDebugPoint(const Camera& camera, const glm::vec2& p,
		const glm::vec4& color, float radiusPx)
	{
		CameraProp prop = camera.GetCameraProp();
		float sx = prop.projection[0][0];
		float sy = prop.projection[1][1];
		m_Shader.Use();
		m_Shader.SetUniformMatrix4fv(prop.matrix, "matrix");
		DrawRing(p, glm::vec2(radiusPx * sx, radiusPx * sy), color);
		DrawQuad(p, glm::vec2(radiusPx * 0.35f * sx, radiusPx * 0.35f * sy), color);
	}

	void Gizmo2D::DrawParticle(const Camera& camera, const glm::vec2& center, float size,
		const glm::vec4& color, float rotationDegrees)
	{
		CameraProp prop = camera.GetCameraProp();
		float sx = prop.projection[0][0];
		float sy = prop.projection[1][1];
		m_Shader.Use();
		m_Shader.SetUniformMatrix4fv(prop.matrix, "matrix");
		// `size` is in mesh/pixel units (same as collision / debug draw).
		const float half = glm::max(size * 0.5f, 4.0f);
		DrawQuad(center, glm::vec2(half * sx, half * sy), color, rotationDegrees);
	}

	void Gizmo2D::DrawQuad(const glm::vec2& center, const glm::vec2& halfSize, const glm::vec4& color, float rotationDegrees)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(center, 0.0f));
		if (glm::abs(rotationDegrees) > 1e-4f)
			transform = glm::rotate(transform, glm::radians(rotationDegrees), glm::vec3(0.0f, 0.0f, 1.0f));
		transform = glm::scale(transform, glm::vec3(halfSize, 1.0f));

		m_Shader.SetUniformMatrix4fv(transform, "transform");
		m_Shader.SetUniform4fv(color, "color");
		m_QuadVAO.Bind();
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		m_QuadVAO.UnBind();
	}

	void Gizmo2D::DrawArrowHead(const glm::vec2& center, const glm::vec2& axis0, const glm::vec2& axis1, const glm::vec4& color)
	{
		glm::mat4 transform(1.0f);
		transform[0] = glm::vec4(axis0, 0.0f, 0.0f);
		transform[1] = glm::vec4(axis1, 0.0f, 0.0f);
		transform[3] = glm::vec4(center, 0.0f, 1.0f);

		m_Shader.SetUniformMatrix4fv(transform, "transform");
		m_Shader.SetUniform4fv(color, "color");
		m_TriVAO.Bind();
		glDrawArrays(GL_TRIANGLES, 0, 3);
		m_TriVAO.UnBind();
	}

	void Gizmo2D::DrawRing(const glm::vec2& center, const glm::vec2& radius, const glm::vec4& color)
	{
		glm::mat4 transform = glm::translate(glm::mat4(1.0f), glm::vec3(center, 0.0f));
		transform = glm::scale(transform, glm::vec3(radius, 1.0f));

		m_Shader.SetUniformMatrix4fv(transform, "transform");
		m_Shader.SetUniform4fv(color, "color");
		m_RingVAO.Bind();
		glDrawArrays(GL_TRIANGLE_STRIP, 0, m_RingVertexCount);
		m_RingVAO.UnBind();
	}
}
