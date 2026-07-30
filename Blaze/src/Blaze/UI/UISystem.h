#pragma once
#include "Blaze/Core/Core.h"
#include "Blaze/UI/WidgetAsset.h"
#include "glm.hpp"
#include <memory>
#include <string>
#include <vector>

namespace Blaze
{
	class Shader;
	class VertexArray;
	struct CameraProp;

	struct UIRect
	{
		float x = 0.0f;
		float y = 0.0f;
		float w = 0.0f;
		float h = 0.0f;

		bool Contains(float px, float py) const
		{
			return px >= x && py >= y && px <= x + w && py <= y + h;
		}
	};

	class BLAZE_API UISystem
	{
	public:
		static void Init();
		static void Shutdown();
		static bool IsReady();

		static bool ShowWidget(const std::string& path);
		static bool ShowWidget(const std::shared_ptr<WidgetAsset>& asset, const std::string& id);
		static void HideWidget(const std::string& id);
		static void HideAll();
		static bool IsWidgetShown(const std::string& id);
		static bool SetWidgetText(const std::string& widgetId, const std::string& nodeName, const std::string& text);
		static std::string GetWidgetText(const std::string& widgetId, const std::string& nodeName);
		static std::shared_ptr<WidgetAsset> GetShownAsset(const std::string& id);

		static void Draw(int framebufferW, int framebufferH);
		static void UpdateInput(int framebufferW, int framebufferH,
			float mouseGameX, float mouseGameY, bool mouseDown, bool mousePressed, bool mouseReleased);

		static UIRect ResolveRect(const WidgetNode& node, const UIRect& parent);
		static float MeasureTextWidth(const std::string& text, float fontSize, const std::string& fontPath = {});

		
		struct FontGlyph
		{
			float advance = 0.0f;
			float x0 = 0.0f, y0 = 0.0f, x1 = 0.0f, y1 = 0.0f;
			float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
		};
		static bool ResolveFont(const std::string& fontPath, unsigned int& outTextureId, float& outBakeSize,
			const FontGlyph*& outGlyphs);
		static void DrawAssetPreview(const WidgetAsset& asset, int framebufferW, int framebufferH);
		static void DrawWorldText(Shader& shader, const CameraProp& prop, const glm::mat4& objectMatrix,
			VertexArray& meshVAO, const std::string& text, float fontSize, const glm::vec4& color,
			const std::string& fontPath = {});
	};
}
