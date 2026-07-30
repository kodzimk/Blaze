#pragma once
#include "Blaze/Core/Core.h"
#include "glm.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace Blaze
{
	enum class WidgetType : int
	{
		Canvas = 0,
		Text = 1,
		Button = 2,
		Image = 3
	};

	struct BLAZE_API WidgetNode
	{
		std::string name = "Widget";
		WidgetType type = WidgetType::Canvas;
		std::string text;
		std::string texturePath;
		std::string fontPath;
		float fontSize = 24.0f;
		bool visible = true;
		glm::vec2 anchorMin = glm::vec2(0.5f);
		glm::vec2 anchorMax = glm::vec2(0.5f);
		glm::vec2 pivot = glm::vec2(0.5f);
		glm::vec2 position = glm::vec2(0.0f); // pixel offset
		glm::vec2 size = glm::vec2(160.0f, 40.0f);
		glm::vec4 color = glm::vec4(1.0f);
		std::vector<WidgetNode> children;

		static const char* TypeName(WidgetType type);
		static WidgetType TypeFromName(const std::string& name);
		void InitDefaults();
	};

	class BLAZE_API WidgetAsset
	{
	public:
		WidgetAsset() = default;

		WidgetNode& GetRoot() { return m_Root; }
		const WidgetNode& GetRoot() const { return m_Root; }
		const std::string& GetPath() const { return m_Path; }
		void SetPath(const std::string& path) { m_Path = path; }

		glm::vec2 GetDesignSize() const { return m_DesignSize; }
		void SetDesignSize(float w, float h)
		{
			m_DesignSize.x = glm::max(w, 64.0f);
			m_DesignSize.y = glm::max(h, 64.0f);
		}

		bool LoadFromFile(const std::string& path);
		bool SaveToFile(const std::string& path) const;

		static WidgetAsset CreateDefault(const std::string& name = "Widget");
		static WidgetAsset CreateEmpty(const std::string& name = "Widget");
		static WidgetNode* FindByName(WidgetNode& root, const std::string& name);
		static const WidgetNode* FindByName(const WidgetNode& root, const std::string& name);

	private:
		std::string m_Path;
		WidgetNode m_Root;
		glm::vec2 m_DesignSize = glm::vec2(1280.0f, 720.0f);
	};
}
