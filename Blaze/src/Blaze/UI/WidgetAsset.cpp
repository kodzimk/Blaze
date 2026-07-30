#include "WidgetAsset.h"
#include "Blaze/Logging/Log.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace Blaze
{
	const char* WidgetNode::TypeName(WidgetType type)
	{
		switch (type)
		{
			case WidgetType::Canvas: return "Canvas";
			case WidgetType::Text: return "Text";
			case WidgetType::Button: return "Button";
			case WidgetType::Image: return "Image";
			default: return "Canvas";
		}
	}

	WidgetType WidgetNode::TypeFromName(const std::string& name)
	{
		std::string lower = name;
		for (char& c : lower)
			c = (char)std::tolower((unsigned char)c);
		if (lower == "text")
			return WidgetType::Text;
		if (lower == "button")
			return WidgetType::Button;
		if (lower == "image")
			return WidgetType::Image;
		return WidgetType::Canvas;
	}

	void WidgetNode::InitDefaults()
	{
		if (type == WidgetType::Canvas)
		{
			anchorMin = glm::vec2(0.0f);
			anchorMax = glm::vec2(1.0f);
			pivot = glm::vec2(0.0f);
			size = glm::vec2(0.0f);
			position = glm::vec2(0.0f);
			text.clear();
			texturePath.clear();
			fontPath.clear();
			color = glm::vec4(1.0f);
			if (name.empty() || name == "Widget")
				name = "Canvas";
		}
		else if (type == WidgetType::Text)
		{
			if (text.empty())
				text = "Hello";
			if (size.x < 1.0f)
				size = glm::vec2(200.0f, 40.0f);
			color = glm::vec4(1.0f);
			if (name.empty() || name == "Widget")
				name = "Text";
		}
		else if (type == WidgetType::Button)
		{
			if (text.empty())
				text = "Button";
			if (size.x < 1.0f || size.y < 1.0f)
				size = glm::vec2(160.0f, 40.0f);
			color = glm::vec4(0.20f, 0.45f, 0.85f, 1.0f);
			if (name.empty() || name == "Widget")
				name = "Button";
		}
		else if (type == WidgetType::Image)
		{
			if (size.x < 1.0f || size.y < 1.0f)
				size = glm::vec2(128.0f, 128.0f);
			color = glm::vec4(1.0f);
			if (name.empty() || name == "Widget")
				name = "Image";
		}
	}

	WidgetAsset WidgetAsset::CreateEmpty(const std::string& name)
	{
		WidgetAsset asset;
		asset.m_Root.type = WidgetType::Canvas;
		asset.m_Root.name = name.empty() ? "Canvas" : name;
		asset.m_Root.InitDefaults();
		return asset;
	}

	WidgetAsset WidgetAsset::CreateDefault(const std::string& name)
	{
		WidgetAsset asset = CreateEmpty(name);

		WidgetNode title;
		title.type = WidgetType::Text;
		title.name = "Title";
		title.InitDefaults();
		title.text = name.empty() ? "New Widget" : name;
		title.anchorMin = title.anchorMax = glm::vec2(0.5f, 0.25f);
		title.fontSize = 32.0f;
		asset.m_Root.children.push_back(title);

		WidgetNode button;
		button.type = WidgetType::Button;
		button.name = "Button";
		button.InitDefaults();
		button.text = "Click Me";
		button.anchorMin = button.anchorMax = glm::vec2(0.5f, 0.55f);
		asset.m_Root.children.push_back(button);
		return asset;
	}

	WidgetNode* WidgetAsset::FindByName(WidgetNode& root, const std::string& name)
	{
		if (root.name == name)
			return &root;
		for (WidgetNode& child : root.children)
		{
			if (WidgetNode* found = FindByName(child, name))
				return found;
		}
		return nullptr;
	}

	const WidgetNode* WidgetAsset::FindByName(const WidgetNode& root, const std::string& name)
	{
		return FindByName(const_cast<WidgetNode&>(root), name);
	}

	static void WriteNode(std::ostream& out, const WidgetNode& node, int depth)
	{
		const std::string pad(depth * 2, ' ');
		out << pad << "widget " << WidgetNode::TypeName(node.type) << " " << node.name << "\n";
		if (!node.text.empty())
			out << pad << "  text " << node.text << "\n";
		if (!node.texturePath.empty())
			out << pad << "  texture " << node.texturePath << "\n";
		if (!node.fontPath.empty())
			out << pad << "  font " << node.fontPath << "\n";
		out << pad << "  fontsize " << node.fontSize << "\n";
		out << pad << "  visible " << (node.visible ? 1 : 0) << "\n";
		out << pad << "  anchormin " << node.anchorMin.x << " " << node.anchorMin.y << "\n";
		out << pad << "  anchormax " << node.anchorMax.x << " " << node.anchorMax.y << "\n";
		out << pad << "  pivot " << node.pivot.x << " " << node.pivot.y << "\n";
		out << pad << "  position " << node.position.x << " " << node.position.y << "\n";
		out << pad << "  size " << node.size.x << " " << node.size.y << "\n";
		out << pad << "  color " << node.color.r << " " << node.color.g << " " << node.color.b << " " << node.color.a << "\n";
		for (const WidgetNode& child : node.children)
			WriteNode(out, child, depth + 1);
		out << pad << "end\n";
	}

	bool WidgetAsset::SaveToFile(const std::string& path) const
	{
		std::ofstream out(path, std::ios::trunc);
		if (!out)
		{
			BZ_INFO("Failed to save widget: {0}", path);
			return false;
		}
		out << "BlazeWidget 1\n";
		out << "designsize " << m_DesignSize.x << " " << m_DesignSize.y << "\n";
		WriteNode(out, m_Root, 0);
		return true;
	}

	bool WidgetAsset::LoadFromFile(const std::string& path)
	{
		std::ifstream in(path);
		if (!in)
		{
			BZ_INFO("Failed to load widget: {0}", path);
			return false;
		}

		m_Path = path;
		m_Root = WidgetNode{};
		m_Root.type = WidgetType::Canvas;
		m_Root.name = "Canvas";
		m_Root.InitDefaults();
		m_DesignSize = glm::vec2(1280.0f, 720.0f);

		std::vector<WidgetNode*> stack;
		std::string line;
		bool haveRoot = false;

		while (std::getline(in, line))
		{
			if (!line.empty() && line.back() == '\r')
				line.pop_back();

			std::string trimmed = line;
			size_t start = trimmed.find_first_not_of(" \t");
			if (start == std::string::npos)
				continue;
			trimmed = trimmed.substr(start);
			if (trimmed.empty() || trimmed[0] == '#')
				continue;

			std::istringstream stream(trimmed);
			std::string key;
			stream >> key;
			for (char& c : key)
				c = (char)std::tolower((unsigned char)c);

			if (key == "blazewidget")
				continue;

			if (key == "designsize")
			{
				float w = 1280.0f, h = 720.0f;
				stream >> w >> h;
				SetDesignSize(w, h);
				continue;
			}

			if (key == "widget")
			{
				std::string typeName, name;
				stream >> typeName;
				std::getline(stream, name);
				size_t ns = name.find_first_not_of(" \t");
				if (ns != std::string::npos)
					name = name.substr(ns);
				else
					name = typeName;

				WidgetNode node;
				node.type = WidgetNode::TypeFromName(typeName);
				node.name = name.empty() ? WidgetNode::TypeName(node.type) : name;
				node.InitDefaults();

				if (!haveRoot)
				{
					m_Root = node;
					haveRoot = true;
					stack.clear();
					stack.push_back(&m_Root);
				}
				else if (!stack.empty())
				{
					stack.back()->children.push_back(node);
					stack.push_back(&stack.back()->children.back());
				}
				continue;
			}

			if (key == "end")
			{
				if (stack.size() > 1)
					stack.pop_back();
				continue;
			}

			if (stack.empty())
				continue;
			WidgetNode& cur = *stack.back();

			if (key == "text")
			{
				std::string rest;
				std::getline(stream, rest);
				size_t ns = rest.find_first_not_of(" \t");
				cur.text = (ns == std::string::npos) ? std::string() : rest.substr(ns);
			}
			else if (key == "texture")
			{
				std::string rest;
				std::getline(stream, rest);
				size_t ns = rest.find_first_not_of(" \t");
				cur.texturePath = (ns == std::string::npos) ? std::string() : rest.substr(ns);
			}
			else if (key == "font")
			{
				std::string rest;
				std::getline(stream, rest);
				size_t ns = rest.find_first_not_of(" \t");
				cur.fontPath = (ns == std::string::npos) ? std::string() : rest.substr(ns);
			}
			else if (key == "fontsize")
				stream >> cur.fontSize;
			else if (key == "visible")
			{
				int v = 1;
				stream >> v;
				cur.visible = (v != 0);
			}
			else if (key == "anchormin")
				stream >> cur.anchorMin.x >> cur.anchorMin.y;
			else if (key == "anchormax")
				stream >> cur.anchorMax.x >> cur.anchorMax.y;
			else if (key == "pivot")
				stream >> cur.pivot.x >> cur.pivot.y;
			else if (key == "position")
				stream >> cur.position.x >> cur.position.y;
			else if (key == "size")
				stream >> cur.size.x >> cur.size.y;
			else if (key == "color")
				stream >> cur.color.r >> cur.color.g >> cur.color.b >> cur.color.a;
		}

		if (!haveRoot)
			m_Root.InitDefaults();
		return true;
	}
}
