#include "UISystem.h"
#include "Blaze/Renderer/Camera.h"
#include "Blaze/Renderer/Shader.h"
#include "Blaze/Renderer/Texture.h"
#include "Blaze/Renderer/VertexArray.h"
#include "Blaze/Renderer/VertexBuffer.h"
#include "Blaze/Scripting/ScriptEngine.h"
#include "Blaze/Logging/Log.h"
#include <glad/glad.h>
#include <gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <unordered_map>
#include <vector>
#include <cstring>
#include <filesystem>

namespace fs = std::filesystem;

#define STB_TRUETYPE_IMPLEMENTATION
#include "../../../vendor/imgui/src/imstb_truetype.h"

namespace Blaze
{
	namespace
	{
		struct GlyphInfo
		{
			float advance = 0.0f;
			float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
			float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
		};

		struct ShownWidget
		{
			std::shared_ptr<WidgetAsset> asset;
			std::string path;
		};

		struct HitTarget
		{
			std::string widgetPath;
			std::string buttonName;
			UIRect rect;
		};

		static bool s_Ready = false;
		static Shader* s_Shader = nullptr;
		static VertexArray* s_VAO = nullptr;
		static VertexBuffer* s_VBO = nullptr;
		static unsigned int s_EBO = 0;
		static unsigned int s_WhiteTex = 0;
		static const int s_AtlasW = 512;
		static const int s_AtlasH = 512;
		static const float s_BakeSize = 32.0f;

		struct FontAtlas
		{
			unsigned int tex = 0;
			GlyphInfo glyphs[96] = {};
			float bakeSize = s_BakeSize;
			bool valid = false;
		};

		static FontAtlas s_DefaultFont;
		static std::unordered_map<std::string, FontAtlas> s_FontCache;
		static std::vector<ShownWidget> s_Shown;
		static std::string s_PressedWidget;
		static std::string s_PressedButton;
		static std::string s_HoveredWidget;
		static std::string s_HoveredButton;
		static std::unordered_map<std::string, std::unique_ptr<Texture>> s_TextureCache;

		static unsigned int GetCachedTexture(const std::string& path)
		{
			if (path.empty())
				return 0;
			std::string norm = path;
			for (char& c : norm)
			{
				if (c == '\\')
					c = '/';
			}
			auto it = s_TextureCache.find(norm);
			if (it != s_TextureCache.end())
				return it->second && it->second->IsValid() ? it->second->GetTextID() : 0;

			auto tex = std::make_unique<Texture>();
			tex->Generate(norm);
			const unsigned int id = tex->IsValid() ? tex->GetTextID() : 0;
			s_TextureCache[norm] = std::move(tex);
			return id;
		}

		static void ClearTextureCache()
		{
			s_TextureCache.clear();
		}

		static void DestroyFontAtlas(FontAtlas& atlas)
		{
			if (atlas.tex)
			{
				glDeleteTextures(1, &atlas.tex);
				atlas.tex = 0;
			}
			atlas.valid = false;
		}

		static void ClearFontCache()
		{
			for (auto& entry : s_FontCache)
				DestroyFontAtlas(entry.second);
			s_FontCache.clear();
			DestroyFontAtlas(s_DefaultFont);
		}

		static const char* s_Vert = R"(#version 330 core
layout (location = 0) in vec3 pos;
layout (location = 1) in vec2 texCoord;
uniform mat4 matrix;
uniform mat4 ortho;
uniform mat4 object_matrix;
uniform vec2 uv_scale;
uniform vec2 uv_offset;
out vec2 TexCoord;
void main()
{
	gl_Position = matrix * object_matrix * ortho * vec4(pos, 1.0);
	TexCoord = texCoord * uv_scale + uv_offset;
}
)";

		static const char* s_Frag = R"(#version 330 core
uniform vec4 color;
in vec2 TexCoord;
uniform sampler2D in_texture;
out vec4 FragColor;
void main()
{
	vec4 tex = texture(in_texture, TexCoord);
	FragColor = color * tex;
}
)";

		static std::string NormalizePath(std::string path)
		{
			for (char& c : path)
			{
				if (c == '\\')
					c = '/';
			}
			return path;
		}

		static std::vector<unsigned char> LoadFontFile(const std::string& path)
		{
			std::ifstream file(path, std::ios::binary | std::ios::ate);
			if (!file)
				return {};
			const std::streamsize size = file.tellg();
			if (size <= 0)
				return {};
			file.seekg(0, std::ios::beg);
			std::vector<unsigned char> data((size_t)size);
			if (!file.read((char*)data.data(), size))
				return {};
			return data;
		}

		static bool BakeFontAtlas(FontAtlas& out, const std::vector<unsigned char>& ttf, const std::string& label)
		{
			if (ttf.empty())
				return false;

			stbtt_fontinfo info;
			if (!stbtt_InitFont(&info, ttf.data(), 0))
				return false;

			std::vector<unsigned char> atlas((size_t)s_AtlasW * (size_t)s_AtlasH, 0);
			stbtt_packedchar packed[96];
			stbtt_pack_context pack;
			if (!stbtt_PackBegin(&pack, atlas.data(), s_AtlasW, s_AtlasH, 0, 1, nullptr))
				return false;
			stbtt_PackSetOversampling(&pack, 1, 1);
			if (!stbtt_PackFontRange(&pack, ttf.data(), 0, s_BakeSize, 32, 96, packed))
			{
				stbtt_PackEnd(&pack);
				return false;
			}
			stbtt_PackEnd(&pack);

			DestroyFontAtlas(out);
			out.bakeSize = s_BakeSize;
			for (int i = 0; i < 96; ++i)
			{
				const stbtt_packedchar& pc = packed[i];
				GlyphInfo& g = out.glyphs[i];
				g.advance = pc.xadvance;
				g.x0 = (float)pc.xoff;
				g.y0 = (float)pc.yoff;
				g.x1 = (float)pc.xoff2;
				g.y1 = (float)pc.yoff2;
				g.u0 = pc.x0 / (float)s_AtlasW;
				g.v0 = pc.y0 / (float)s_AtlasH;
				g.u1 = pc.x1 / (float)s_AtlasW;
				g.v1 = pc.y1 / (float)s_AtlasH;
			}

			std::vector<unsigned char> rgba((size_t)s_AtlasW * (size_t)s_AtlasH * 4);
			for (int i = 0; i < s_AtlasW * s_AtlasH; ++i)
			{
				const unsigned char a = atlas[i];
				rgba[i * 4 + 0] = 255;
				rgba[i * 4 + 1] = 255;
				rgba[i * 4 + 2] = 255;
				rgba[i * 4 + 3] = a;
			}

			glGenTextures(1, &out.tex);
			glBindTexture(GL_TEXTURE_2D, out.tex);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s_AtlasW, s_AtlasH, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
			glBindTexture(GL_TEXTURE_2D, 0);
			out.valid = out.tex != 0;
			if (out.valid && !label.empty())
				BZ_INFO("UI font: {0}", label);
			return out.valid;
		}

		static bool BakeDefaultFont()
		{
			const char* candidates[] = {
				"C:/Windows/Fonts/arial.ttf",
				"C:/Windows/Fonts/segoeui.ttf",
				"C:/Windows/Fonts/tahoma.ttf",
			};

			for (const char* path : candidates)
			{
				std::vector<unsigned char> ttf = LoadFontFile(path);
				if (!ttf.empty() && BakeFontAtlas(s_DefaultFont, ttf, path))
					return true;
			}
			return false;
		}

		static FontAtlas* GetFontAtlas(const std::string& fontPath)
		{
			if (fontPath.empty())
				return s_DefaultFont.valid ? &s_DefaultFont : nullptr;

			const std::string norm = NormalizePath(fontPath);
			auto it = s_FontCache.find(norm);
			if (it != s_FontCache.end())
				return it->second.valid ? &it->second : (s_DefaultFont.valid ? &s_DefaultFont : nullptr);

			FontAtlas atlas;
			std::vector<unsigned char> ttf = LoadFontFile(norm);
			if (ttf.empty())
			{
				// Try common project-relative layouts if an absolute path failed / cwd differs.
				const std::string candidates[] = {
					norm,
					std::string("content/") + fs::path(norm).filename().string(),
				};
				for (const std::string& c : candidates)
				{
					ttf = LoadFontFile(c);
					if (!ttf.empty())
						break;
				}
			}
			if (!BakeFontAtlas(atlas, ttf, norm))
			{
				BZ_INFO("Failed to load font '{0}' — using default", norm);
				s_FontCache[norm] = FontAtlas{};
				return s_DefaultFont.valid ? &s_DefaultFont : nullptr;
			}
			s_FontCache[norm] = std::move(atlas);
			return &s_FontCache[norm];
		}

		static void DrawTexturedQuad(float x, float y, float w, float h, const glm::vec4& color,
			unsigned int texture, float u0, float v0, float u1, float v1)
		{
			if (w <= 0.0f || h <= 0.0f || !s_Shader || !s_VAO)
				return;

			glm::mat4 object(1.0f);
			object = glm::translate(object, glm::vec3(x, y, 0.0f));
			object = glm::scale(object, glm::vec3(w, h, 1.0f));

			s_Shader->SetUniform4fv(color, "color");
			s_Shader->SetUniformMatrix4fv(object, "object_matrix");
			s_Shader->SetUniform2fv(glm::vec2(u1 - u0, v1 - v0), "uv_scale");
			s_Shader->SetUniform2fv(glm::vec2(u0, v0), "uv_offset");

			glBindTexture(GL_TEXTURE_2D, texture);
			s_VAO->Bind();
			glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
			s_VAO->UnBind();
			glBindTexture(GL_TEXTURE_2D, 0);
		}

		static void DrawSolidQuad(float x, float y, float w, float h, const glm::vec4& color)
		{
			DrawTexturedQuad(x, y, w, h, color, s_WhiteTex, 0.0f, 0.0f, 1.0f, 1.0f);
		}

		static void DrawString(const std::string& text, float x, float y, float fontSize, const glm::vec4& color,
			const std::string& fontPath = {})
		{
			FontAtlas* font = GetFontAtlas(fontPath);
			if (!font || !font->valid || text.empty())
				return;
			const float scale = fontSize / font->bakeSize;
			float cursor = x;
			for (unsigned char ch : text)
			{
				if (ch < 32 || ch > 127)
					ch = '?';
				const GlyphInfo& g = font->glyphs[ch - 32];
				const float x0 = cursor + g.x0 * scale;
				const float y0 = y + g.y0 * scale;
				const float x1 = cursor + g.x1 * scale;
				const float y1 = y + g.y1 * scale;
				DrawTexturedQuad(x0, y0, x1 - x0, y1 - y0, color, font->tex, g.u0, g.v0, g.u1, g.v1);
				cursor += g.advance * scale;
			}
		}

		static void CollectHits(const WidgetNode& node, const UIRect& parent, const std::string& widgetPath,
			std::vector<HitTarget>& hits)
		{
			if (!node.visible)
				return;
			const UIRect rect = UISystem::ResolveRect(node, parent);
			if (node.type == WidgetType::Button)
				hits.push_back({ widgetPath, node.name, rect });
			for (const WidgetNode& child : node.children)
				CollectHits(child, rect, widgetPath, hits);
		}

		static void DrawNode(const WidgetNode& node, const UIRect& parent,
			const std::string& widgetPath, bool interactive)
		{
			if (!node.visible)
				return;

			const UIRect rect = UISystem::ResolveRect(node, parent);

			if (node.type == WidgetType::Button)
			{
				glm::vec4 color = node.color;
				const bool hovered = interactive
					&& s_HoveredWidget == widgetPath && s_HoveredButton == node.name;
				const bool pressed = interactive
					&& s_PressedWidget == widgetPath && s_PressedButton == node.name;
				if (pressed)
					color *= glm::vec4(0.75f, 0.75f, 0.75f, 1.0f);
				else if (hovered)
					color *= glm::vec4(1.15f, 1.15f, 1.15f, 1.0f);
				color = glm::clamp(color, 0.0f, 1.0f);
				DrawSolidQuad(rect.x, rect.y, rect.w, rect.h, color);

				if (!node.text.empty())
				{
					const float tw = UISystem::MeasureTextWidth(node.text, node.fontSize, node.fontPath);
					const float tx = rect.x + (rect.w - tw) * 0.5f;
					const float ty = rect.y + rect.h * 0.5f + node.fontSize * 0.35f;
					DrawString(node.text, tx, ty, node.fontSize, glm::vec4(1.0f), node.fontPath);
				}
			}
			else if (node.type == WidgetType::Text)
			{
				DrawString(node.text, rect.x, rect.y + node.fontSize, node.fontSize, node.color, node.fontPath);
			}
			else if (node.type == WidgetType::Image)
			{
				const unsigned int tex = GetCachedTexture(node.texturePath);
				if (tex != 0)
					DrawTexturedQuad(rect.x, rect.y, rect.w, rect.h, node.color, tex, 0.0f, 0.0f, 1.0f, 1.0f);
				else
					DrawSolidQuad(rect.x, rect.y, rect.w, rect.h, glm::vec4(0.35f, 0.15f, 0.35f, node.color.a));
			}

			for (const WidgetNode& child : node.children)
				DrawNode(child, rect, widgetPath, interactive);
		}

		static void BeginUIPass(int framebufferW, int framebufferH)
		{
			const glm::mat4 identity(1.0f);
			const glm::mat4 pixelOrtho = glm::ortho(0.0f, (float)framebufferW, (float)framebufferH, 0.0f, -1.0f, 1.0f);
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_DEPTH_TEST);
			s_Shader->Use();
			s_Shader->SetUniformMatrix4fv(pixelOrtho, "matrix");
			s_Shader->SetUniformMatrix4fv(identity, "ortho");
		}

		// Layout in design pixels, uniformly scaled to fit the framebuffer (letterboxed).
		static void BeginUIPassDesign(int framebufferW, int framebufferH,
			float designW, float designH, float& outScale, float& outOx, float& outOy)
		{
			designW = glm::max(designW, 64.0f);
			designH = glm::max(designH, 64.0f);
			outScale = glm::min((float)framebufferW / designW, (float)framebufferH / designH);
			outOx = ((float)framebufferW - designW * outScale) * 0.5f;
			outOy = ((float)framebufferH - designH * outScale) * 0.5f;

			const glm::mat4 identity(1.0f);
			const glm::mat4 pixelOrtho = glm::ortho(0.0f, (float)framebufferW, (float)framebufferH, 0.0f, -1.0f, 1.0f);
			glm::mat4 fit(1.0f);
			fit = glm::translate(fit, glm::vec3(outOx, outOy, 0.0f));
			fit = glm::scale(fit, glm::vec3(outScale, outScale, 1.0f));

			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glDisable(GL_DEPTH_TEST);
			s_Shader->Use();
			s_Shader->SetUniformMatrix4fv(pixelOrtho * fit, "matrix");
			s_Shader->SetUniformMatrix4fv(identity, "ortho");
		}

		static void ApplyDesignScissor(int framebufferW, int framebufferH,
			float designW, float designH, float scale, float ox, float oy)
		{
			const int x = (int)std::floor(ox);
			const int w = (int)std::ceil(designW * scale);
			const int h = (int)std::ceil(designH * scale);
			const int y = (int)std::floor((float)framebufferH - oy - designH * scale);
			glEnable(GL_SCISSOR_TEST);
			glScissor(x, y, glm::max(w, 0), glm::max(h, 0));
		}

		static ShownWidget* FindShown(const std::string& path)
		{
			const std::string norm = NormalizePath(path);
			for (ShownWidget& shown : s_Shown)
			{
				if (NormalizePath(shown.path) == norm)
					return &shown;
			}
			return nullptr;
		}
	}

	void UISystem::Init()
	{
		if (s_Ready)
			return;

		s_Shader = new Shader();
		s_VAO = new VertexArray();
		s_VBO = new VertexBuffer();
		s_Shader->CreateProgram(s_Vert, s_Frag);
		s_Shader->LinkProgram();

		const float verts[] = {
			0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
			1.0f, 0.0f, 0.0f, 1.0f, 0.0f,
			1.0f, 1.0f, 0.0f, 1.0f, 1.0f,
			0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
		};
		const unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

		glGenBuffers(1, &s_EBO);
		s_VAO->Bind();
		s_VBO->Bind();
		s_VBO->SetFloat(verts, sizeof(verts), 5);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
		s_VAO->AttribPointer(*s_VBO, 3, 0);
		s_VAO->AttribPointer(*s_VBO, 2, 3);
		s_VBO->UnBind();
		s_VAO->UnBind();

		const unsigned char white[] = { 255, 255, 255, 255 };
		glGenTextures(1, &s_WhiteTex);
		glBindTexture(GL_TEXTURE_2D, s_WhiteTex);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
		glBindTexture(GL_TEXTURE_2D, 0);

		if (!BakeDefaultFont())
			BZ_INFO("UISystem: text rendering disabled (no font)");

		s_Ready = true;
	}

	void UISystem::Shutdown()
	{
		HideAll();
		ClearTextureCache();
		ClearFontCache();
		if (s_WhiteTex) { glDeleteTextures(1, &s_WhiteTex); s_WhiteTex = 0; }
		if (s_EBO) { glDeleteBuffers(1, &s_EBO); s_EBO = 0; }
		delete s_VBO; s_VBO = nullptr;
		delete s_VAO; s_VAO = nullptr;
		delete s_Shader; s_Shader = nullptr;
		s_PressedWidget.clear();
		s_PressedButton.clear();
		s_HoveredWidget.clear();
		s_HoveredButton.clear();
		s_Ready = false;
	}

	bool UISystem::IsReady() { return s_Ready; }

	bool UISystem::ShowWidget(const std::string& path)
	{
		const std::string norm = NormalizePath(path);
		if (FindShown(norm))
			return true;

		auto asset = std::make_shared<WidgetAsset>();
		if (!asset->LoadFromFile(path))
			return false;
		asset->SetPath(norm);
		s_Shown.push_back({ asset, norm });
		BZ_INFO("ShowWidget: {0}", norm);
		return true;
	}

	bool UISystem::ShowWidget(const std::shared_ptr<WidgetAsset>& asset, const std::string& id)
	{
		if (!asset)
			return false;
		const std::string norm = NormalizePath(id.empty() ? asset->GetPath() : id);
		if (norm.empty())
			return false;

		if (ShownWidget* existing = FindShown(norm))
		{
			existing->asset = asset;
			asset->SetPath(norm);
			return true;
		}

		asset->SetPath(norm);
		s_Shown.push_back({ asset, norm });
		BZ_INFO("ShowWidget: {0}", norm);
		return true;
	}

	void UISystem::HideWidget(const std::string& path)
	{
		const std::string norm = NormalizePath(path);
		s_Shown.erase(std::remove_if(s_Shown.begin(), s_Shown.end(),
			[&](const ShownWidget& w) { return NormalizePath(w.path) == norm; }), s_Shown.end());
	}

	void UISystem::HideAll()
	{
		s_Shown.clear();
		s_PressedWidget.clear();
		s_PressedButton.clear();
		s_HoveredWidget.clear();
		s_HoveredButton.clear();
	}

	bool UISystem::IsWidgetShown(const std::string& path)
	{
		return FindShown(path) != nullptr;
	}

	bool UISystem::SetWidgetText(const std::string& widgetPath, const std::string& nodeName, const std::string& text)
	{
		ShownWidget* shown = FindShown(widgetPath);
		if (!shown || !shown->asset)
			return false;
		WidgetNode* node = WidgetAsset::FindByName(shown->asset->GetRoot(), nodeName);
		if (!node)
			return false;
		node->text = text;
		return true;
	}

	std::string UISystem::GetWidgetText(const std::string& widgetPath, const std::string& nodeName)
	{
		ShownWidget* shown = FindShown(widgetPath);
		if (!shown || !shown->asset)
			return {};
		WidgetNode* node = WidgetAsset::FindByName(shown->asset->GetRoot(), nodeName);
		return node ? node->text : std::string();
	}

	std::shared_ptr<WidgetAsset> UISystem::GetShownAsset(const std::string& id)
	{
		ShownWidget* shown = FindShown(id);
		return shown ? shown->asset : nullptr;
	}

	UIRect UISystem::ResolveRect(const WidgetNode& node, const UIRect& parent)
	{
		if (node.type == WidgetType::Canvas)
			return parent;

		const float ax0 = parent.x + node.anchorMin.x * parent.w;
		const float ay0 = parent.y + node.anchorMin.y * parent.h;
		const float ax1 = parent.x + node.anchorMax.x * parent.w;
		const float ay1 = parent.y + node.anchorMax.y * parent.h;
		const bool stretchX = std::abs(node.anchorMax.x - node.anchorMin.x) > 1e-4f;
		const bool stretchY = std::abs(node.anchorMax.y - node.anchorMin.y) > 1e-4f;

		float w = stretchX ? (ax1 - ax0) : node.size.x;
		float h = stretchY ? (ay1 - ay0) : node.size.y;
		if (node.type == WidgetType::Text && w < 1.0f)
			w = MeasureTextWidth(node.text, node.fontSize, node.fontPath);
		if (node.type == WidgetType::Text && h < 1.0f)
			h = node.fontSize * 1.2f;
		w = glm::max(w, 0.0f);
		h = glm::max(h, 0.0f);

		float x = ax0 + node.position.x - node.pivot.x * w;
		float y = ay0 + node.position.y - node.pivot.y * h;
		if (stretchX)
			x = ax0 + node.position.x;
		if (stretchY)
			y = ay0 + node.position.y;
		return UIRect{ x, y, w, h };
	}

	float UISystem::MeasureTextWidth(const std::string& text, float fontSize, const std::string& fontPath)
	{
		FontAtlas* font = GetFontAtlas(fontPath);
		if (!font || !font->valid || text.empty())
			return (float)text.size() * fontSize * 0.5f;
		const float scale = fontSize / font->bakeSize;
		float w = 0.0f;
		for (unsigned char ch : text)
		{
			if (ch < 32 || ch > 127)
				ch = '?';
			w += font->glyphs[ch - 32].advance * scale;
		}
		return w;
	}

	bool UISystem::ResolveFont(const std::string& fontPath, unsigned int& outTextureId, float& outBakeSize,
		const FontGlyph*& outGlyphs)
	{
		outTextureId = 0;
		outBakeSize = s_BakeSize;
		outGlyphs = nullptr;
		if (!s_Ready)
			Init();
		FontAtlas* font = GetFontAtlas(fontPath);
		if (!font || !font->valid)
			return false;
		outTextureId = font->tex;
		outBakeSize = font->bakeSize;
		outGlyphs = reinterpret_cast<const FontGlyph*>(font->glyphs);
		static_assert(sizeof(FontGlyph) == sizeof(GlyphInfo), "FontGlyph must match GlyphInfo layout");
		return true;
	}

	void UISystem::Draw(int framebufferW, int framebufferH)
	{
		if (!s_Ready || framebufferW <= 0 || framebufferH <= 0)
			return;

		for (const ShownWidget& shown : s_Shown)
		{
			if (!shown.asset)
				continue;

			const glm::vec2 design = shown.asset->GetDesignSize();
			float scale = 1.0f, ox = 0.0f, oy = 0.0f;
			BeginUIPassDesign(framebufferW, framebufferH, design.x, design.y, scale, ox, oy);
			ApplyDesignScissor(framebufferW, framebufferH, design.x, design.y, scale, ox, oy);

			const UIRect screen{ 0.0f, 0.0f, design.x, design.y };
			DrawNode(shown.asset->GetRoot(), screen, shown.path, true);

			glDisable(GL_SCISSOR_TEST);
		}
	}

	void UISystem::DrawAssetPreview(const WidgetAsset& asset, int framebufferW, int framebufferH)
	{
		if (!s_Ready || framebufferW <= 0 || framebufferH <= 0)
			return;

		const glm::vec2 design = asset.GetDesignSize();
		float scale = 1.0f, ox = 0.0f, oy = 0.0f;
		BeginUIPassDesign(framebufferW, framebufferH, design.x, design.y, scale, ox, oy);
		ApplyDesignScissor(framebufferW, framebufferH, design.x, design.y, scale, ox, oy);

		const UIRect screen{ 0.0f, 0.0f, design.x, design.y };
		DrawNode(asset.GetRoot(), screen, asset.GetPath(), false);
		glDisable(GL_SCISSOR_TEST);
	}

	void UISystem::UpdateInput(int framebufferW, int framebufferH,
		float mouseGameX, float mouseGameY, bool mouseDown, bool mousePressed, bool mouseReleased)
	{
		if (!s_Ready)
			return;

		s_HoveredWidget.clear();
		s_HoveredButton.clear();

		// Topmost shown widget wins (last in list).
		for (auto it = s_Shown.rbegin(); it != s_Shown.rend(); ++it)
		{
			if (!it->asset)
				continue;

			const glm::vec2 design = it->asset->GetDesignSize();
			const float designW = glm::max(design.x, 64.0f);
			const float designH = glm::max(design.y, 64.0f);
			const float scale = glm::min((float)framebufferW / designW, (float)framebufferH / designH);
			const float ox = ((float)framebufferW - designW * scale) * 0.5f;
			const float oy = ((float)framebufferH - designH * scale) * 0.5f;
			const float designMouseX = (mouseGameX - ox) / glm::max(scale, 1e-6f);
			const float designMouseY = (mouseGameY - oy) / glm::max(scale, 1e-6f);

			std::vector<HitTarget> localHits;
			const UIRect screen{ 0.0f, 0.0f, designW, designH };
			CollectHits(it->asset->GetRoot(), screen, it->path, localHits);

			for (auto hit = localHits.rbegin(); hit != localHits.rend(); ++hit)
			{
				if (hit->rect.Contains(designMouseX, designMouseY))
				{
					s_HoveredWidget = hit->widgetPath;
					s_HoveredButton = hit->buttonName;
					break;
				}
			}
			if (!s_HoveredButton.empty())
				break;
		}

		if (mousePressed && !s_HoveredButton.empty())
		{
			s_PressedWidget = s_HoveredWidget;
			s_PressedButton = s_HoveredButton;
			BZ_INFO("UI button pressed: {0} / {1}", s_PressedWidget, s_PressedButton);
			ScriptEngine::NotifyWidgetClicked(s_PressedWidget, s_PressedButton);
		}

		if (mouseReleased || !mouseDown)
		{
			s_PressedWidget.clear();
			s_PressedButton.clear();
		}
	}

	void UISystem::DrawWorldText(Shader& shader, const CameraProp& prop, const glm::mat4& objectMatrix,
		VertexArray& meshVAO, const std::string& text, float fontSize, const glm::vec4& color,
		const std::string& fontPath)
	{
		FontAtlas* font = GetFontAtlas(fontPath);
		if (!s_Ready || !font || !font->valid || text.empty() || fontSize <= 0.0f)
			return;

		const float sx = prop.projection[0][0];
		const float sy = prop.projection[1][1];
		if (glm::abs(sx) < 1e-8f || glm::abs(sy) < 1e-8f)
			return;

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable(GL_DEPTH_TEST);

		shader.Use();
		shader.SetUniform4fv(color, "color");

		const float scale = fontSize / font->bakeSize;
		const float totalW = MeasureTextWidth(text, fontSize, fontPath);
		float cursor = -totalW * 0.5f;
		const float baseline = 0.0f;

		glBindTexture(GL_TEXTURE_2D, font->tex);
		meshVAO.Bind();

		for (unsigned char ch : text)
		{
			if (ch < 32 || ch > 127)
				ch = '?';
			const GlyphInfo& g = font->glyphs[ch - 32];
			// Mesh space is Y-up; stb packed chars are Y-down — flip vertical.
			const float x0 = cursor + g.x0 * scale;
			const float y0 = baseline - g.y1 * scale;
			const float x1 = cursor + g.x1 * scale;
			const float y1 = baseline - g.y0 * scale;
			const float w = x1 - x0;
			const float h = y1 - y0;
			if (w > 0.0f && h > 0.0f)
			{
				// Default mesh is 100x100 (±50). Same placement trick as tilemap cells.
				const float cx = (x0 + x1) * 0.5f;
				const float cy = (y0 + y1) * 0.5f;
				glm::mat4 local(1.0f);
				local = glm::translate(local, glm::vec3(cx * sx, cy * sy, 0.0f));
				local = glm::scale(local, glm::vec3(w / 100.0f, h / 100.0f, 1.0f));

				shader.SetUniformMatrix4fv(objectMatrix * local, "object_matrix");
				// Mesh V=0 is bottom; stb atlas V grows downward — flip V like tilesets.
				shader.SetUniform2fv(glm::vec2(g.u1 - g.u0, g.v0 - g.v1), "uv_scale");
				shader.SetUniform2fv(glm::vec2(g.u0, g.v1), "uv_offset");
				glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
			}
			cursor += g.advance * scale;
		}

		meshVAO.UnBind();
		glBindTexture(GL_TEXTURE_2D, 0);
	}
}
