#include "ScriptEngine.h"
#include "OnScreenDebug.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Renderer/Renderer.h"
#include "Blaze/Renderer/Camera.h"
#include "Blaze/Audio/AudioEngine.h"
#include "Blaze/Core/Input.h"
#include "Blaze/Core/Project.h"
#include "Blaze/Event/KeyCodes.h"
#include "Blaze/Event/MouseButtonCodes.h"
#include "Blaze/Logging/Log.h"
#include "Blaze/UI/UISystem.h"

// Soft safeties: full SOL_ALL_SAFETIES_ON can abort() the process on script errors.
#define SOL_SAFE_FUNCTIONS 1
#define SOL_SAFE_USERTYPE 1
#define SOL_EXCEPTIONS_SAFE_PROPAGATION 1
#include "sol.hpp"

#include "../vendor/imgui/src/imgui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <tuple>

namespace Blaze
{
	struct ScriptInstance
	{
		GameObject* object = nullptr;
		sol::environment env;
		sol::protected_function onStart;
		sol::protected_function onUpdate;
		sol::protected_function onCollisionBegin;
		sol::protected_function onCollisionEnd;
		sol::protected_function onTriggerBegin;
		sol::protected_function onTriggerEnd;
		sol::protected_function onWidgetClicked;
		bool started = false;
	};

	static std::unique_ptr<sol::state> s_Lua;
	static bool s_Ready = false;
	static std::unordered_map<GameObject*, ScriptInstance> s_Instances;
	static ScriptInstance s_MapInstance;
	static bool s_HasMapInstance = false;
	static std::unordered_map<int, bool> s_KeyWasDown;
	static std::unordered_map<int, bool> s_MouseWasDown;
	static GameObject* s_CurrentObject = nullptr;
	static std::vector<GameObject*> s_PendingDestroy;
	static std::unordered_set<GameObject*> s_LiveObjects;
	static std::string s_PendingOpenMap;

	struct ScriptObject
	{
		GameObject* raw = nullptr;

		ScriptObject() = default;
		explicit ScriptObject(GameObject* object) : raw(object) {}

		bool IsValid() const
		{
			return raw != nullptr && s_LiveObjects.find(raw) != s_LiveObjects.end();
		}

		std::string GetName() const
		{
			return IsValid() ? raw->GetName() : std::string();
		}

		std::tuple<float, float> GetLocation() const
		{
			if (!IsValid())
				return std::make_tuple(0.0f, 0.0f);
			glm::vec3 p = raw->GetPosition();
			return std::make_tuple(p.x, p.y);
		}

		float GetRotation() const
		{
			return IsValid() ? raw->GetRotation() : 0.0f;
		}

		std::tuple<float, float> GetScale() const
		{
			if (!IsValid())
				return std::make_tuple(1.0f, 1.0f);
			glm::vec2 s = raw->GetScale();
			return std::make_tuple(s.x, s.y);
		}

		std::tuple<float, float, float, float> GetColor() const
		{
			if (!IsValid())
				return std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f);
			glm::vec4 c = raw->GetColor();
			return std::make_tuple(c.r, c.g, c.b, c.a);
		}

		void SetColor(float r, float g, float b, float a)
		{
			if (IsValid())
				raw->SetColor(r, g, b, a);
		}

		void SetColorRGB(float r, float g, float b)
		{
			SetColor(r, g, b, 1.0f);
		}

		std::string GetTag() const
		{
			return IsValid() ? raw->GetTag() : std::string();
		}

		std::string GetTags() const
		{
			return IsValid() ? raw->GetTagsString() : std::string();
		}

		bool HasTag(const std::string& tag) const
		{
			return IsValid() && raw->HasTag(tag);
		}

		void SetTag(const std::string& tag)
		{
			if (IsValid())
				raw->SetTag(tag);
		}

		void AddTag(const std::string& tag)
		{
			if (IsValid())
				raw->AddTag(tag);
		}

		void SetVisible(bool visible)
		{
			if (IsValid())
				raw->SetVisible(visible);
		}

		bool IsVisible() const
		{
			return IsValid() && raw->IsVisible();
		}
	};

	struct ScriptWidgetElement
	{
		std::shared_ptr<WidgetAsset> asset;
		std::string name;

		WidgetNode* Node() const
		{
			if (!asset)
				return nullptr;
			return WidgetAsset::FindByName(asset->GetRoot(), name);
		}

		bool IsValid() const { return Node() != nullptr; }

		std::string GetName() const { return name; }

		std::string GetType() const
		{
			WidgetNode* node = Node();
			return node ? WidgetNode::TypeName(node->type) : std::string();
		}

		void SetText(const std::string& text)
		{
			if (WidgetNode* node = Node())
				node->text = text;
		}

		std::string GetText() const
		{
			WidgetNode* node = Node();
			return node ? node->text : std::string();
		}

		void SetVisible(bool visible)
		{
			if (WidgetNode* node = Node())
				node->visible = visible;
		}

		bool IsVisible() const
		{
			WidgetNode* node = Node();
			return node && node->visible;
		}

		void SetSize(float w, float h)
		{
			if (WidgetNode* node = Node())
				node->size = glm::vec2(w, h);
		}

		void SetPosition(float x, float y)
		{
			if (WidgetNode* node = Node())
				node->position = glm::vec2(x, y);
		}

		void SetAnchor(float minX, float minY, float maxX, float maxY)
		{
			if (WidgetNode* node = Node())
			{
				node->anchorMin = glm::vec2(minX, minY);
				node->anchorMax = glm::vec2(maxX, maxY);
			}
		}

		void SetColor(float r, float g, float b, sol::optional<float> a)
		{
			if (WidgetNode* node = Node())
				node->color = glm::vec4(r, g, b, a.value_or(1.0f));
		}

		void SetFontSize(float size)
		{
			if (WidgetNode* node = Node())
				node->fontSize = size;
		}

		void SetFontPath(const std::string& path)
		{
			if (WidgetNode* node = Node())
				node->fontPath = path;
		}

		std::string GetFontPath() const
		{
			WidgetNode* node = Node();
			return node ? node->fontPath : std::string();
		}

		void SetTexture(const std::string& path)
		{
			if (WidgetNode* node = Node())
				node->texturePath = path;
		}

		std::string GetTexture() const
		{
			WidgetNode* node = Node();
			return node ? node->texturePath : std::string();
		}

		void SetImage(const std::string& path) { SetTexture(path); }
		std::string GetImage() const { return GetTexture(); }
	};

	struct ScriptWidget
	{
		std::shared_ptr<WidgetAsset> asset;
		std::string id;

		bool IsValid() const { return asset != nullptr; }

		std::string GetId() const { return id; }
		std::string GetName() const
		{
			return IsValid() ? asset->GetRoot().name : std::string();
		}

		WidgetNode* FindNode(const std::string& name) const
		{
			if (!IsValid())
				return nullptr;
			return WidgetAsset::FindByName(asset->GetRoot(), name);
		}

		WidgetNode* FindParent(const std::string& parentName) const
		{
			if (!IsValid())
				return nullptr;
			if (parentName.empty())
				return &asset->GetRoot();
			return WidgetAsset::FindByName(asset->GetRoot(), parentName);
		}

		ScriptWidgetElement Find(const std::string& name) const
		{
			if (!FindNode(name))
				return {};
			return ScriptWidgetElement{ asset, name };
		}

		ScriptWidgetElement FindText(const std::string& name) const
		{
			WidgetNode* node = FindNode(name);
			if (!node || node->type != WidgetType::Text)
				return {};
			return ScriptWidgetElement{ asset, name };
		}

		ScriptWidgetElement FindButton(const std::string& name) const
		{
			WidgetNode* node = FindNode(name);
			if (!node || node->type != WidgetType::Button)
				return {};
			return ScriptWidgetElement{ asset, name };
		}

		ScriptWidgetElement FindImage(const std::string& name) const
		{
			WidgetNode* node = FindNode(name);
			if (!node || node->type != WidgetType::Image)
				return {};
			return ScriptWidgetElement{ asset, name };
		}

		ScriptWidgetElement AddText(const std::string& name, const std::string& text,
			sol::optional<float> anchorX, sol::optional<float> anchorY,
			sol::optional<std::string> parentName)
		{
			WidgetNode* parent = FindParent(parentName.value_or(std::string()));
			if (!parent || FindNode(name))
				return {};

			WidgetNode node;
			node.type = WidgetType::Text;
			node.name = name;
			node.InitDefaults();
			node.text = text;
			const float ax = anchorX.value_or(0.5f);
			const float ay = anchorY.value_or(0.5f);
			node.anchorMin = node.anchorMax = glm::vec2(ax, ay);
			parent->children.push_back(std::move(node));
			return ScriptWidgetElement{ asset, name };
		}

		ScriptWidgetElement AddButton(const std::string& name, const std::string& text,
			sol::optional<float> anchorX, sol::optional<float> anchorY,
			sol::optional<std::string> parentName)
		{
			WidgetNode* parent = FindParent(parentName.value_or(std::string()));
			if (!parent || FindNode(name))
				return {};

			WidgetNode node;
			node.type = WidgetType::Button;
			node.name = name;
			node.InitDefaults();
			node.text = text;
			const float ax = anchorX.value_or(0.5f);
			const float ay = anchorY.value_or(0.5f);
			node.anchorMin = node.anchorMax = glm::vec2(ax, ay);
			parent->children.push_back(std::move(node));
			return ScriptWidgetElement{ asset, name };
		}

		ScriptWidgetElement AddImage(const std::string& name, const std::string& texturePath,
			sol::optional<float> anchorX, sol::optional<float> anchorY,
			sol::optional<std::string> parentName)
		{
			WidgetNode* parent = FindParent(parentName.value_or(std::string()));
			if (!parent || FindNode(name))
				return {};

			WidgetNode node;
			node.type = WidgetType::Image;
			node.name = name;
			node.InitDefaults();
			node.texturePath = texturePath;
			const float ax = anchorX.value_or(0.5f);
			const float ay = anchorY.value_or(0.5f);
			node.anchorMin = node.anchorMax = glm::vec2(ax, ay);
			parent->children.push_back(std::move(node));
			return ScriptWidgetElement{ asset, name };
		}

		bool SetText(const std::string& nodeName, const std::string& text)
		{
			WidgetNode* node = FindNode(nodeName);
			if (!node)
				return false;
			node->text = text;
			return true;
		}

		std::string GetText(const std::string& nodeName) const
		{
			WidgetNode* node = FindNode(nodeName);
			return node ? node->text : std::string();
		}

		bool SetVisible(const std::string& nodeName, bool visible)
		{
			WidgetNode* node = FindNode(nodeName);
			if (!node)
				return false;
			node->visible = visible;
			return true;
		}

		bool SetSize(const std::string& nodeName, float w, float h)
		{
			WidgetNode* node = FindNode(nodeName);
			if (!node)
				return false;
			node->size = glm::vec2(w, h);
			return true;
		}

		bool SetPosition(const std::string& nodeName, float x, float y)
		{
			WidgetNode* node = FindNode(nodeName);
			if (!node)
				return false;
			node->position = glm::vec2(x, y);
			return true;
		}

		bool SetAnchor(const std::string& nodeName, float minX, float minY, float maxX, float maxY)
		{
			WidgetNode* node = FindNode(nodeName);
			if (!node)
				return false;
			node->anchorMin = glm::vec2(minX, minY);
			node->anchorMax = glm::vec2(maxX, maxY);
			return true;
		}

		bool SetColor(const std::string& nodeName, float r, float g, float b, sol::optional<float> a)
		{
			WidgetNode* node = FindNode(nodeName);
			if (!node)
				return false;
			node->color = glm::vec4(r, g, b, a.value_or(1.0f));
			return true;
		}

		bool SetFontSize(const std::string& nodeName, float size)
		{
			WidgetNode* node = FindNode(nodeName);
			if (!node)
				return false;
			node->fontSize = size;
			return true;
		}

		bool SetTexture(const std::string& nodeName, const std::string& path)
		{
			WidgetNode* node = FindNode(nodeName);
			if (!node)
				return false;
			node->texturePath = path;
			return true;
		}

		std::string GetTexture(const std::string& nodeName) const
		{
			WidgetNode* node = FindNode(nodeName);
			return node ? node->texturePath : std::string();
		}

		bool SetImage(const std::string& nodeName, const std::string& path)
		{
			return SetTexture(nodeName, path);
		}

		std::string GetImage(const std::string& nodeName) const
		{
			return GetTexture(nodeName);
		}
	};

	static int s_RuntimeWidgetSerial = 0;

	static std::string NormalizeWidgetPath(std::string path)
	{
		for (char& c : path)
		{
			if (c == '\\')
				c = '/';
		}
		return path;
	}

	static Renderer* s_Renderer = nullptr;
	static PhysicsWorld2D* s_Physics = nullptr;

	static const std::vector<float> s_DefaultVertices = {
		 50.f,  50.f, 0.0f, 1.0f, 1.0f,
		 50.f, -50.f, 0.0f, 1.0f, 0.0f,
		-50.f, -50.f, 0.0f, 0.0f, 0.0f,
		-50.f,  50.f, 0.0f, 0.0f, 1.0f,
	};

	static const std::vector<unsigned int> s_DefaultIndices = {
		0, 1, 3,
		1, 2, 3
	};

	struct ScriptError
	{
		std::string phase;
		std::string objectName;
		std::string scriptPath;
		std::string where;
		std::string message;
	};

	static std::vector<ScriptError> s_ScriptErrors;
	static bool s_ShowErrorWindow = false;

	static std::string ExtractWhere(const std::string& scriptPath, const std::string& err)
	{
		// sol/Lua formats like: [string "path"]:12: message  OR  path:12: message
		std::smatch match;
		std::regex pattern(R"((?:\[string \"([^\"]+)\"\]|([^:\s][^:]*?\.lua)):(\d+):)");
		if (std::regex_search(err, match, pattern))
		{
			std::string file = match[1].matched ? match[1].str() : match[2].str();
			std::string line = match[3].str();
			if (file.empty())
				file = scriptPath;
			return file + " : line " + line;
		}

		std::regex lineOnly(R"(:(\d+):)");
		if (std::regex_search(err, match, lineOnly))
			return scriptPath + " : line " + match[1].str();

		if (!scriptPath.empty())
			return scriptPath;
		return "unknown location";
	}

	static void ReportLuaError(const std::string& phase, GameObject* object,
		const std::string& scriptPath, const std::string& errWhat)
	{
		ScriptError entry;
		entry.phase = phase;
		entry.objectName = object ? object->GetName() : (scriptPath.empty() ? std::string("<none>") : std::string("[Map]"));
		entry.scriptPath = scriptPath.empty() && object ? object->GetScriptPath() : scriptPath;
		entry.where = ExtractWhere(entry.scriptPath, errWhat);
		entry.message = errWhat;
		s_ScriptErrors.push_back(entry);
		s_ShowErrorWindow = true;

		BZ_ERROR("========== LUA ERROR ==========");
		BZ_ERROR("When:   {0}", phase);
		BZ_ERROR("Object: {0}", entry.objectName);
		BZ_ERROR("Where:  {0}", entry.where);
		BZ_ERROR("Script: {0}", entry.scriptPath);
		BZ_ERROR("Error:  {0}", errWhat);
		BZ_ERROR("================================");

		std::fprintf(stderr,
			"\n========== LUA ERROR ==========\n"
			"When:   %s\n"
			"Object: %s\n"
			"Where:  %s\n"
			"Script: %s\n"
			"Error:  %s\n"
			"================================\n\n",
			phase.c_str(),
			entry.objectName.c_str(),
			entry.where.c_str(),
			entry.scriptPath.c_str(),
			errWhat.c_str());
		std::fflush(stderr);
	}

	struct PlayTransform
	{
		glm::vec3 position{ 0.0f };
		float rotation = 0.0f;
		glm::vec2 scale{ 1.0f, 1.0f };
	};
	static std::unordered_map<GameObject*, PlayTransform> s_PlayTransforms;

	static void StorePlayTransform(GameObject* object)
	{
		if (!object)
			return;

		PlayTransform t;
		t.position = object->GetLocalPosition();
		t.rotation = object->GetLocalRotation();
		t.scale = object->GetLocalScale();
		s_PlayTransforms[object] = t;
	}

	static void ApplyPlayTransforms()
	{
		for (auto& entry : s_PlayTransforms)
		{
			GameObject* object = entry.first;
			if (!object)
				continue;
			PlayTransform& t = entry.second;
			object->SetLocalPosition(t.position);
			object->SetLocalRotation(t.rotation);
			object->SetLocalScale(t.scale);
		}
	}

	static void CaptureInitialPlayTransforms(const std::vector<GameObject*>& objects)
	{
		s_PlayTransforms.clear();
		for (GameObject* object : objects)
		{
			if (object)
				StorePlayTransform(object);
		}
	}

	void ScriptEngine::CapturePlayTransforms(const std::vector<GameObject*>& objects)
	{
		for (GameObject* object : objects)
		{
			if (object)
				StorePlayTransform(object);
		}
	}

	void ScriptEngine::ApplyStoredPlayTransforms()
	{
		ApplyPlayTransforms();
	}

	static bool IsKeyDownCode(int code)
	{
		return code >= 0 && Input::IsKeyPressed(code);
	}

	static bool IsKeyPressedEdge(int code)
	{
		if (code < 0)
			return false;
		bool down = Input::IsKeyPressed(code);
		bool was = s_KeyWasDown[code];
		return down && !was;
	}

	static void SampleInputEdges()
	{
		static const int s_TrackKeys[] = {
			BZ_KEY_A, BZ_KEY_B, BZ_KEY_C, BZ_KEY_D, BZ_KEY_E, BZ_KEY_F, BZ_KEY_G, BZ_KEY_H,
			BZ_KEY_I, BZ_KEY_J, BZ_KEY_K, BZ_KEY_L, BZ_KEY_M, BZ_KEY_N, BZ_KEY_O, BZ_KEY_P,
			BZ_KEY_Q, BZ_KEY_R, BZ_KEY_S, BZ_KEY_T, BZ_KEY_U, BZ_KEY_V, BZ_KEY_W, BZ_KEY_X,
			BZ_KEY_Y, BZ_KEY_Z,
			BZ_KEY_0, BZ_KEY_1, BZ_KEY_2, BZ_KEY_3, BZ_KEY_4, BZ_KEY_5, BZ_KEY_6, BZ_KEY_7, BZ_KEY_8, BZ_KEY_9,
			BZ_KEY_SPACE, BZ_KEY_LEFT, BZ_KEY_RIGHT, BZ_KEY_UP, BZ_KEY_DOWN,
			BZ_KEY_ENTER, BZ_KEY_ESCAPE, BZ_KEY_TAB,
			BZ_KEY_LEFT_SHIFT, BZ_KEY_RIGHT_SHIFT, BZ_KEY_LEFT_CONTROL, BZ_KEY_RIGHT_CONTROL,
			BZ_KEY_LEFT_ALT, BZ_KEY_RIGHT_ALT
		};
		for (int code : s_TrackKeys)
			s_KeyWasDown[code] = Input::IsKeyPressed(code);

		s_MouseWasDown[BZ_MOUSE_BUTTON_LEFT] = Input::IsMouseButtonPressed(BZ_MOUSE_BUTTON_LEFT);
		s_MouseWasDown[BZ_MOUSE_BUTTON_RIGHT] = Input::IsMouseButtonPressed(BZ_MOUSE_BUTTON_RIGHT);
		s_MouseWasDown[BZ_MOUSE_BUTTON_MIDDLE] = Input::IsMouseButtonPressed(BZ_MOUSE_BUTTON_MIDDLE);
	}

	static std::string FormatLuaArgs(sol::variadic_args args)
	{
		std::string message;
		for (auto v : args)
		{
			if (!message.empty())
				message += "  ";
			sol::type t = v.get_type();
			if (t == sol::type::string)
				message += v.as<std::string>();
			else if (t == sol::type::number)
			{
				double n = v.as<double>();
				if (n == (double)(long long)n)
					message += std::to_string((long long)n);
				else
					message += std::to_string(n);
			}
			else if (t == sol::type::boolean)
				message += v.as<bool>() ? "true" : "false";
			else if (t == sol::type::nil)
				message += "nil";
			else
				message += "<value>";
		}
		return message;
	}

	static int ResolveKeyCode(std::string key)
	{
		for (char& c : key)
			c = (char)std::toupper((unsigned char)c);

		if (key.size() == 1 && key[0] >= 'A' && key[0] <= 'Z')
			return BZ_KEY_A + (key[0] - 'A');
		if (key.size() == 1 && key[0] >= '0' && key[0] <= '9')
			return BZ_KEY_0 + (key[0] - '0');

		if (key == "SPACE") return BZ_KEY_SPACE;
		if (key == "LEFT") return BZ_KEY_LEFT;
		if (key == "RIGHT") return BZ_KEY_RIGHT;
		if (key == "UP") return BZ_KEY_UP;
		if (key == "DOWN") return BZ_KEY_DOWN;
		if (key == "ENTER" || key == "RETURN") return BZ_KEY_ENTER;
		if (key == "ESCAPE" || key == "ESC") return BZ_KEY_ESCAPE;
		if (key == "TAB") return BZ_KEY_TAB;
		if (key == "LEFTSHIFT" || key == "LSHIFT" || key == "SHIFT") return BZ_KEY_LEFT_SHIFT;
		if (key == "RIGHTSHIFT" || key == "RSHIFT") return BZ_KEY_RIGHT_SHIFT;
		if (key == "LEFTCONTROL" || key == "LCTRL" || key == "CTRL" || key == "CONTROL") return BZ_KEY_LEFT_CONTROL;
		if (key == "RIGHTCONTROL" || key == "RCTRL") return BZ_KEY_RIGHT_CONTROL;
		if (key == "LEFTALT" || key == "LALT" || key == "ALT") return BZ_KEY_LEFT_ALT;
		if (key == "RIGHTALT" || key == "RALT") return BZ_KEY_RIGHT_ALT;

		return -1;
	}

	static bool AttachRuntimeScript(GameObject* object, bool callStart);

	static void BindApi(sol::state& lua)
	{
		lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

		lua.new_usertype<ScriptObject>("GameObject",
			sol::no_constructor,
			"GetName", &ScriptObject::GetName,
			"GetLocation", &ScriptObject::GetLocation,
			"GetRotation", &ScriptObject::GetRotation,
			"GetScale", &ScriptObject::GetScale,
			"GetColor", &ScriptObject::GetColor,
			"SetColor", sol::overload(&ScriptObject::SetColor, &ScriptObject::SetColorRGB),
			"GetTag", &ScriptObject::GetTag,
			"GetTags", &ScriptObject::GetTags,
			"HasTag", &ScriptObject::HasTag,
			"SetTag", &ScriptObject::SetTag,
			"AddTag", &ScriptObject::AddTag,
			"SetVisible", &ScriptObject::SetVisible,
			"IsVisible", &ScriptObject::IsVisible,
			"IsValid", &ScriptObject::IsValid);

		lua.new_usertype<ScriptWidgetElement>("WidgetElement",
			sol::no_constructor,
			"IsValid", &ScriptWidgetElement::IsValid,
			"GetName", &ScriptWidgetElement::GetName,
			"GetType", &ScriptWidgetElement::GetType,
			"SetText", &ScriptWidgetElement::SetText,
			"GetText", &ScriptWidgetElement::GetText,
			"SetVisible", &ScriptWidgetElement::SetVisible,
			"IsVisible", &ScriptWidgetElement::IsVisible,
			"SetSize", &ScriptWidgetElement::SetSize,
			"SetPosition", &ScriptWidgetElement::SetPosition,
			"SetAnchor", &ScriptWidgetElement::SetAnchor,
			"SetColor", &ScriptWidgetElement::SetColor,
			"SetFontSize", &ScriptWidgetElement::SetFontSize,
			"SetFontPath", &ScriptWidgetElement::SetFontPath,
			"GetFontPath", &ScriptWidgetElement::GetFontPath,
			"SetTexture", &ScriptWidgetElement::SetTexture,
			"GetTexture", &ScriptWidgetElement::GetTexture,
			"SetImage", &ScriptWidgetElement::SetImage,
			"GetImage", &ScriptWidgetElement::GetImage);

		lua.new_usertype<ScriptWidget>("Widget",
			sol::no_constructor,
			"IsValid", &ScriptWidget::IsValid,
			"GetId", &ScriptWidget::GetId,
			"GetName", &ScriptWidget::GetName,
			"Find", &ScriptWidget::Find,
			"FindText", &ScriptWidget::FindText,
			"FindButton", &ScriptWidget::FindButton,
			"FindImage", &ScriptWidget::FindImage,
			"AddText", &ScriptWidget::AddText,
			"AddButton", &ScriptWidget::AddButton,
			"AddImage", &ScriptWidget::AddImage,
			"SetText", &ScriptWidget::SetText,
			"GetText", &ScriptWidget::GetText,
			"SetVisible", &ScriptWidget::SetVisible,
			"SetSize", &ScriptWidget::SetSize,
			"SetPosition", &ScriptWidget::SetPosition,
			"SetAnchor", &ScriptWidget::SetAnchor,
			"SetColor", &ScriptWidget::SetColor,
			"SetFontSize", &ScriptWidget::SetFontSize,
			"SetTexture", &ScriptWidget::SetTexture,
			"GetTexture", &ScriptWidget::GetTexture,
			"SetImage", &ScriptWidget::SetImage,
			"GetImage", &ScriptWidget::GetImage);

		lua.set_function("CreateWidget", [](sol::optional<std::string> nameOpt) -> ScriptWidget
		{
			const std::string name = nameOpt.value_or("Widget");
			auto asset = std::make_shared<WidgetAsset>(WidgetAsset::CreateEmpty(name));
			const std::string id = "runtime:" + name + "_" + std::to_string(++s_RuntimeWidgetSerial);
			asset->SetPath(id);
			return ScriptWidget{ asset, id };
		});

		lua.set_function("LoadWidget", [](const std::string& path) -> ScriptWidget
		{
			auto asset = std::make_shared<WidgetAsset>();
			if (!asset->LoadFromFile(path))
				return ScriptWidget{};
			const std::string id = NormalizeWidgetPath(path);
			asset->SetPath(id);
			return ScriptWidget{ asset, id };
		});

		lua.set_function("print", [](sol::variadic_args args)
		{
			std::string message = FormatLuaArgs(args);
			BZ_INFO("[Lua] {0}", message);
			OnScreenDebug::Print(message);
		});

		lua.set_function("PrintString", [](const std::string& message, sol::optional<float> duration)
		{
			float time = duration.value_or(4.0f);
			BZ_INFO("[Lua] {0}", message);
			OnScreenDebug::Print(message, time);
		});

		lua.set_function("Destroy", sol::overload(
			[](const ScriptObject& object)
			{
				if (object.IsValid())
					ScriptEngine::QueueDestroy(object.raw);
			},
			[]()
			{
				ScriptEngine::QueueDestroy(s_CurrentObject);
			}));

		lua.set_function("SetTag", sol::overload(
			[](const std::string& tag)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetTag(tag);
			},
			[](const ScriptObject& object, const std::string& tag)
			{
				if (object.IsValid())
					object.raw->SetTag(tag);
			}));

		lua.set_function("AddTag", sol::overload(
			[](const std::string& tag)
			{
				if (s_CurrentObject)
					s_CurrentObject->AddTag(tag);
			},
			[](const ScriptObject& object, const std::string& tag)
			{
				if (object.IsValid())
					object.raw->AddTag(tag);
			}));

		lua.set_function("GetTag", []()
		{
			return s_CurrentObject ? s_CurrentObject->GetTag() : std::string();
		});

		lua.set_function("HasTag", sol::overload(
			[](const std::string& tag)
			{
				return s_CurrentObject && s_CurrentObject->HasTag(tag);
			},
			[](const ScriptObject& object, const std::string& tag)
			{
				return object.HasTag(tag);
			}));

		lua.set_function("FindByTag", [](const std::string& tag) -> sol::object
		{
			GameObject* found = ScriptEngine::FindByTag(tag);
			if (!found || !s_Lua)
				return sol::make_object(s_Lua->lua_state(), sol::lua_nil);
			return sol::make_object(*s_Lua, ScriptObject(found));
		});

		lua.set_function("FindAllByTag", [](const std::string& tag)
		{
			sol::table result = s_Lua->create_table();
			int index = 1;
			for (GameObject* object : ScriptEngine::FindAllByTag(tag))
				result[index++] = ScriptObject(object);
			return result;
		});

		lua.set_function("GetCurrentMapName", []()
		{
			return ScriptEngine::GetCurrentMapName();
		});

		lua.set_function("OpenMap", [](const std::string& mapName)
		{
			return ScriptEngine::RequestOpenMap(mapName);
		});

		lua.set_function("Spawn", sol::overload(
			[]()
			{
				GameObject* object = ScriptEngine::SpawnObject("sprite", "", "", 0.0f, 0.0f, 0.0f);
				return ScriptObject(object);
			},
			[](const std::string& typeName)
			{
				GameObject* object = ScriptEngine::SpawnObject(typeName, "", "", 0.0f, 0.0f, 0.0f);
				return ScriptObject(object);
			},
			[](const sol::table& opts)
			{
				std::string typeName = "sprite";
				std::string name;
				std::string tag;
				float x = 0.0f, y = 0.0f, z = 0.0f;

				if (sol::optional<std::string> t = opts["type"]) typeName = *t;
				if (sol::optional<std::string> n = opts["name"]) name = *n;
				if (sol::optional<std::string> tg = opts["tag"]) tag = *tg;
				if (sol::optional<std::string> tags = opts["tags"]) tag = *tags;
				if (sol::optional<float> ox = opts["x"]) x = *ox;
				if (sol::optional<float> oy = opts["y"]) y = *oy;
				if (sol::optional<float> oz = opts["z"]) z = *oz;

				GameObject* object = ScriptEngine::SpawnObject(typeName, name, tag, x, y, z);
				if (!object)
					return ScriptObject();

				if (sol::optional<std::string> tex = opts["texture"])
					object->SetTexture(*tex);
				if (sol::optional<sol::table> color = opts["color"])
				{
					float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
					const sol::table& c = *color;
					if (sol::optional<float> v = c["r"]) r = *v;
					else if (sol::optional<float> v = c[1]) r = *v;
					if (sol::optional<float> v = c["g"]) g = *v;
					else if (sol::optional<float> v = c[2]) g = *v;
					if (sol::optional<float> v = c["b"]) b = *v;
					else if (sol::optional<float> v = c[3]) b = *v;
					if (sol::optional<float> v = c["a"]) a = *v;
					else if (sol::optional<float> v = c[4]) a = *v;
					object->SetColor(r, g, b, a);
				}
				if (sol::optional<std::string> script = opts["script"])
				{
					object->AddScript();
					object->SetScriptPath(*script);
					AttachRuntimeScript(object, true);
				}

				return ScriptObject(object);
			}));

		sol::table log = lua.create_named_table("Log");
		log["Info"] = [](const std::string& msg)
		{
			BZ_INFO("[Lua] {0}", msg);
			OnScreenDebug::Print(msg, 4.0f, 0.4f, 0.85f, 1.0f);
		};
		log["Warn"] = [](const std::string& msg)
		{
			BZ_INFO("[Lua Warn] {0}", msg);
			OnScreenDebug::Print(msg, 5.0f, 1.0f, 0.55f, 0.1f);
		};

		sol::table input = lua.create_named_table("Input");
		input["IsKeyDown"] = [](const std::string& key)
		{
			return IsKeyDownCode(ResolveKeyCode(key));
		};
		input["IsKeyPressed"] = [](const std::string& key)
		{
			return IsKeyPressedEdge(ResolveKeyCode(key));
		};
		input["IsMouseDown"] = [](int button)
		{
			return Input::IsMouseButtonPressed(button);
		};
		input["IsMousePressed"] = [](int button)
		{
			bool down = Input::IsMouseButtonPressed(button);
			bool was = s_MouseWasDown[button];
			return down && !was;
		};
		input["GetMouseX"] = []() { return Input::GetMouseX(); };
		input["GetMouseY"] = []() { return Input::GetMouseY(); };

		lua["MOUSE_LEFT"] = BZ_MOUSE_BUTTON_LEFT;
		lua["MOUSE_RIGHT"] = BZ_MOUSE_BUTTON_RIGHT;
		lua["MOUSE_MIDDLE"] = BZ_MOUSE_BUTTON_MIDDLE;

		lua.set_function("ShowWidget", sol::overload(
			[](const std::string& path) -> ScriptWidget
			{
				if (!UISystem::ShowWidget(path))
					return ScriptWidget{};
				const std::string id = NormalizeWidgetPath(path);
				auto asset = UISystem::GetShownAsset(id);
				if (!asset)
					return ScriptWidget{};
				return ScriptWidget{ asset, id };
			},
			[](const ScriptWidget& widget) -> ScriptWidget
			{
				if (!widget.IsValid())
					return ScriptWidget{};
				if (!UISystem::ShowWidget(widget.asset, widget.id))
					return ScriptWidget{};
				return widget;
			}));
		lua.set_function("HideWidget", sol::overload(
			[](const std::string& path)
			{
				UISystem::HideWidget(path);
			},
			[](const ScriptWidget& widget)
			{
				if (widget.IsValid())
					UISystem::HideWidget(widget.id);
			}));
		lua.set_function("HideAllWidgets", []()
		{
			UISystem::HideAll();
		});
		lua.set_function("SetWidgetText", sol::overload(
			[](const std::string& widgetPath, const std::string& nodeName, const std::string& text)
			{
				return UISystem::SetWidgetText(widgetPath, nodeName, text);
			},
			[](ScriptWidget widget, const std::string& nodeName, const std::string& text)
			{
				return widget.SetText(nodeName, text);
			}));
		lua.set_function("GetWidgetText", sol::overload(
			[](const std::string& widgetPath, const std::string& nodeName)
			{
				return UISystem::GetWidgetText(widgetPath, nodeName);
			},
			[](ScriptWidget widget, const std::string& nodeName)
			{
				return widget.GetText(nodeName);
			}));

		lua.set_function("SetWidgetImage", sol::overload(
			[](ScriptWidget widget, const std::string& nodeName, const std::string& path)
			{
				return widget.SetImage(nodeName, path);
			},
			[](const std::string& widgetId, const std::string& nodeName, const std::string& path)
			{
				auto asset = UISystem::GetShownAsset(widgetId);
				if (!asset)
					return false;
				WidgetNode* node = WidgetAsset::FindByName(asset->GetRoot(), nodeName);
				if (!node)
					return false;
				node->texturePath = path;
				return true;
			}));

		lua.set_function("SetText", [](const std::string& text)
		{
			if (s_CurrentObject && s_CurrentObject->IsText())
				s_CurrentObject->SetText(text);
		});
		lua.set_function("GetText", []() -> std::string
		{
			if (s_CurrentObject && s_CurrentObject->IsText())
				return s_CurrentObject->GetText();
			return {};
		});
		lua.set_function("SetFontSize", [](float size)
		{
			if (s_CurrentObject && s_CurrentObject->IsText())
				s_CurrentObject->SetFontSize(size);
		});
		lua.set_function("SetFontPath", sol::overload(
			[](const std::string& path)
			{
				if (s_CurrentObject && s_CurrentObject->IsText())
					s_CurrentObject->SetFontPath(path);
			},
			[](const ScriptObject& object, const std::string& path)
			{
				if (object.IsValid() && object.raw->IsText())
					object.raw->SetFontPath(path);
			}));
		lua.set_function("GetFontPath", sol::overload(
			[]() -> std::string
			{
				if (s_CurrentObject && s_CurrentObject->IsText())
					return s_CurrentObject->GetFontPath();
				return {};
			},
			[](const ScriptObject& object) -> std::string
			{
				if (object.IsValid() && object.raw->IsText())
					return object.raw->GetFontPath();
				return {};
			}));

		lua.set_function("SetVisible", sol::overload(
			[](bool visible)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetVisible(visible);
			},
			[](const ScriptObject& object, bool visible)
			{
				if (object.IsValid())
					object.raw->SetVisible(visible);
			}));
		lua.set_function("IsVisible", sol::overload(
			[]()
			{
				return s_CurrentObject && s_CurrentObject->IsVisible();
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() && object.raw->IsVisible();
			}));

		lua.set_function("SetLocation", sol::overload(
			[](float x, float y)
			{
				if (!s_CurrentObject)
					return;
				glm::vec3 pos(x, y, s_CurrentObject->GetPosition().z);
				s_CurrentObject->SetPosition(pos);
				StorePlayTransform(s_CurrentObject);
			},
			[](float x, float y, float z)
			{
				if (!s_CurrentObject)
					return;
				glm::vec3 pos(x, y, z);
				s_CurrentObject->SetPosition(pos);
				StorePlayTransform(s_CurrentObject);
			}));

		lua.set_function("SetRotation", [](float degrees)
		{
			if (!s_CurrentObject)
				return;
			s_CurrentObject->SetRotation(degrees);
			StorePlayTransform(s_CurrentObject);
		});

		lua.set_function("Rotate", [](float degrees)
		{
			if (!s_CurrentObject)
				return;
			s_CurrentObject->Rotate(degrees);
			StorePlayTransform(s_CurrentObject);
		});

		lua.set_function("SetPosition", sol::overload(
			[](float x, float y)
			{
				if (!s_CurrentObject)
					return;
				glm::vec3 pos(x, y, s_CurrentObject->GetPosition().z);
				s_CurrentObject->SetPosition(pos);
				StorePlayTransform(s_CurrentObject);
			},
			[](float x, float y, float z)
			{
				if (!s_CurrentObject)
					return;
				glm::vec3 pos(x, y, z);
				s_CurrentObject->SetPosition(pos);
				StorePlayTransform(s_CurrentObject);
			}));

		lua.set_function("SetScale", sol::overload(
			[](float uniform)
			{
				if (!s_CurrentObject)
					return;
				s_CurrentObject->SetScale(glm::vec2(uniform, uniform));
				StorePlayTransform(s_CurrentObject);
			},
			[](float x, float y)
			{
				if (!s_CurrentObject)
					return;
				s_CurrentObject->SetScale(glm::vec2(x, y));
				StorePlayTransform(s_CurrentObject);
			}));

		lua.set_function("GetLocation", []()
		{
			if (!s_CurrentObject)
				return std::make_tuple(0.0f, 0.0f);
			glm::vec3 p = s_CurrentObject->GetPosition();
			return std::make_tuple(p.x, p.y);
		});

		lua.set_function("GetRotation", []()
		{
			return s_CurrentObject ? s_CurrentObject->GetRotation() : 0.0f;
		});

		lua.set_function("GetScale", []()
		{
			if (!s_CurrentObject)
				return std::make_tuple(1.0f, 1.0f);
			glm::vec2 s = s_CurrentObject->GetScale();
			return std::make_tuple(s.x, s.y);
		});

		lua.set_function("SetColor", sol::overload(
			[](float r, float g, float b)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetColor(r, g, b, 1.0f);
			},
			[](float r, float g, float b, float a)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetColor(r, g, b, a);
			},
			[](const sol::table& color)
			{
				if (!s_CurrentObject)
					return;
				float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
				if (sol::optional<float> v = color["r"]) r = *v;
				else if (sol::optional<float> v = color[1]) r = *v;
				if (sol::optional<float> v = color["g"]) g = *v;
				else if (sol::optional<float> v = color[2]) g = *v;
				if (sol::optional<float> v = color["b"]) b = *v;
				else if (sol::optional<float> v = color[3]) b = *v;
				if (sol::optional<float> v = color["a"]) a = *v;
				else if (sol::optional<float> v = color[4]) a = *v;
				s_CurrentObject->SetColor(r, g, b, a);
			},
			[](const ScriptObject& object, float r, float g, float b)
			{
				if (object.IsValid())
					object.raw->SetColor(r, g, b, 1.0f);
			},
			[](const ScriptObject& object, float r, float g, float b, float a)
			{
				if (object.IsValid())
					object.raw->SetColor(r, g, b, a);
			},
			[](const ScriptObject& object, const sol::table& color)
			{
				if (!object.IsValid())
					return;
				float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
				if (sol::optional<float> v = color["r"]) r = *v;
				else if (sol::optional<float> v = color[1]) r = *v;
				if (sol::optional<float> v = color["g"]) g = *v;
				else if (sol::optional<float> v = color[2]) g = *v;
				if (sol::optional<float> v = color["b"]) b = *v;
				else if (sol::optional<float> v = color[3]) b = *v;
				if (sol::optional<float> v = color["a"]) a = *v;
				else if (sol::optional<float> v = color[4]) a = *v;
				object.raw->SetColor(r, g, b, a);
			}));

		lua.set_function("GetColor", sol::overload(
			[]()
			{
				if (!s_CurrentObject)
					return std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f);
				glm::vec4 c = s_CurrentObject->GetColor();
				return std::make_tuple(c.r, c.g, c.b, c.a);
			},
			[](const ScriptObject& object)
			{
				if (!object.IsValid())
					return std::make_tuple(1.0f, 1.0f, 1.0f, 1.0f);
				glm::vec4 c = object.raw->GetColor();
				return std::make_tuple(c.r, c.g, c.b, c.a);
			}));

		auto applyTranslate = [](float x, float y)
		{
			if (!s_CurrentObject)
				return;
			glm::vec3 pos = s_CurrentObject->GetPosition();
			pos.x += x;
			pos.y += y;
			s_CurrentObject->SetPosition(pos);
			StorePlayTransform(s_CurrentObject);
		};

		lua.set_function("Translate", sol::overload(
			[applyTranslate](float x, float y)
			{
				applyTranslate(x, y);
			},
			[applyTranslate](const sol::table& offset)
			{
				float x = 0.0f;
				float y = 0.0f;

				sol::optional<float> namedX = offset["x"];
				sol::optional<float> namedY = offset["y"];
				if (namedX && namedY)
				{
					x = *namedX;
					y = *namedY;
				}
				else
				{
					sol::optional<float> i1 = offset[1];
					sol::optional<float> i2 = offset[2];
					if (i1) x = *i1;
					if (i2) y = *i2;
				}

				applyTranslate(x, y);
			}));

		auto readVec2 = [](const sol::table& t, float& x, float& y)
		{
			x = 0.0f;
			y = 0.0f;
			if (sol::optional<float> nx = t["x"]) x = *nx;
			else if (sol::optional<float> i1 = t[1]) x = *i1;
			if (sol::optional<float> ny = t["y"]) y = *ny;
			else if (sol::optional<float> i2 = t[2]) y = *i2;
		};

		lua.set_function("SetVelocity", sol::overload(
			[](float x, float y)
			{
				if (!s_Physics || !s_CurrentObject) return;
				s_Physics->SetLinearVelocity(s_CurrentObject, glm::vec2(x, y));
			},
			[readVec2](const sol::table& velocity)
			{
				if (!s_Physics || !s_CurrentObject) return;
				float x, y;
				readVec2(velocity, x, y);
				s_Physics->SetLinearVelocity(s_CurrentObject, glm::vec2(x, y));
			},
			[](const ScriptObject& object, float x, float y)
			{
				if (!s_Physics || !object.IsValid()) return;
				s_Physics->SetLinearVelocity(object.raw, glm::vec2(x, y));
			},
			[readVec2](const ScriptObject& object, const sol::table& velocity)
			{
				if (!s_Physics || !object.IsValid()) return;
				float x, y;
				readVec2(velocity, x, y);
				s_Physics->SetLinearVelocity(object.raw, glm::vec2(x, y));
			}));

		lua.set_function("GetVelocity", sol::overload(
			[]()
			{
				if (!s_Physics || !s_CurrentObject)
					return std::make_tuple(0.0f, 0.0f);
				glm::vec2 v = s_Physics->GetLinearVelocity(s_CurrentObject);
				return std::make_tuple(v.x, v.y);
			},
			[](const ScriptObject& object)
			{
				if (!s_Physics || !object.IsValid())
					return std::make_tuple(0.0f, 0.0f);
				glm::vec2 v = s_Physics->GetLinearVelocity(object.raw);
				return std::make_tuple(v.x, v.y);
			}));

		lua.set_function("AddVelocity", sol::overload(
			[](float x, float y)
			{
				if (!s_Physics || !s_CurrentObject) return;
				s_Physics->AddLinearVelocity(s_CurrentObject, glm::vec2(x, y));
			},
			[readVec2](const sol::table& delta)
			{
				if (!s_Physics || !s_CurrentObject) return;
				float x, y;
				readVec2(delta, x, y);
				s_Physics->AddLinearVelocity(s_CurrentObject, glm::vec2(x, y));
			},
			[](const ScriptObject& object, float x, float y)
			{
				if (!s_Physics || !object.IsValid()) return;
				s_Physics->AddLinearVelocity(object.raw, glm::vec2(x, y));
			}));

		lua.set_function("ApplyForce", sol::overload(
			[](float x, float y)
			{
				if (!s_Physics || !s_CurrentObject) return;
				s_Physics->ApplyForce(s_CurrentObject, glm::vec2(x, y));
			},
			[readVec2](const sol::table& force)
			{
				if (!s_Physics || !s_CurrentObject) return;
				float x, y;
				readVec2(force, x, y);
				s_Physics->ApplyForce(s_CurrentObject, glm::vec2(x, y));
			},
			[](const ScriptObject& object, float x, float y)
			{
				if (!s_Physics || !object.IsValid()) return;
				s_Physics->ApplyForce(object.raw, glm::vec2(x, y));
			}));

		lua.set_function("ApplyImpulse", sol::overload(
			[](float x, float y)
			{
				if (!s_Physics || !s_CurrentObject) return;
				s_Physics->ApplyImpulse(s_CurrentObject, glm::vec2(x, y));
			},
			[readVec2](const sol::table& impulse)
			{
				if (!s_Physics || !s_CurrentObject) return;
				float x, y;
				readVec2(impulse, x, y);
				s_Physics->ApplyImpulse(s_CurrentObject, glm::vec2(x, y));
			},
			[](const ScriptObject& object, float x, float y)
			{
				if (!s_Physics || !object.IsValid()) return;
				s_Physics->ApplyImpulse(object.raw, glm::vec2(x, y));
			}));

		lua.set_function("SetGravity", sol::overload(
			[](float x, float y)
			{
				if (!s_Physics) return;
				float sx, sy;
				sx = Camera::m_prop.projection[0][0];
				sy = Camera::m_prop.projection[1][1];
				if (glm::abs(sx) < 1e-8f) sx = 1.0f;
				if (glm::abs(sy) < 1e-8f) sy = 1.0f;
				s_Physics->SetGravity(glm::vec2(x / sx, y / sy));
			},
			[readVec2](const sol::table& gravity)
			{
				if (!s_Physics) return;
				float x, y;
				readVec2(gravity, x, y);
				float sx = Camera::m_prop.projection[0][0];
				float sy = Camera::m_prop.projection[1][1];
				if (glm::abs(sx) < 1e-8f) sx = 1.0f;
				if (glm::abs(sy) < 1e-8f) sy = 1.0f;
				s_Physics->SetGravity(glm::vec2(x / sx, y / sy));
			}));

		lua.set_function("GetGravity", []()
		{
			if (!s_Physics)
				return std::make_tuple(0.0f, 0.0f);
			float sx = Camera::m_prop.projection[0][0];
			float sy = Camera::m_prop.projection[1][1];
			if (glm::abs(sx) < 1e-8f) sx = 1.0f;
			if (glm::abs(sy) < 1e-8f) sy = 1.0f;
			glm::vec2 g = s_Physics->GetGravity();
			return std::make_tuple(g.x * sx, g.y * sy);
		});

		lua.set_function("SetGravityScale", sol::overload(
			[](float scale)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetRigidbodyGravityScale(scale);
			},
			[](const ScriptObject& object, float scale)
			{
				if (object.IsValid())
					object.raw->SetRigidbodyGravityScale(scale);
			}));

		lua.set_function("GetGravityScale", sol::overload(
			[]()
			{
				return s_CurrentObject ? s_CurrentObject->GetRigidbodyGravityScale() : 1.0f;
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() ? object.raw->GetRigidbodyGravityScale() : 1.0f;
			}));

		lua.set_function("SetKinematic", sol::overload(
			[](bool kinematic)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetRigidbodyKinematic(kinematic);
			},
			[](const ScriptObject& object, bool kinematic)
			{
				if (object.IsValid())
					object.raw->SetRigidbodyKinematic(kinematic);
			}));

		lua.set_function("IsKinematic", sol::overload(
			[]()
			{
				return s_CurrentObject && s_CurrentObject->IsRigidbodyKinematic();
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() && object.raw->IsRigidbodyKinematic();
			}));

		lua.set_function("SetMass", sol::overload(
			[](float mass)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetRigidbodyMass(mass);
			},
			[](const ScriptObject& object, float mass)
			{
				if (object.IsValid())
					object.raw->SetRigidbodyMass(mass);
			}));

		lua.set_function("GetMass", sol::overload(
			[]()
			{
				return s_CurrentObject ? s_CurrentObject->GetRigidbodyMass() : 1.0f;
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() ? object.raw->GetRigidbodyMass() : 1.0f;
			}));

		lua.set_function("HasPhysics", sol::overload(
			[]()
			{
				return s_CurrentObject
					&& s_CurrentObject->HasCollision()
					&& s_CurrentObject->GetCollisionType() == CollisionType::PhysicsRigidbody2D;
			},
			[](const ScriptObject& object)
			{
				return object.IsValid()
					&& object.raw->HasCollision()
					&& object.raw->GetCollisionType() == CollisionType::PhysicsRigidbody2D;
			}));

		lua.set_function("SetTrigger", sol::overload(
			[](bool trigger)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetIsTrigger(trigger);
			},
			[](const ScriptObject& object, bool trigger)
			{
				if (object.IsValid())
					object.raw->SetIsTrigger(trigger);
			}));

		lua.set_function("IsTrigger", sol::overload(
			[]()
			{
				return s_CurrentObject && s_CurrentObject->IsTrigger();
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() && object.raw->IsTrigger();
			}));

		lua.set_function("SetCollisionLayer", sol::overload(
			[](int layer)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetCollisionLayer(layer);
			},
			[](const ScriptObject& object, int layer)
			{
				if (object.IsValid())
					object.raw->SetCollisionLayer(layer);
			}));

		lua.set_function("GetCollisionLayer", sol::overload(
			[]()
			{
				return s_CurrentObject ? s_CurrentObject->GetCollisionLayer() : 0;
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() ? object.raw->GetCollisionLayer() : 0;
			}));

		lua.set_function("SetCollisionMask", sol::overload(
			[](uint32_t mask)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetCollisionMask(mask);
			},
			[](const ScriptObject& object, uint32_t mask)
			{
				if (object.IsValid())
					object.raw->SetCollisionMask(mask);
			}));

		lua.set_function("GetCollisionMask", sol::overload(
			[]()
			{
				return s_CurrentObject ? s_CurrentObject->GetCollisionMask() : 0u;
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() ? object.raw->GetCollisionMask() : 0u;
			}));

		lua.set_function("SetCollidesWithLayer", sol::overload(
			[](int layer, bool enabled)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetCollidesWithLayer(layer, enabled);
			},
			[](const ScriptObject& object, int layer, bool enabled)
			{
				if (object.IsValid())
					object.raw->SetCollidesWithLayer(layer, enabled);
			}));

		lua["LAYER_DEFAULT"] = CollisionLayer_Default;
		lua["LAYER_PLAYER"] = CollisionLayer_Player;
		lua["LAYER_ENEMY"] = CollisionLayer_Enemy;
		lua["LAYER_PROJECTILE"] = CollisionLayer_Projectile;
		lua["LAYER_TRIGGER"] = CollisionLayer_Trigger;
		lua["LAYER_GROUND"] = CollisionLayer_Ground;
		lua["LAYER_PICKUP"] = CollisionLayer_Pickup;

		lua.set_function("PlayAudio", sol::overload(
			[]()
			{
				if (s_CurrentObject)
					AudioEngine::Play(s_CurrentObject);
			},
			[](const ScriptObject& object)
			{
				if (object.IsValid())
					AudioEngine::Play(object.raw);
			}));

		lua.set_function("StopAudio", sol::overload(
			[]()
			{
				if (s_CurrentObject)
					AudioEngine::Stop(s_CurrentObject);
			},
			[](const ScriptObject& object)
			{
				if (object.IsValid())
					AudioEngine::Stop(object.raw);
			}));

		lua.set_function("IsAudioPlaying", sol::overload(
			[]()
			{
				return s_CurrentObject && AudioEngine::IsPlaying(s_CurrentObject);
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() && AudioEngine::IsPlaying(object.raw);
			}));

		lua.set_function("SetAudioVolume", sol::overload(
			[](float volume)
			{
				if (!s_CurrentObject)
					return;
				s_CurrentObject->SetAudioVolume(volume);
				AudioEngine::SyncVolume(s_CurrentObject);
			},
			[](const ScriptObject& object, float volume)
			{
				if (!object.IsValid())
					return;
				object.raw->SetAudioVolume(volume);
				AudioEngine::SyncVolume(object.raw);
			}));

		lua.set_function("GetAudioVolume", sol::overload(
			[]()
			{
				return s_CurrentObject ? s_CurrentObject->GetAudioVolume() : 1.0f;
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() ? object.raw->GetAudioVolume() : 1.0f;
			}));

		lua.set_function("SetAudioLoop", sol::overload(
			[](bool loop)
			{
				if (!s_CurrentObject)
					return;
				s_CurrentObject->SetAudioLoop(loop);
				AudioEngine::SyncLoop(s_CurrentObject);
			},
			[](const ScriptObject& object, bool loop)
			{
				if (!object.IsValid())
					return;
				object.raw->SetAudioLoop(loop);
				AudioEngine::SyncLoop(object.raw);
			}));

		lua.set_function("GetAudioLoop", sol::overload(
			[]()
			{
				return s_CurrentObject && s_CurrentObject->GetAudioLoop();
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() && object.raw->GetAudioLoop();
			}));

		lua.set_function("SetAudioPath", sol::overload(
			[](const std::string& path)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetAudioPath(path);
			},
			[](const ScriptObject& object, const std::string& path)
			{
				if (object.IsValid())
					object.raw->SetAudioPath(path);
			}));

		lua.set_function("GetAudioPath", sol::overload(
			[]()
			{
				return s_CurrentObject ? s_CurrentObject->GetAudioPath() : std::string();
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() ? object.raw->GetAudioPath() : std::string();
			}));

		lua.set_function("HasAudio", sol::overload(
			[]()
			{
				return s_CurrentObject && s_CurrentObject->HasAudio();
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() && object.raw->HasAudio();
			}));

		lua.set_function("SetFlipbook", sol::overload(
			[](int columns, int rows)
			{
				if (!s_CurrentObject) return;
				s_CurrentObject->SetFlipbookColumns(columns);
				s_CurrentObject->SetFlipbookRows(rows);
			},
			[](int columns, int rows, float fps)
			{
				if (!s_CurrentObject) return;
				s_CurrentObject->SetFlipbookColumns(columns);
				s_CurrentObject->SetFlipbookRows(rows);
				s_CurrentObject->SetFlipbookFps(fps);
			},
			[](const ScriptObject& object, int columns, int rows)
			{
				if (!object.IsValid()) return;
				object.raw->SetFlipbookColumns(columns);
				object.raw->SetFlipbookRows(rows);
			},
			[](const ScriptObject& object, int columns, int rows, float fps)
			{
				if (!object.IsValid()) return;
				object.raw->SetFlipbookColumns(columns);
				object.raw->SetFlipbookRows(rows);
				object.raw->SetFlipbookFps(fps);
			}));

		lua.set_function("SetFlipbookFrameCount", sol::overload(
			[](int count)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetFlipbookFrameCount(count);
			},
			[](const ScriptObject& object, int count)
			{
				if (object.IsValid())
					object.raw->SetFlipbookFrameCount(count);
			}));

		lua.set_function("SetFlipbookFps", sol::overload(
			[](float fps)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetFlipbookFps(fps);
			},
			[](const ScriptObject& object, float fps)
			{
				if (object.IsValid())
					object.raw->SetFlipbookFps(fps);
			}));

		lua.set_function("PlayFlipbook", sol::overload(
			[]()
			{
				if (s_CurrentObject)
					s_CurrentObject->PlayFlipbook();
			},
			[](const ScriptObject& object)
			{
				if (object.IsValid())
					object.raw->PlayFlipbook();
			}));

		lua.set_function("StopFlipbook", sol::overload(
			[]()
			{
				if (s_CurrentObject)
					s_CurrentObject->StopFlipbook();
			},
			[](const ScriptObject& object)
			{
				if (object.IsValid())
					object.raw->StopFlipbook();
			}));

		lua.set_function("PauseFlipbook", sol::overload(
			[]()
			{
				if (s_CurrentObject)
					s_CurrentObject->PauseFlipbook();
			},
			[](const ScriptObject& object)
			{
				if (object.IsValid())
					object.raw->PauseFlipbook();
			}));

		lua.set_function("SetFlipbookFrame", sol::overload(
			[](int frame)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetFlipbookFrame(frame);
			},
			[](const ScriptObject& object, int frame)
			{
				if (object.IsValid())
					object.raw->SetFlipbookFrame(frame);
			}));

		lua.set_function("GetFlipbookFrame", sol::overload(
			[]()
			{
				return s_CurrentObject ? s_CurrentObject->GetFlipbookFrame() : 0;
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() ? object.raw->GetFlipbookFrame() : 0;
			}));

		lua.set_function("SetFlipbookLoop", sol::overload(
			[](bool loop)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetFlipbookLoop(loop);
			},
			[](const ScriptObject& object, bool loop)
			{
				if (object.IsValid())
					object.raw->SetFlipbookLoop(loop);
			}));

		lua.set_function("IsFlipbookPlaying", sol::overload(
			[]()
			{
				return s_CurrentObject && s_CurrentObject->IsFlipbookPlaying();
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() && object.raw->IsFlipbookPlaying();
			}));

		lua.set_function("SetTilemapSize", sol::overload(
			[](int columns, int rows)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetTilemapSize(columns, rows);
			},
			[](const ScriptObject& object, int columns, int rows)
			{
				if (object.IsValid())
					object.raw->SetTilemapSize(columns, rows);
			}));

		lua.set_function("SetTileCellSize", sol::overload(
			[](float size)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetTileCellSize(size);
			},
			[](const ScriptObject& object, float size)
			{
				if (object.IsValid())
					object.raw->SetTileCellSize(size);
			}));

		lua.set_function("SetTileset", sol::overload(
			[](int columns, int rows)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetTilesetGrid(columns, rows);
			},
			[](const ScriptObject& object, int columns, int rows)
			{
				if (object.IsValid())
					object.raw->SetTilesetGrid(columns, rows);
			}));

		lua.set_function("SetTile", sol::overload(
			[](int x, int y, int tileId)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetTile(x, y, tileId);
			},
			[](const ScriptObject& object, int x, int y, int tileId)
			{
				if (object.IsValid())
					object.raw->SetTile(x, y, tileId);
			}));

		lua.set_function("GetTile", sol::overload(
			[](int x, int y)
			{
				return s_CurrentObject ? s_CurrentObject->GetTile(x, y) : -1;
			},
			[](const ScriptObject& object, int x, int y)
			{
				return object.IsValid() ? object.raw->GetTile(x, y) : -1;
			}));

		lua.set_function("FillTiles", sol::overload(
			[](int tileId)
			{
				if (s_CurrentObject)
					s_CurrentObject->FillTiles(tileId);
			},
			[](const ScriptObject& object, int tileId)
			{
				if (object.IsValid())
					object.raw->FillTiles(tileId);
			}));

		lua.set_function("ClearTiles", sol::overload(
			[]()
			{
				if (s_CurrentObject)
					s_CurrentObject->ClearTiles();
			},
			[](const ScriptObject& object)
			{
				if (object.IsValid())
					object.raw->ClearTiles();
			}));

		lua.set_function("GetTilemapSize", sol::overload(
			[]()
			{
				if (!s_CurrentObject)
					return std::make_tuple(0, 0);
				return std::make_tuple(s_CurrentObject->GetTilemapColumns(), s_CurrentObject->GetTilemapRows());
			},
			[](const ScriptObject& object)
			{
				if (!object.IsValid())
					return std::make_tuple(0, 0);
				return std::make_tuple(object.raw->GetTilemapColumns(), object.raw->GetTilemapRows());
			}));

		lua.set_function("SetParent", sol::overload(
			[](const ScriptObject& parent)
			{
				if (!s_CurrentObject)
					return false;
				if (!parent.IsValid())
					return s_CurrentObject->SetParent(nullptr, true);
				return s_CurrentObject->SetParent(parent.raw, true);
			},
			[]()
			{
				if (!s_CurrentObject)
					return false;
				return s_CurrentObject->SetParent(nullptr, true);
			},
			[](const ScriptObject& object, const ScriptObject& parent)
			{
				if (!object.IsValid())
					return false;
				if (!parent.IsValid())
					return object.raw->SetParent(nullptr, true);
				return object.raw->SetParent(parent.raw, true);
			}));

		lua.set_function("GetParent", sol::overload(
			[]() -> sol::object
			{
				if (!s_CurrentObject || !s_CurrentObject->GetParent() || !s_Lua)
					return sol::make_object(s_Lua->lua_state(), sol::lua_nil);
				return sol::make_object(*s_Lua, ScriptObject(s_CurrentObject->GetParent()));
			},
			[](const ScriptObject& object) -> sol::object
			{
				if (!object.IsValid() || !object.raw->GetParent() || !s_Lua)
					return sol::make_object(s_Lua->lua_state(), sol::lua_nil);
				return sol::make_object(*s_Lua, ScriptObject(object.raw->GetParent()));
			}));

		lua.set_function("ClearParent", sol::overload(
			[]()
			{
				if (s_CurrentObject)
					s_CurrentObject->ClearParent(true);
			},
			[](const ScriptObject& object)
			{
				if (object.IsValid())
					object.raw->ClearParent(true);
			}));

		lua.set_function("GetWorldLocation", sol::overload(
			[]()
			{
				if (!s_CurrentObject)
					return std::make_tuple(0.0f, 0.0f);
				glm::vec3 p = s_CurrentObject->GetWorldPosition();
				return std::make_tuple(p.x, p.y);
			},
			[](const ScriptObject& object)
			{
				if (!object.IsValid())
					return std::make_tuple(0.0f, 0.0f);
				glm::vec3 p = object.raw->GetWorldPosition();
				return std::make_tuple(p.x, p.y);
			}));
	}

	bool ScriptEngine::Init()
	{
		if (s_Ready)
			return true;

		try
		{
			s_Lua = std::make_unique<sol::state>();
			lua_atpanic(s_Lua->lua_state(), [](lua_State* L) -> int
			{
				const char* msg = lua_tostring(L, -1);
				std::fprintf(stderr, "\n*** LUA PANIC (no abort): %s ***\n\n", msg ? msg : "(null)");
				std::fflush(stderr);
				BZ_ERROR("LUA PANIC: {0}", msg ? msg : "(null)");
				// Throw instead of letting Lua abort the process
				throw std::runtime_error(msg ? msg : "Lua panic");
			});
			BindApi(*s_Lua);
			s_Ready = true;
			BZ_INFO("ScriptEngine ready (Lua)");
			return true;
		}
		catch (const std::exception& e)
		{
			BZ_ERROR("ScriptEngine init failed: {0}", e.what());
			s_Lua.reset();
			s_Ready = false;
			return false;
		}
	}

	void ScriptEngine::Shutdown()
	{
		OnPlayModeStop();
		s_PendingOpenMap.clear();
		s_Renderer = nullptr;
		s_Physics = nullptr;
		s_Lua.reset();
		s_Ready = false;
	}

	void ScriptEngine::SetRenderer(Renderer* renderer)
	{
		s_Renderer = renderer;
	}

	void ScriptEngine::SetPhysicsWorld(PhysicsWorld2D* physics)
	{
		s_Physics = physics;
	}

	GameObject* ScriptEngine::FindByTag(const std::string& tag)
	{
		if (!s_Renderer || tag.empty())
			return nullptr;
		for (GameObject* object : s_Renderer->GetObjects())
		{
			if (object && s_LiveObjects.find(object) != s_LiveObjects.end() && object->HasTag(tag))
				return object;
		}
		return nullptr;
	}

	std::vector<GameObject*> ScriptEngine::FindAllByTag(const std::string& tag)
	{
		std::vector<GameObject*> result;
		if (!s_Renderer || tag.empty())
			return result;
		for (GameObject* object : s_Renderer->GetObjects())
		{
			if (object && s_LiveObjects.find(object) != s_LiveObjects.end() && object->HasTag(tag))
				result.push_back(object);
		}
		return result;
	}

	std::string ScriptEngine::GetCurrentMapName()
	{
		const std::string& active = Project::GetActiveMap();
		if (active.empty())
			return {};
		return std::filesystem::path(active).stem().string();
	}

	bool ScriptEngine::RequestOpenMap(const std::string& mapNameOrPath)
	{
		const std::string resolved = Project::ResolveMapPath(mapNameOrPath);
		if (resolved.empty())
		{
			BZ_ERROR("OpenMap failed: map not found '{0}'", mapNameOrPath);
			return false;
		}

		s_PendingOpenMap = resolved;
		BZ_INFO("OpenMap queued: {0}", resolved);
		return true;
	}

	bool ScriptEngine::ConsumePendingOpenMap(std::string& outPath)
	{
		if (s_PendingOpenMap.empty())
			return false;
		outPath = s_PendingOpenMap;
		s_PendingOpenMap.clear();
		return true;
	}

	static bool AttachRuntimeScript(GameObject* object, bool callStart)
	{
		if (!object || !object->HasScript() || !s_Lua || !s_Ready)
			return false;
		if (object->GetScriptPath().empty())
			return false;
		if (s_Instances.find(object) != s_Instances.end())
			return true;

		ScriptInstance instance;
		instance.object = object;
		instance.env = sol::environment(*s_Lua, sol::create, s_Lua->globals());
		instance.env["selfName"] = object->GetName();
		instance.env["self"] = ScriptObject(object);

		sol::protected_function_result result = s_Lua->safe_script_file(
			object->GetScriptPath(),
			instance.env,
			sol::script_pass_on_error);
		if (!result.valid())
		{
			sol::error err = result;
			ReportLuaError("Spawn / compile", object, object->GetScriptPath(), err.what());
			return false;
		}

		sol::object startObj = instance.env["OnStart"];
		sol::object updateObj = instance.env["OnUpdate"];
		sol::object beginObj = instance.env["OnCollisionBegin"];
		if (!beginObj.valid() || beginObj.get_type() == sol::type::lua_nil)
			beginObj = instance.env["OnCollisionEnter"];
		sol::object endObj = instance.env["OnCollisionEnd"];
		if (!endObj.valid() || endObj.get_type() == sol::type::lua_nil)
			endObj = instance.env["OnCollisionExit"];

		if (startObj.is<sol::protected_function>())
			instance.onStart = startObj.as<sol::protected_function>();
		else if (startObj.is<sol::function>())
			instance.onStart = startObj.as<sol::function>();
		if (updateObj.is<sol::protected_function>())
			instance.onUpdate = updateObj.as<sol::protected_function>();
		else if (updateObj.is<sol::function>())
			instance.onUpdate = updateObj.as<sol::function>();
		if (beginObj.is<sol::protected_function>())
			instance.onCollisionBegin = beginObj.as<sol::protected_function>();
		else if (beginObj.is<sol::function>())
			instance.onCollisionBegin = beginObj.as<sol::function>();
		if (endObj.is<sol::protected_function>())
			instance.onCollisionEnd = endObj.as<sol::protected_function>();
		else if (endObj.is<sol::function>())
			instance.onCollisionEnd = endObj.as<sol::function>();

		sol::object triggerBeginObj = instance.env["OnTriggerBegin"];
		if (!triggerBeginObj.valid() || triggerBeginObj.get_type() == sol::type::lua_nil)
			triggerBeginObj = instance.env["OnTriggerEnter"];
		sol::object triggerEndObj = instance.env["OnTriggerEnd"];
		if (!triggerEndObj.valid() || triggerEndObj.get_type() == sol::type::lua_nil)
			triggerEndObj = instance.env["OnTriggerExit"];
		if (triggerBeginObj.is<sol::protected_function>())
			instance.onTriggerBegin = triggerBeginObj.as<sol::protected_function>();
		else if (triggerBeginObj.is<sol::function>())
			instance.onTriggerBegin = triggerBeginObj.as<sol::function>();
		if (triggerEndObj.is<sol::protected_function>())
			instance.onTriggerEnd = triggerEndObj.as<sol::protected_function>();
		else if (triggerEndObj.is<sol::function>())
			instance.onTriggerEnd = triggerEndObj.as<sol::function>();

		sol::object widgetClickedObj = instance.env["OnWidgetClicked"];
		if (widgetClickedObj.is<sol::protected_function>())
			instance.onWidgetClicked = widgetClickedObj.as<sol::protected_function>();
		else if (widgetClickedObj.is<sol::function>())
			instance.onWidgetClicked = widgetClickedObj.as<sol::function>();

		if (callStart && instance.onStart.valid())
		{
			GameObject* prev = s_CurrentObject;
			s_CurrentObject = object;
			sol::protected_function_result startResult = instance.onStart();
			if (!startResult.valid())
			{
				sol::error err = startResult;
				ReportLuaError("Spawn OnStart", object, object->GetScriptPath(), err.what());
			}
			s_CurrentObject = prev;
			instance.started = true;
		}

		StorePlayTransform(object);
		s_Instances[object] = std::move(instance);
		return true;
	}

	GameObject* ScriptEngine::SpawnObject(const std::string& typeName,
		const std::string& name,
		const std::string& tag,
		float x, float y, float z)
	{
		if (!s_Renderer || !s_Ready)
			return nullptr;

		s_Renderer->CreateObject(s_DefaultVertices, s_DefaultIndices);
		if (s_Renderer->GetObjects().empty())
			return nullptr;

		GameObject* object = s_Renderer->GetObjects().back();
		std::string type = typeName;
		for (char& c : type)
			c = (char)std::tolower((unsigned char)c);

		if (type == "circle")
		{
			object->SetName(name.empty() ? "Circle" : name);
			object->SetType(ObjectType::Circle);
			object->AddCollision(CollisionType::Circle);
			object->SetCollisionRadius(50.0f);
		}
		else if (type == "box")
		{
			object->SetName(name.empty() ? "Box" : name);
			object->SetType(ObjectType::Box);
			object->AddCollision(CollisionType::Block);
			object->SetCollisionSize(glm::vec2(100.0f, 100.0f));
		}
		else if (type == "camera")
		{
			object->SetName(name.empty() ? "Camera" : name);
			object->SetType(ObjectType::Camera);
		}
		else if (type == "sound")
		{
			object->SetName(name.empty() ? "Sound" : name);
			object->SetType(ObjectType::Sound);
			object->AddAudio();
		}
		else if (type == "empty")
		{
			object->SetName(name.empty() ? "Empty" : name);
			object->SetType(ObjectType::Empty);
		}
		else if (type == "tilemap")
		{
			object->SetName(name.empty() ? "Tilemap" : name);
			object->SetType(ObjectType::Tilemap);
			object->InitTilemapDefaults();
			object->SetTexture("content/texture/container.jpg");
			object->SetTilesetGrid(1, 1);
		}
		else if (type == "text")
		{
			object->SetName(name.empty() ? "Text" : name);
			object->SetType(ObjectType::Text);
			object->SetText("Text");
			object->SetFontSize(48.0f);
		}
		else
		{
			object->SetName(name.empty() ? "Sprite" : name);
			object->SetType(ObjectType::Sprite);
			object->SetTexture("content/texture/container.jpg");
		}

		if (!name.empty())
			object->SetName(name);
		if (!tag.empty())
		{
			if (tag.find(',') != std::string::npos || tag.find(';') != std::string::npos)
				object->SetTagsFromString(tag);
			else
				object->SetTag(tag);
		}

		glm::vec3 pos(x, y, z);
		object->SetPosition(pos);
		s_LiveObjects.insert(object);
		StorePlayTransform(object);

		if (object->HasScript())
			AttachRuntimeScript(object, true);

		return object;
	}

	bool ScriptEngine::IsReady()
	{
		return s_Ready;
	}

	void ScriptEngine::RemoveObject(GameObject* object)
	{
		if (!object)
			return;
		s_Instances.erase(object);
		s_PlayTransforms.erase(object);
		s_LiveObjects.erase(object);
		if (s_CurrentObject == object)
			s_CurrentObject = nullptr;

		s_PendingDestroy.erase(
			std::remove(s_PendingDestroy.begin(), s_PendingDestroy.end(), object),
			s_PendingDestroy.end());
	}

	void ScriptEngine::QueueDestroy(GameObject* object)
	{
		if (!object)
			return;
		if (s_LiveObjects.find(object) == s_LiveObjects.end())
			return;
		for (GameObject* pending : s_PendingDestroy)
		{
			if (pending == object)
				return;
		}
		s_PendingDestroy.push_back(object);
	}

	std::vector<GameObject*> ScriptEngine::ConsumePendingDestroys()
	{
		std::vector<GameObject*> out;
		out.swap(s_PendingDestroy);
		for (GameObject* object : out)
			s_LiveObjects.erase(object);
		return out;
	}

	void ScriptEngine::OnPlayModeStop()
	{
		s_Instances.clear();
		s_MapInstance = ScriptInstance{};
		s_HasMapInstance = false;
		s_PlayTransforms.clear();
		s_KeyWasDown.clear();
		s_MouseWasDown.clear();
		s_PendingDestroy.clear();
		s_LiveObjects.clear();
		s_CurrentObject = nullptr;
		OnScreenDebug::Clear();
	}

	static bool BindScriptCallbacks(ScriptInstance& instance)
	{
		sol::object startObj = instance.env["OnStart"];
		sol::object updateObj = instance.env["OnUpdate"];
		sol::object beginObj = instance.env["OnCollisionBegin"];
		if (!beginObj.valid() || beginObj.get_type() == sol::type::lua_nil)
			beginObj = instance.env["OnCollisionEnter"];
		sol::object endObj = instance.env["OnCollisionEnd"];
		if (!endObj.valid() || endObj.get_type() == sol::type::lua_nil)
			endObj = instance.env["OnCollisionExit"];

		if (startObj.is<sol::protected_function>())
			instance.onStart = startObj.as<sol::protected_function>();
		else if (startObj.is<sol::function>())
			instance.onStart = startObj.as<sol::function>();

		if (updateObj.is<sol::protected_function>())
			instance.onUpdate = updateObj.as<sol::protected_function>();
		else if (updateObj.is<sol::function>())
			instance.onUpdate = updateObj.as<sol::function>();

		if (beginObj.is<sol::protected_function>())
			instance.onCollisionBegin = beginObj.as<sol::protected_function>();
		else if (beginObj.is<sol::function>())
			instance.onCollisionBegin = beginObj.as<sol::function>();

		if (endObj.is<sol::protected_function>())
			instance.onCollisionEnd = endObj.as<sol::protected_function>();
		else if (endObj.is<sol::function>())
			instance.onCollisionEnd = endObj.as<sol::function>();

		sol::object triggerBeginObj = instance.env["OnTriggerBegin"];
		if (!triggerBeginObj.valid() || triggerBeginObj.get_type() == sol::type::lua_nil)
			triggerBeginObj = instance.env["OnTriggerEnter"];
		sol::object triggerEndObj = instance.env["OnTriggerEnd"];
		if (!triggerEndObj.valid() || triggerEndObj.get_type() == sol::type::lua_nil)
			triggerEndObj = instance.env["OnTriggerExit"];
		if (triggerBeginObj.is<sol::protected_function>())
			instance.onTriggerBegin = triggerBeginObj.as<sol::protected_function>();
		else if (triggerBeginObj.is<sol::function>())
			instance.onTriggerBegin = triggerBeginObj.as<sol::function>();
		if (triggerEndObj.is<sol::protected_function>())
			instance.onTriggerEnd = triggerEndObj.as<sol::protected_function>();
		else if (triggerEndObj.is<sol::function>())
			instance.onTriggerEnd = triggerEndObj.as<sol::function>();

		sol::object widgetClickedObj = instance.env["OnWidgetClicked"];
		if (widgetClickedObj.is<sol::protected_function>())
			instance.onWidgetClicked = widgetClickedObj.as<sol::protected_function>();
		else if (widgetClickedObj.is<sol::function>())
			instance.onWidgetClicked = widgetClickedObj.as<sol::function>();

		return true;
	}

	static bool LoadMapScriptInstance(bool& compileOk)
	{
		s_HasMapInstance = false;
		s_MapInstance = ScriptInstance{};

		const std::string& mapScript = Project::GetMapScript();
		if (mapScript.empty())
			return true;

		std::error_code ec;
		if (!std::filesystem::exists(mapScript, ec))
		{
			ReportLuaError("Load / compile map script (press Start)", nullptr, mapScript,
				"Map script file not found");
			compileOk = false;
			return false;
		}

		ScriptInstance instance;
		instance.object = nullptr;
		instance.env = sol::environment(*s_Lua, sol::create, s_Lua->globals());
		instance.env["selfName"] = "[Map]";
		instance.env["mapName"] = std::filesystem::path(Project::GetActiveMap()).stem().string();
		instance.env["self"] = sol::lua_nil;

		sol::protected_function_result result = s_Lua->safe_script_file(
			mapScript,
			instance.env,
			sol::script_pass_on_error);

		if (!result.valid())
		{
			sol::error err = result;
			ReportLuaError("Load / compile map script (press Start)", nullptr, mapScript, err.what());
			compileOk = false;
			return false;
		}

		BindScriptCallbacks(instance);
		s_MapInstance = std::move(instance);
		s_HasMapInstance = true;
		return true;
	}

	bool ScriptEngine::OnPlayModeStart(const std::vector<GameObject*>& objects)
	{
		OnPlayModeStop();
		ClearScriptErrors();
		if (!Init())
			return false;

		for (GameObject* object : objects)
		{
			if (object)
				s_LiveObjects.insert(object);
		}

		CaptureInitialPlayTransforms(objects);

		bool compileOk = true;
		for (GameObject* object : objects)
		{
			if (!object || !object->HasScript())
				continue;
			if (object->GetScriptPath().empty())
			{
				ReportLuaError("Load / compile (press Start)", object, "",
					"Script component has no file assigned");
				compileOk = false;
				continue;
			}

			ScriptInstance instance;
			instance.object = object;
			instance.env = sol::environment(*s_Lua, sol::create, s_Lua->globals());
			instance.env["selfName"] = object->GetName();
			instance.env["self"] = ScriptObject(object);

			// MUST use safe_script_file — unprotected script_file aborts the process on errors.
			sol::protected_function_result result = s_Lua->safe_script_file(
				object->GetScriptPath(),
				instance.env,
				sol::script_pass_on_error);

			if (!result.valid())
			{
				sol::error err = result;
				ReportLuaError("Load / compile (press Start)", object, object->GetScriptPath(), err.what());
				compileOk = false;
				continue;
			}

			sol::object startObj = instance.env["OnStart"];
			sol::object updateObj = instance.env["OnUpdate"];
			sol::object beginObj = instance.env["OnCollisionBegin"];
			if (!beginObj.valid() || beginObj.get_type() == sol::type::lua_nil)
				beginObj = instance.env["OnCollisionEnter"];
			sol::object endObj = instance.env["OnCollisionEnd"];
			if (!endObj.valid() || endObj.get_type() == sol::type::lua_nil)
				endObj = instance.env["OnCollisionExit"];

			if (startObj.is<sol::protected_function>())
				instance.onStart = startObj.as<sol::protected_function>();
			else if (startObj.is<sol::function>())
				instance.onStart = startObj.as<sol::function>();

			if (updateObj.is<sol::protected_function>())
				instance.onUpdate = updateObj.as<sol::protected_function>();
			else if (updateObj.is<sol::function>())
				instance.onUpdate = updateObj.as<sol::function>();

			if (beginObj.is<sol::protected_function>())
				instance.onCollisionBegin = beginObj.as<sol::protected_function>();
			else if (beginObj.is<sol::function>())
				instance.onCollisionBegin = beginObj.as<sol::function>();

			if (endObj.is<sol::protected_function>())
				instance.onCollisionEnd = endObj.as<sol::protected_function>();
			else if (endObj.is<sol::function>())
				instance.onCollisionEnd = endObj.as<sol::function>();

			sol::object triggerBeginObj = instance.env["OnTriggerBegin"];
			if (!triggerBeginObj.valid() || triggerBeginObj.get_type() == sol::type::lua_nil)
				triggerBeginObj = instance.env["OnTriggerEnter"];
			sol::object triggerEndObj = instance.env["OnTriggerEnd"];
			if (!triggerEndObj.valid() || triggerEndObj.get_type() == sol::type::lua_nil)
				triggerEndObj = instance.env["OnTriggerExit"];
			if (triggerBeginObj.is<sol::protected_function>())
				instance.onTriggerBegin = triggerBeginObj.as<sol::protected_function>();
			else if (triggerBeginObj.is<sol::function>())
				instance.onTriggerBegin = triggerBeginObj.as<sol::function>();
			if (triggerEndObj.is<sol::protected_function>())
				instance.onTriggerEnd = triggerEndObj.as<sol::protected_function>();
			else if (triggerEndObj.is<sol::function>())
				instance.onTriggerEnd = triggerEndObj.as<sol::function>();

			sol::object widgetClickedObj = instance.env["OnWidgetClicked"];
			if (widgetClickedObj.is<sol::protected_function>())
				instance.onWidgetClicked = widgetClickedObj.as<sol::protected_function>();
			else if (widgetClickedObj.is<sol::function>())
				instance.onWidgetClicked = widgetClickedObj.as<sol::function>();

			s_Instances[object] = std::move(instance);
		}

		LoadMapScriptInstance(compileOk);

		if (!compileOk)
		{
			BZ_ERROR("Play cancelled — fix Lua errors before Start");
			std::fprintf(stderr, "\n*** Play cancelled — fix Lua errors before Start ***\n\n");
			std::fflush(stderr);
			s_Instances.clear();
			s_MapInstance = ScriptInstance{};
			s_HasMapInstance = false;
			s_PlayTransforms.clear();
			s_CurrentObject = nullptr;
			return false;
		}

		ApplyPlayTransforms();
		try
		{
			if (s_HasMapInstance && s_MapInstance.onStart.valid())
			{
				s_CurrentObject = nullptr;
				s_MapInstance.env["self"] = sol::lua_nil;
				sol::protected_function_result result = s_MapInstance.onStart();
				if (!result.valid())
				{
					sol::error err = result;
					ReportLuaError("Map OnStart (press Start)", nullptr,
						Project::GetMapScript(), err.what());
				}
			}

			for (auto& entry : s_Instances)
			{
				ScriptInstance& instance = entry.second;
				s_CurrentObject = instance.object;
				instance.env["self"] = ScriptObject(instance.object);
				if (!instance.onStart.valid())
				{
					StorePlayTransform(instance.object);
					continue;
				}

				sol::protected_function_result result = instance.onStart();
				if (!result.valid())
				{
					sol::error err = result;
					ReportLuaError("OnStart (press Start)", instance.object,
						instance.object ? instance.object->GetScriptPath() : "", err.what());
				}
				instance.started = true;
				StorePlayTransform(instance.object);
			}
		}
		catch (const std::exception& e)
		{
			ReportLuaError("OnStart (exception)", s_CurrentObject, "", e.what());
		}
		s_CurrentObject = nullptr;
		return true;
	}

	void ScriptEngine::OnUpdate(float dt, const std::vector<GameObject*>& /*objects*/)
	{
		if (!s_Ready)
			return;

		ApplyPlayTransforms();

		try
		{
			if (s_HasMapInstance && s_MapInstance.onUpdate.valid())
			{
				s_CurrentObject = nullptr;
				s_MapInstance.env["self"] = sol::lua_nil;
				sol::protected_function_result result = s_MapInstance.onUpdate(dt);
				if (!result.valid())
				{
					sol::error err = result;
					ReportLuaError("Map OnUpdate", nullptr, Project::GetMapScript(), err.what());
				}
			}

			for (auto& entry : s_Instances)
			{
				ScriptInstance& instance = entry.second;
				if (!instance.onUpdate.valid())
					continue;

				s_CurrentObject = instance.object;
				instance.env["self"] = ScriptObject(instance.object);
				sol::protected_function_result result = instance.onUpdate(dt);
				if (!result.valid())
				{
					sol::error err = result;
					ReportLuaError("OnUpdate", instance.object,
						instance.object ? instance.object->GetScriptPath() : "", err.what());
				}
				StorePlayTransform(instance.object);
			}
		}
		catch (const std::exception& e)
		{
			ReportLuaError("OnUpdate (exception)", s_CurrentObject, "", e.what());
		}
		s_CurrentObject = nullptr;

		OnScreenDebug::Update(dt);
		SampleInputEdges();
	}

	static void CallCollisionCallback(ScriptInstance& instance, sol::protected_function& fn,
		GameObject* other, const char* phase)
	{
		if (!fn.valid() || !other)
			return;
		if (s_LiveObjects.find(instance.object) == s_LiveObjects.end())
			return;
		if (s_LiveObjects.find(other) == s_LiveObjects.end())
			return;

		s_CurrentObject = instance.object;
		instance.env["self"] = ScriptObject(instance.object);
		sol::protected_function_result result = fn(ScriptObject(other));
		if (!result.valid())
		{
			sol::error err = result;
			ReportLuaError(phase, instance.object,
				instance.object ? instance.object->GetScriptPath() : "", err.what());
		}
		if (s_LiveObjects.find(instance.object) != s_LiveObjects.end())
			StorePlayTransform(instance.object);
	}

	void ScriptEngine::OnCollisionEvents(const std::vector<CollisionEvent>& events)
	{
		if (!s_Ready || events.empty())
			return;

		try
		{
			for (const CollisionEvent& ev : events)
			{
				if (!ev.a || !ev.b)
					continue;

				const bool begin = (ev.type == CollisionEvent::Type::Begin);
				const char* phase = ev.isTrigger
					? (begin ? "OnTriggerBegin" : "OnTriggerEnd")
					: (begin ? "OnCollisionBegin" : "OnCollisionEnd");

				auto itA = s_Instances.find(ev.a);
				if (itA != s_Instances.end())
				{
					sol::protected_function& fn = ev.isTrigger
						? (begin ? itA->second.onTriggerBegin : itA->second.onTriggerEnd)
						: (begin ? itA->second.onCollisionBegin : itA->second.onCollisionEnd);
					CallCollisionCallback(itA->second, fn, ev.b, phase);
				}

				auto itB = s_Instances.find(ev.b);
				if (itB != s_Instances.end())
				{
					sol::protected_function& fn = ev.isTrigger
						? (begin ? itB->second.onTriggerBegin : itB->second.onTriggerEnd)
						: (begin ? itB->second.onCollisionBegin : itB->second.onCollisionEnd);
					CallCollisionCallback(itB->second, fn, ev.a, phase);
				}
			}
		}
		catch (const std::exception& e)
		{
			ReportLuaError("OnCollision (exception)", s_CurrentObject, "", e.what());
		}
		s_CurrentObject = nullptr;
	}

	void ScriptEngine::NotifyWidgetClicked(const std::string& widgetPath, const std::string& buttonName)
	{
		if (!s_Ready)
			return;

		auto invoke = [&](ScriptInstance& instance)
		{
			if (!instance.onWidgetClicked.valid())
				return;
			GameObject* prev = s_CurrentObject;
			s_CurrentObject = instance.object;
			if (instance.object)
				instance.env["self"] = ScriptObject(instance.object);
			else
				instance.env["self"] = sol::lua_nil;
			sol::protected_function_result result = instance.onWidgetClicked(widgetPath, buttonName);
			if (!result.valid())
			{
				sol::error err = result;
				ReportLuaError("OnWidgetClicked", instance.object,
					instance.object ? instance.object->GetScriptPath() : Project::GetMapScript(),
					err.what());
			}
			s_CurrentObject = prev;
		};

		try
		{
			if (s_HasMapInstance)
				invoke(s_MapInstance);
			for (auto& entry : s_Instances)
				invoke(entry.second);
		}
		catch (const std::exception& e)
		{
			ReportLuaError("OnWidgetClicked (exception)", s_CurrentObject, "", e.what());
			s_CurrentObject = nullptr;
		}
	}

	bool ScriptEngine::HasScriptErrors()
	{
		return !s_ScriptErrors.empty();
	}

	void ScriptEngine::ClearScriptErrors()
	{
		s_ScriptErrors.clear();
		s_ShowErrorWindow = false;
	}

	void ScriptEngine::DrawErrorWindow()
	{
		if (!s_ShowErrorWindow || s_ScriptErrors.empty())
			return;

		ImGui::SetNextWindowSize(ImVec2(560.0f, 280.0f), ImGuiCond_Appearing);
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		if (!ImGui::Begin("Lua Script Error", &s_ShowErrorWindow,
			ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse))
		{
			ImGui::End();
			return;
		}

		ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
			"%d Lua error(s) — also printed in the console", (int)s_ScriptErrors.size());
		ImGui::Separator();

		ImGui::BeginChild("##LuaErrorList", ImVec2(0, -36.0f), true);
		for (size_t i = 0; i < s_ScriptErrors.size(); ++i)
		{
			const ScriptError& e = s_ScriptErrors[i];
			ImGui::PushID((int)i);
			ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "[%s]", e.phase.c_str());
			ImGui::Text("Object: %s", e.objectName.c_str());
			ImGui::Text("Where:  %s", e.where.c_str());
			ImGui::TextWrapped("Error:  %s", e.message.c_str());
			ImGui::Separator();
			ImGui::PopID();
		}
		ImGui::EndChild();

		if (ImGui::Button("Clear", ImVec2(120.0f, 0.0f)))
			ClearScriptErrors();
		ImGui::SameLine();
		ImGui::TextDisabled("Check the terminal / console for the full log");

		ImGui::End();
	}
}
