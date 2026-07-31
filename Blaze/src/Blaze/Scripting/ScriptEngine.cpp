#include "ScriptEngine.h"
#include "OnScreenDebug.h"
#include "ScriptTimers.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Renderer/Renderer.h"
#include "Blaze/Renderer/Camera.h"
#include "Blaze/Audio/AudioEngine.h"
#include "Blaze/Core/Input.h"
#include "Blaze/Core/Application.h"
#include "Blaze/Core/Project.h"
#include "Blaze/Event/KeyCodes.h"
#include "Blaze/Event/MouseButtonCodes.h"
#include "Blaze/Logging/Log.h"
#include "Blaze/UI/UISystem.h"
#include "Blaze/ImGui/PropertiesWindow.h"
#include "Blaze/Physics/PhysicsWorld2D.h"
#include "Blaze/Renderer/DebugDraw2D.h"
#include "Blaze/Renderer/ParticleSystem2D.h"
#include "Blaze/Scripting/SaveObject.h"
#include "Blaze/Assets/TiledImporter.h"
#include <filesystem>

// Soft safeties: full SOL_ALL_SAFETIES_ON can abort() the process on script errors.
#define SOL_SAFE_FUNCTIONS 1
#define SOL_SAFE_USERTYPE 1
#define SOL_EXCEPTIONS_SAFE_PROPAGATION 1
#include "sol.hpp"

#include "../vendor/imgui/src/imgui.h"

#include <algorithm>
#include <cctype>
#include <cmath>
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
	static bool s_MouseEnabled = false;      // false = auto-hide over Game view; true = show cursor
	static bool s_CursorVisible = true;
	static GameObject* s_CameraFollowTarget = nullptr;
	static float s_CameraFollowSmooth = 0.0f; // 0 = snap, >0 = seconds-ish lag
	static float s_CameraFollowOffsetX = 0.0f;
	static float s_CameraFollowOffsetY = 0.0f;

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

		std::tuple<float, float> GetRightVector() const
		{
			const float rad = glm::radians(GetRotation());
			return std::make_tuple(glm::cos(rad), glm::sin(rad));
		}

		std::tuple<float, float> GetUpVector() const
		{
			const float rad = glm::radians(GetRotation());
			return std::make_tuple(-glm::sin(rad), glm::cos(rad));
		}

		std::tuple<float, float> GetLeftVector() const
		{
			const float rad = glm::radians(GetRotation());
			return std::make_tuple(-glm::cos(rad), -glm::sin(rad));
		}

		std::tuple<float, float> GetDownVector() const
		{
			const float rad = glm::radians(GetRotation());
			return std::make_tuple(glm::sin(rad), -glm::cos(rad));
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

		void SetTexture(const std::string& path)
		{
			if (IsValid())
				raw->SetTexture(path);
		}

		std::string GetTexture() const
		{
			return IsValid() ? raw->GetTexturePath() : std::string{};
		}

		void SetZoom(float zoom)
		{
			if (IsValid())
				raw->SetZoom(zoom);
		}

		float GetZoom() const
		{
			return IsValid() ? raw->GetZoom() : 1.0f;
		}

		void SetDrivesView(bool drives)
		{
			if (IsValid())
				raw->SetDrivesView(drives);
		}

		bool DrivesView() const
		{
			return IsValid() && raw->DrivesView();
		}

		bool IsCamera() const
		{
			return IsValid() && raw->IsCamera();
		}

		int GetChildCount() const
		{
			return IsValid() ? (int)raw->GetChildren().size() : 0;
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

	static SaveObjectData TableToSaveData(sol::table table)
	{
		SaveObjectData data;
		if (!table.valid())
			return data;
		for (const auto& pair : table)
		{
			if (!pair.first.is<std::string>())
				continue;
			const std::string key = pair.first.as<std::string>();
			if (key.empty() || key == "__index")
				continue;
			sol::object value = pair.second;
			if (!value.valid() || value.get_type() == sol::type::lua_nil ||
				value.is<sol::function>() || value.is<sol::table>())
				continue;
			if (value.is<bool>())
				data.SetBool(key, value.as<bool>());
			else if (value.is<double>() || value.is<int>() || value.is<float>() || value.is<std::int64_t>())
				data.SetNumber(key, value.as<double>());
			else if (value.is<std::string>())
				data.SetString(key, value.as<std::string>());
		}
		return data;
	}

	static sol::table SaveDataToTable(sol::state& lua, const SaveObjectData& data)
	{
		sol::table table = lua.create_table();
		for (const std::string& key : data.GetKeys())
		{
			if (data.IsBool(key))
				table[key] = data.GetBool(key);
			else if (data.IsString(key))
				table[key] = data.GetString(key);
			else
				table[key] = data.GetNumber(key);
		}
		return table;
	}

	static void CallSaveMethod(sol::table instance, sol::table proto, const char* methodName)
	{
		if (!proto.valid())
			return;
		sol::object fnObj = proto[methodName];
		if (!fnObj.is<sol::function>())
			return;
		sol::protected_function fn = fnObj.as<sol::protected_function>();
		auto result = fn(instance);
		if (!result.valid())
		{
			sol::error err = result;
			BZ_CORE_ERROR("SaveObject.{0} error: {1}", methodName, err.what());
		}
	}

	static sol::object InstantiateSaveClass(sol::state& lua, const std::string& classNameOrPath, bool callOnCreate)
	{
		if (classNameOrPath.empty())
		{
			sol::table data = lua.create_table();
			data["version"] = 1;
			return data;
		}

		const std::string scriptPath = SaveGameSlots::FindSaveScriptPath(classNameOrPath);
		if (scriptPath.empty())
		{
			BZ_CORE_WARN("Save Script class not found: {0}", classNameOrPath);
			return sol::lua_nil;
		}

		sol::environment env(lua, sol::create, lua.globals());
		sol::protected_function_result loadResult = lua.safe_script_file(scriptPath, env);
		if (!loadResult.valid())
		{
			sol::error err = loadResult;
			BZ_CORE_ERROR("Failed to load Save Script '{0}': {1}", scriptPath, err.what());
			return sol::lua_nil;
		}

		sol::object protoObj = env["SaveObject"];
		if (!protoObj.is<sol::table>())
		{
			BZ_CORE_ERROR("Save Script '{0}' has no SaveObject table", scriptPath);
			return sol::lua_nil;
		}

		sol::table proto = protoObj.as<sol::table>();
		sol::table instance = lua.create_table();
		for (const auto& pair : proto)
		{
			if (pair.second.is<sol::function>())
				continue;
			instance[pair.first] = pair.second;
		}
		instance["_class"] = SaveGameSlots::ClassStem(classNameOrPath);

		sol::table mt = lua.create_table();
		mt["__index"] = proto;
		instance[sol::metatable_key] = mt;

		if (callOnCreate)
			CallSaveMethod(instance, proto, "OnCreate");
		return instance;
	}

	static sol::object CastSaveObjectImpl(sol::state& lua, sol::table data, const std::string& className)
	{
		if (!data.valid() || className.empty())
			return sol::lua_nil;

		sol::object storedClass = data["_class"];
		if (storedClass.is<std::string>())
		{
			if (!SaveGameSlots::ClassNamesMatch(storedClass.as<std::string>(), className))
				return sol::lua_nil;
		}

		sol::object instObj = InstantiateSaveClass(lua, className, false);
		if (!instObj.is<sol::table>())
			return sol::lua_nil;

		sol::table instance = instObj.as<sol::table>();
		for (const auto& pair : data)
		{
			if (pair.second.is<sol::function>())
				continue;
			instance[pair.first] = pair.second;
		}
		instance["_class"] = SaveGameSlots::ClassStem(className);

		sol::object mtObj = instance[sol::metatable_key];
		sol::table proto = lua.create_table();
		if (mtObj.is<sol::table>())
		{
			sol::object index = mtObj.as<sol::table>()["__index"];
			if (index.is<sol::table>())
				proto = index.as<sol::table>();
		}
		CallSaveMethod(instance, proto, "OnLoad");
		return instance;
	}

	static void BindApi(sol::state& lua)
	{
		lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

		lua.new_usertype<ScriptObject>("GameObject",
			sol::no_constructor,
			"GetName", &ScriptObject::GetName,
			"GetLocation", &ScriptObject::GetLocation,
			"GetRotation", &ScriptObject::GetRotation,
			"GetRightVector", &ScriptObject::GetRightVector,
			"GetUpVector", &ScriptObject::GetUpVector,
			"GetLeftVector", &ScriptObject::GetLeftVector,
			"GetDownVector", &ScriptObject::GetDownVector,
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
			"SetTexture", &ScriptObject::SetTexture,
			"GetTexture", &ScriptObject::GetTexture,
			"SetZoom", &ScriptObject::SetZoom,
			"GetZoom", &ScriptObject::GetZoom,
			"SetDrivesView", &ScriptObject::SetDrivesView,
			"DrivesView", &ScriptObject::DrivesView,
			"IsCamera", &ScriptObject::IsCamera,
			"GetChildCount", &ScriptObject::GetChildCount,
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

		lua.set_function("FindByName", [](const std::string& name) -> sol::object
		{
			GameObject* found = ScriptEngine::FindByName(name);
			if (!found || !s_Lua)
				return sol::make_object(s_Lua->lua_state(), sol::lua_nil);
			return sol::make_object(*s_Lua, ScriptObject(found));
		});

		lua.set_function("FindAllByName", [](const std::string& name)
		{
			sol::table result = s_Lua->create_table();
			int index = 1;
			for (GameObject* object : ScriptEngine::FindAllByName(name))
				result[index++] = ScriptObject(object);
			return result;
		});

		lua.set_function("EnableMouse", [](bool enabled)
		{
			ScriptEngine::EnableMouse(enabled);
		});
		lua.set_function("IsMouseEnabled", []()
		{
			return ScriptEngine::IsMouseEnabled();
		});
		lua.set_function("SetCursorVisible", [](bool visible)
		{
			ScriptEngine::SetCursorVisible(visible);
		});
		lua.set_function("ShowCursor", [](bool visible)
		{
			ScriptEngine::SetCursorVisible(visible);
		});
		lua.set_function("IsCursorVisible", []()
		{
			return ScriptEngine::IsCursorVisible();
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

		auto resolveMouseWorld = []() -> glm::vec2
		{
			Application& app = Application::Get();
			const float winW = (float)glm::max(app.GetWindow().GetWidth(), 1u);
			const float winH = (float)glm::max(app.GetWindow().GetHeight(), 1u);
			const float mx = (float)Input::GetMouseX();
			const float my = (float)Input::GetMouseY();

			// In the editor play Game view, map through the game framebuffer first.
			if (app.HasEditor() && StartWindow::IsPlaying())
			{
				float gx = 0.0f, gy = 0.0f;
				if (GameWindow::TryGetMouseGamePixel(gx, gy))
				{
					return Camera::ScreenToWorld(
						gx, gy,
						(float)glm::max(GameWindow::GetFrameWidth(), 1),
						(float)glm::max(GameWindow::GetFrameHeight(), 1));
				}
			}

			return Camera::ScreenToWorld(mx, my, winW, winH);
		};

		input["GetMouseWorld"] = [resolveMouseWorld]()
		{
			const glm::vec2 w = resolveMouseWorld();
			return std::make_tuple(w.x, w.y);
		};
		input["GetMouseWorldX"] = [resolveMouseWorld]()
		{
			return resolveMouseWorld().x;
		};
		input["GetMouseWorldY"] = [resolveMouseWorld]()
		{
			return resolveMouseWorld().y;
		};
		input["IsMouseOverGame"] = []()
		{
			if (!Application::Get().HasEditor())
				return true;
			if (!StartWindow::IsPlaying())
				return false;
			return GameWindow::IsMouseOverGameView();
		};
		input["EnableMouse"] = [](bool enabled)
		{
			ScriptEngine::EnableMouse(enabled);
		};
		input["IsMouseEnabled"] = []()
		{
			return ScriptEngine::IsMouseEnabled();
		};
		input["SetCursorVisible"] = [](bool visible)
		{
			ScriptEngine::SetCursorVisible(visible);
		};
		input["ShowCursor"] = [](bool visible)
		{
			ScriptEngine::SetCursorVisible(visible);
		};
		input["IsCursorVisible"] = []()
		{
			return ScriptEngine::IsCursorVisible();
		};
		input["ScreenToWorld"] = [](float screenX, float screenY)
		{
			Application& app = Application::Get();
			float w = (float)glm::max(app.GetWindow().GetWidth(), 1u);
			float h = (float)glm::max(app.GetWindow().GetHeight(), 1u);
			if (app.HasEditor() && StartWindow::IsPlaying())
			{
				w = (float)glm::max(GameWindow::GetFrameWidth(), 1);
				h = (float)glm::max(GameWindow::GetFrameHeight(), 1);
			}
			const glm::vec2 world = Camera::ScreenToWorld(screenX, screenY, w, h);
			return std::make_tuple(world.x, world.y);
		};

		lua.set_function("GetMouseWorld", [resolveMouseWorld]()
		{
			const glm::vec2 w = resolveMouseWorld();
			return std::make_tuple(w.x, w.y);
		});

		lua["MOUSE_LEFT"] = BZ_MOUSE_BUTTON_LEFT;
		lua["MOUSE_RIGHT"] = BZ_MOUSE_BUTTON_RIGHT;
		lua["MOUSE_MIDDLE"] = BZ_MOUSE_BUTTON_MIDDLE;

		sol::table particles = lua.create_named_table("Particles");
		particles.set_function("Emit", sol::overload(
			[](float x, float y)
			{
				ParticleSystem2D::Emit(x, y, 0.0f, 80.0f, 0.5f, 10.0f);
			},
			[](float x, float y, float vx, float vy)
			{
				ParticleSystem2D::Emit(x, y, vx, vy, 0.6f, 12.0f);
			},
			[](float x, float y, float vx, float vy, float life, float size)
			{
				ParticleSystem2D::Emit(x, y, vx, vy, life, size);
			},
			[](float x, float y, float vx, float vy, float life, float size,
				float r, float g, float b, float a)
			{
				ParticleSystem2D::Emit(x, y, vx, vy, life, size, r, g, b, a);
			}));
		particles.set_function("Burst", sol::overload(
			[](float x, float y)
			{
				ParticleSystem2D::Burst(x, y, 18, 80.0f, 220.0f, 0.55f, 10.0f);
			},
			[](float x, float y, int count)
			{
				ParticleSystem2D::Burst(x, y, count, 80.0f, 220.0f, 0.55f, 10.0f);
			},
			[](float x, float y, int count, float speed, float life, float size)
			{
				ParticleSystem2D::Burst(x, y, count, speed * 0.5f, speed, life, size);
			},
			[](float x, float y, int count, float speedMin, float speedMax,
				float life, float size, float r, float g, float b, float a)
			{
				ParticleSystem2D::Burst(x, y, count, speedMin, speedMax, life, size, r, g, b, a);
			},
			[](const sol::table& opts)
			{
				float x = 0.0f, y = 0.0f;
				int count = 16;
				float speedMin = 60.0f, speedMax = 200.0f;
				float life = 0.55f, size = 10.0f;
				float r = 1.0f, g = 0.8f, b = 0.2f, a = 1.0f;
				float gravityY = -400.0f;
				if (sol::optional<float> v = opts["x"]) x = *v;
				if (sol::optional<float> v = opts["y"]) y = *v;
				if (sol::optional<int> v = opts["count"]) count = *v;
				if (sol::optional<float> v = opts["speed"]) { speedMin = *v * 0.5f; speedMax = *v; }
				if (sol::optional<float> v = opts["speedMin"]) speedMin = *v;
				if (sol::optional<float> v = opts["speedMax"]) speedMax = *v;
				if (sol::optional<float> v = opts["life"]) life = *v;
				if (sol::optional<float> v = opts["size"]) size = *v;
				if (sol::optional<float> v = opts["gravity"]) gravityY = *v;
				if (sol::optional<float> v = opts["r"]) r = *v;
				if (sol::optional<float> v = opts["g"]) g = *v;
				if (sol::optional<float> v = opts["b"]) b = *v;
				if (sol::optional<float> v = opts["a"]) a = *v;
				if (sol::optional<sol::table> color = opts["color"])
				{
					if (sol::optional<float> v = (*color)["r"]) r = *v;
					if (sol::optional<float> v = (*color)["g"]) g = *v;
					if (sol::optional<float> v = (*color)["b"]) b = *v;
					if (sol::optional<float> v = (*color)["a"]) a = *v;
				}
				ParticleSystem2D::Burst(x, y, count, speedMin, speedMax, life, size, r, g, b, a, gravityY);
			}));
		particles.set_function("Clear", []() { ParticleSystem2D::Clear(); });
		particles.set_function("GetCount", []() { return ParticleSystem2D::GetAliveCount(); });

		lua.set_function("EmitParticles", sol::overload(
			[](float x, float y, int count)
			{
				ParticleSystem2D::Burst(x, y, count, 80.0f, 220.0f, 0.55f, 10.0f);
			},
			[](float x, float y, int count, float speed, float life, float size)
			{
				ParticleSystem2D::Burst(x, y, count, speed * 0.5f, speed, life, size);
			}));

		// UE-style latent helpers: Delay / SetTimer / Tween
		lua.set_function("Delay", [](float seconds, sol::protected_function callback)
		{
			return ScriptTimers::Delay(seconds, callback, s_CurrentObject);
		});

		lua.set_function("SetTimer", sol::overload(
			[](sol::protected_function callback, float rate, sol::optional<bool> loopingOpt)
			{
				return ScriptTimers::SetTimer(rate, callback, loopingOpt.value_or(false), s_CurrentObject);
			},
			[](float rate, sol::protected_function callback, sol::optional<bool> loopingOpt)
			{
				return ScriptTimers::SetTimer(rate, callback, loopingOpt.value_or(false), s_CurrentObject);
			}));

		lua.set_function("ClearTimer", [](int handle)
		{
			return ScriptTimers::ClearTimer(handle);
		});
		lua.set_function("IsTimerActive", [](int handle)
		{
			return ScriptTimers::IsTimerActive(handle);
		});
		lua.set_function("ClearAllTimers", []()
		{
			ScriptTimers::ClearAll();
		});

		lua.set_function("Tween", sol::overload(
			[](float from, float to, float duration, sol::protected_function onUpdate)
			{
				return ScriptTimers::TweenFloat(from, to, duration, onUpdate, sol::protected_function{}, "Linear", s_CurrentObject);
			},
			[](float from, float to, float duration, sol::protected_function onUpdate, sol::protected_function onComplete)
			{
				return ScriptTimers::TweenFloat(from, to, duration, onUpdate, onComplete, "Linear", s_CurrentObject);
			},
			[](float from, float to, float duration, sol::protected_function onUpdate,
				sol::protected_function onComplete, const std::string& ease)
			{
				return ScriptTimers::TweenFloat(from, to, duration, onUpdate, onComplete, ease, s_CurrentObject);
			},
			[](float from, float to, float duration, sol::protected_function onUpdate, const std::string& ease)
			{
				return ScriptTimers::TweenFloat(from, to, duration, onUpdate, sol::protected_function{}, ease, s_CurrentObject);
			}));

		lua.set_function("TweenVec2", sol::overload(
			[](float x0, float y0, float x1, float y1, float duration, sol::protected_function onUpdate)
			{
				return ScriptTimers::TweenVec2(x0, y0, x1, y1, duration, onUpdate, sol::protected_function{}, "Linear", s_CurrentObject);
			},
			[](float x0, float y0, float x1, float y1, float duration, sol::protected_function onUpdate,
				sol::protected_function onComplete)
			{
				return ScriptTimers::TweenVec2(x0, y0, x1, y1, duration, onUpdate, onComplete, "Linear", s_CurrentObject);
			},
			[](float x0, float y0, float x1, float y1, float duration, sol::protected_function onUpdate,
				sol::protected_function onComplete, const std::string& ease)
			{
				return ScriptTimers::TweenVec2(x0, y0, x1, y1, duration, onUpdate, onComplete, ease, s_CurrentObject);
			}));

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

		lua.set_function("SetTexture", sol::overload(
			[](const std::string& path)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetTexture(path);
			},
			[](const ScriptObject& object, const std::string& path)
			{
				if (object.IsValid())
					object.raw->SetTexture(path);
			}));

		lua.set_function("GetTexture", sol::overload(
			[]()
			{
				return s_CurrentObject ? s_CurrentObject->GetTexturePath() : std::string{};
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() ? object.raw->GetTexturePath() : std::string{};
			}));

		lua.set_function("SetZoom", sol::overload(
			[](float zoom)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetZoom(zoom);
			},
			[](const ScriptObject& object, float zoom)
			{
				if (object.IsValid())
					object.raw->SetZoom(zoom);
			}));

		lua.set_function("GetZoom", sol::overload(
			[]()
			{
				return s_CurrentObject ? s_CurrentObject->GetZoom() : 1.0f;
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() ? object.raw->GetZoom() : 1.0f;
			}));

		lua.set_function("SetDrivesView", sol::overload(
			[](bool drives)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetDrivesView(drives);
			},
			[](const ScriptObject& object, bool drives)
			{
				if (object.IsValid())
					object.raw->SetDrivesView(drives);
			}));

		lua.set_function("DrivesView", sol::overload(
			[]()
			{
				return s_CurrentObject && s_CurrentObject->DrivesView();
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() && object.raw->DrivesView();
			}));

		lua.set_function("IsCamera", sol::overload(
			[]()
			{
				return s_CurrentObject && s_CurrentObject->IsCamera();
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() && object.raw->IsCamera();
			}));

		lua.set_function("GetChildren", sol::overload(
			[]()
			{
				sol::table result = s_Lua->create_table();
				if (!s_CurrentObject)
					return result;
				int i = 1;
				for (GameObject* child : s_CurrentObject->GetChildren())
				{
					if (child && s_LiveObjects.find(child) != s_LiveObjects.end())
						result[i++] = ScriptObject(child);
				}
				return result;
			},
			[](const ScriptObject& object)
			{
				sol::table result = s_Lua->create_table();
				if (!object.IsValid())
					return result;
				int i = 1;
				for (GameObject* child : object.raw->GetChildren())
				{
					if (child && s_LiveObjects.find(child) != s_LiveObjects.end())
						result[i++] = ScriptObject(child);
				}
				return result;
			}));

		lua.set_function("GetChildCount", sol::overload(
			[]()
			{
				return s_CurrentObject ? (int)s_CurrentObject->GetChildren().size() : 0;
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() ? (int)object.raw->GetChildren().size() : 0;
			}));

		lua.set_function("GetChild", sol::overload(
			[](int index) -> sol::object
			{
				if (!s_CurrentObject || !s_Lua || index < 1)
					return sol::make_object(s_Lua->lua_state(), sol::lua_nil);
				const auto& kids = s_CurrentObject->GetChildren();
				if (index > (int)kids.size())
					return sol::make_object(s_Lua->lua_state(), sol::lua_nil);
				GameObject* child = kids[(size_t)index - 1];
				if (!child || s_LiveObjects.find(child) == s_LiveObjects.end())
					return sol::make_object(s_Lua->lua_state(), sol::lua_nil);
				return sol::make_object(*s_Lua, ScriptObject(child));
			},
			[](const ScriptObject& object, int index) -> sol::object
			{
				if (!object.IsValid() || !s_Lua || index < 1)
					return sol::make_object(s_Lua->lua_state(), sol::lua_nil);
				const auto& kids = object.raw->GetChildren();
				if (index > (int)kids.size())
					return sol::make_object(s_Lua->lua_state(), sol::lua_nil);
				GameObject* child = kids[(size_t)index - 1];
				if (!child || s_LiveObjects.find(child) == s_LiveObjects.end())
					return sol::make_object(s_Lua->lua_state(), sol::lua_nil);
				return sol::make_object(*s_Lua, ScriptObject(child));
			}));

		lua.set_function("SetCameraFollow", sol::overload(
			[]()
			{
				// Follow the script's own object (call from player OnStart/OnUpdate).
				s_CameraFollowTarget = s_CurrentObject;
				s_CameraFollowSmooth = 0.0f;
				s_CameraFollowOffsetX = 0.0f;
				s_CameraFollowOffsetY = 0.0f;
			},
			[](float smooth)
			{
				s_CameraFollowTarget = s_CurrentObject;
				s_CameraFollowSmooth = glm::max(smooth, 0.0f);
				s_CameraFollowOffsetX = 0.0f;
				s_CameraFollowOffsetY = 0.0f;
			},
			[](float smooth, float ox, float oy)
			{
				s_CameraFollowTarget = s_CurrentObject;
				s_CameraFollowSmooth = glm::max(smooth, 0.0f);
				s_CameraFollowOffsetX = ox;
				s_CameraFollowOffsetY = oy;
			},
			[](const ScriptObject& target)
			{
				s_CameraFollowTarget = target.IsValid() ? target.raw : nullptr;
				s_CameraFollowSmooth = 0.0f;
				s_CameraFollowOffsetX = 0.0f;
				s_CameraFollowOffsetY = 0.0f;
			},
			[](const ScriptObject& target, float smooth)
			{
				s_CameraFollowTarget = target.IsValid() ? target.raw : nullptr;
				s_CameraFollowSmooth = glm::max(smooth, 0.0f);
				s_CameraFollowOffsetX = 0.0f;
				s_CameraFollowOffsetY = 0.0f;
			},
			[](const ScriptObject& target, float smooth, float ox, float oy)
			{
				s_CameraFollowTarget = target.IsValid() ? target.raw : nullptr;
				s_CameraFollowSmooth = glm::max(smooth, 0.0f);
				s_CameraFollowOffsetX = ox;
				s_CameraFollowOffsetY = oy;
			},
			[](const std::string& name)
			{
				s_CameraFollowTarget = ScriptEngine::FindByName(name);
				s_CameraFollowSmooth = 0.0f;
				s_CameraFollowOffsetX = 0.0f;
				s_CameraFollowOffsetY = 0.0f;
			},
			[](const std::string& name, float smooth)
			{
				s_CameraFollowTarget = ScriptEngine::FindByName(name);
				s_CameraFollowSmooth = glm::max(smooth, 0.0f);
				s_CameraFollowOffsetX = 0.0f;
				s_CameraFollowOffsetY = 0.0f;
			},
			[](const std::string& name, float smooth, float ox, float oy)
			{
				s_CameraFollowTarget = ScriptEngine::FindByName(name);
				s_CameraFollowSmooth = glm::max(smooth, 0.0f);
				s_CameraFollowOffsetX = ox;
				s_CameraFollowOffsetY = oy;
			}));

		lua.set_function("ClearCameraFollow", []()
		{
			s_CameraFollowTarget = nullptr;
		});

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

		lua.set_function("GetRightVector", sol::overload(
			[]()
			{
				ScriptObject self(s_CurrentObject);
				return self.GetRightVector();
			},
			[](const ScriptObject& object)
			{
				return object.GetRightVector();
			}));

		lua.set_function("GetUpVector", sol::overload(
			[]()
			{
				ScriptObject self(s_CurrentObject);
				return self.GetUpVector();
			},
			[](const ScriptObject& object)
			{
				return object.GetUpVector();
			}));

		lua.set_function("GetLeftVector", sol::overload(
			[]()
			{
				ScriptObject self(s_CurrentObject);
				return self.GetLeftVector();
			},
			[](const ScriptObject& object)
			{
				return object.GetLeftVector();
			}));

		lua.set_function("GetDownVector", sol::overload(
			[]()
			{
				ScriptObject self(s_CurrentObject);
				return self.GetDownVector();
			},
			[](const ScriptObject& object)
			{
				return object.GetDownVector();
			}));

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

		lua["MASK_ALL"] = 0xFFFFFFFFu;

		auto makeRaycastResult = [&lua](bool ok, const RaycastHit& hit, float missX, float missY) -> sol::table
		{
			sol::table result = lua.create_table();
			result["Hit"] = ok;
			result["hit"] = ok;

			sol::table location = lua.create_table();
			sol::table normal = lua.create_table();

			if (ok && hit.object)
			{
				ScriptObject object(hit.object);
				result["Object"] = object;
				result["object"] = object;
				result["Name"] = hit.object->GetName();
				result["name"] = hit.object->GetName();
				result["Tag"] = hit.object->GetTag();
				result["tag"] = hit.object->GetTag();

				location["x"] = hit.point.x;
				location["y"] = hit.point.y;
				result["Location"] = location;
				result["location"] = location;
				result["x"] = hit.point.x;
				result["y"] = hit.point.y;

				normal["x"] = hit.normal.x;
				normal["y"] = hit.normal.y;
				result["Normal"] = normal;
				result["normal"] = normal;
				result["nx"] = hit.normal.x;
				result["ny"] = hit.normal.y;

				result["Fraction"] = hit.fraction;
				result["fraction"] = hit.fraction;
				result["Distance"] = hit.distance;
				result["distance"] = hit.distance;
			}
			else
			{
				result["Object"] = sol::lua_nil;
				result["object"] = sol::lua_nil;
				result["Name"] = "";
				result["name"] = "";
				result["Tag"] = "";
				result["tag"] = "";

				location["x"] = missX;
				location["y"] = missY;
				result["Location"] = location;
				result["location"] = location;
				result["x"] = missX;
				result["y"] = missY;

				normal["x"] = 0.0f;
				normal["y"] = 0.0f;
				result["Normal"] = normal;
				result["normal"] = normal;
				result["nx"] = 0.0f;
				result["ny"] = 0.0f;

				result["Fraction"] = 1.0f;
				result["fraction"] = 1.0f;
				result["Distance"] = 0.0f;
				result["distance"] = 0.0f;
			}

			return result;
		};

		lua.set_function("Raycast",
			[makeRaycastResult](float x1, float y1, float x2, float y2,
				sol::optional<bool> drawDebugOpt,
				sol::optional<float> durationOpt,
				sol::optional<uint32_t> layerMaskOpt) -> sol::table
			{
				if (!s_Physics || !s_Renderer)
					return makeRaycastResult(false, RaycastHit{}, x2, y2);

				const bool drawDebug = drawDebugOpt.value_or(false);
				const float duration = durationOpt.value_or(0.0f);
				const uint32_t layerMask = layerMaskOpt.value_or(0xFFFFFFFFu);

				RaycastHit hit;
				const bool ok = s_Physics->Raycast(
					glm::vec2(x1, y1), glm::vec2(x2, y2),
					s_Renderer->GetObjects(), hit,
					layerMask, s_CurrentObject, false);

				if (drawDebug)
					DebugDraw2D::DrawRaycast(glm::vec2(x1, y1), glm::vec2(x2, y2), ok, hit.point, duration);

				return makeRaycastResult(ok, hit, x2, y2);
			});

		lua.set_function("RaycastAll",
			[&lua, makeRaycastResult](float x1, float y1, float x2, float y2,
				sol::optional<bool> drawDebugOpt,
				sol::optional<float> durationOpt,
				sol::optional<uint32_t> layerMaskOpt) -> sol::table
			{
				sol::table result = lua.create_table();
				if (!s_Physics || !s_Renderer)
					return result;

				const bool drawDebug = drawDebugOpt.value_or(false);
				const float duration = durationOpt.value_or(0.0f);
				const uint32_t layerMask = layerMaskOpt.value_or(0xFFFFFFFFu);

				std::vector<RaycastHit> hits = s_Physics->RaycastAll(
					glm::vec2(x1, y1), glm::vec2(x2, y2),
					s_Renderer->GetObjects(),
					layerMask, s_CurrentObject, false);

				if (drawDebug)
				{
					if (!hits.empty())
						DebugDraw2D::DrawRaycast(glm::vec2(x1, y1), glm::vec2(x2, y2), true, hits.front().point, duration);
					else
						DebugDraw2D::DrawRaycast(glm::vec2(x1, y1), glm::vec2(x2, y2), false, glm::vec2(x2, y2), duration);
				}

				int i = 1;
				for (const RaycastHit& hit : hits)
					result[i++] = makeRaycastResult(true, hit, hit.point.x, hit.point.y);
				return result;
			});

		lua.set_function("OverlapPoint",
			[&lua](float x, float y, sol::optional<uint32_t> layerMaskOpt) -> sol::table
			{
				sol::table result = lua.create_table();
				if (!s_Physics || !s_Renderer)
					return result;
				std::vector<GameObject*> found;
				s_Physics->OverlapPoint(glm::vec2(x, y), s_Renderer->GetObjects(), found,
					layerMaskOpt.value_or(0xFFFFFFFFu), s_CurrentObject, true);
				int i = 1;
				for (GameObject* obj : found)
					result[i++] = ScriptObject(obj);
				return result;
			});

		lua.set_function("OverlapCircle",
			[&lua](float x, float y, float radius, sol::optional<uint32_t> layerMaskOpt) -> sol::table
			{
				sol::table result = lua.create_table();
				if (!s_Physics || !s_Renderer)
					return result;
				std::vector<GameObject*> found;
				s_Physics->OverlapCircle(glm::vec2(x, y), radius, s_Renderer->GetObjects(), found,
					layerMaskOpt.value_or(0xFFFFFFFFu), s_CurrentObject, true);
				int i = 1;
				for (GameObject* obj : found)
					result[i++] = ScriptObject(obj);
				return result;
			});

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

		// Character helpers — use last physics Step contacts (+ short feet probe for IsGrounded).
		lua.set_function("IsGrounded", sol::overload(
			[](sol::optional<float> maxSlopeOpt)
			{
				if (!s_Physics || !s_CurrentObject || !s_Renderer)
					return false;
				const float slope = maxSlopeOpt.value_or(50.0f);
				return s_Physics->CheckGround(s_CurrentObject, s_Renderer->GetObjects(), 6.0f, slope);
			},
			[](const ScriptObject& object, sol::optional<float> maxSlopeOpt)
			{
				if (!s_Physics || !object.IsValid() || !s_Renderer)
					return false;
				const float slope = maxSlopeOpt.value_or(50.0f);
				return s_Physics->CheckGround(object.raw, s_Renderer->GetObjects(), 6.0f, slope);
			}));

		lua.set_function("CheckGround", sol::overload(
			[](sol::optional<float> probeOpt, sol::optional<float> maxSlopeOpt)
			{
				if (!s_Physics || !s_CurrentObject || !s_Renderer)
					return false;
				return s_Physics->CheckGround(s_CurrentObject, s_Renderer->GetObjects(),
					probeOpt.value_or(6.0f), maxSlopeOpt.value_or(50.0f));
			},
			[](const ScriptObject& object, sol::optional<float> probeOpt, sol::optional<float> maxSlopeOpt)
			{
				if (!s_Physics || !object.IsValid() || !s_Renderer)
					return false;
				return s_Physics->CheckGround(object.raw, s_Renderer->GetObjects(),
					probeOpt.value_or(6.0f), maxSlopeOpt.value_or(50.0f));
			}));

		lua.set_function("IsTouchingCeiling", sol::overload(
			[](sol::optional<float> maxSlopeOpt)
			{
				return s_Physics && s_CurrentObject
					&& s_Physics->IsTouchingCeiling(s_CurrentObject, maxSlopeOpt.value_or(50.0f));
			},
			[](const ScriptObject& object, sol::optional<float> maxSlopeOpt)
			{
				return s_Physics && object.IsValid()
					&& s_Physics->IsTouchingCeiling(object.raw, maxSlopeOpt.value_or(50.0f));
			}));

		lua.set_function("IsTouchingWall", sol::overload(
			[]()
			{
				return s_Physics && s_CurrentObject && s_Physics->IsTouchingWall(s_CurrentObject);
			},
			[](const ScriptObject& object)
			{
				return s_Physics && object.IsValid() && s_Physics->IsTouchingWall(object.raw);
			}));

		lua.set_function("IsTouchingWallLeft", sol::overload(
			[]()
			{
				return s_Physics && s_CurrentObject && s_Physics->IsTouchingWallLeft(s_CurrentObject);
			},
			[](const ScriptObject& object)
			{
				return s_Physics && object.IsValid() && s_Physics->IsTouchingWallLeft(object.raw);
			}));

		lua.set_function("IsTouchingWallRight", sol::overload(
			[]()
			{
				return s_Physics && s_CurrentObject && s_Physics->IsTouchingWallRight(s_CurrentObject);
			},
			[](const ScriptObject& object)
			{
				return s_Physics && object.IsValid() && s_Physics->IsTouchingWallRight(object.raw);
			}));

		lua.set_function("GetGroundNormal", sol::overload(
			[](sol::optional<float> maxSlopeOpt)
			{
				if (!s_Physics || !s_CurrentObject)
					return std::make_tuple(0.0f, 1.0f);
				glm::vec2 n = s_Physics->GetGroundNormal(s_CurrentObject, maxSlopeOpt.value_or(50.0f));
				return std::make_tuple(n.x, n.y);
			},
			[](const ScriptObject& object, sol::optional<float> maxSlopeOpt)
			{
				if (!s_Physics || !object.IsValid())
					return std::make_tuple(0.0f, 1.0f);
				glm::vec2 n = s_Physics->GetGroundNormal(object.raw, maxSlopeOpt.value_or(50.0f));
				return std::make_tuple(n.x, n.y);
			}));

		lua.set_function("GetContactCount", sol::overload(
			[](sol::optional<bool> includeTriggersOpt)
			{
				if (!s_Physics || !s_CurrentObject)
					return 0;
				return s_Physics->GetContactCount(s_CurrentObject, includeTriggersOpt.value_or(false));
			},
			[](const ScriptObject& object, sol::optional<bool> includeTriggersOpt)
			{
				if (!s_Physics || !object.IsValid())
					return 0;
				return s_Physics->GetContactCount(object.raw, includeTriggersOpt.value_or(false));
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

		lua.set_function("PlayOneShot", sol::overload(
			[](const std::string& path)
			{
				return AudioEngine::PlayOneShot(path, 1.0f);
			},
			[](const std::string& path, float volume)
			{
				return AudioEngine::PlayOneShot(path, volume);
			}));
		lua.set_function("PlaySound", sol::overload(
			[](const std::string& path)
			{
				return AudioEngine::PlayOneShot(path, 1.0f);
			},
			[](const std::string& path, float volume)
			{
				return AudioEngine::PlayOneShot(path, volume);
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
			[](const std::string& clipName)
			{
				if (s_CurrentObject)
					return s_CurrentObject->PlayFlipbook(clipName);
				return false;
			},
			[](const ScriptObject& object)
			{
				if (object.IsValid())
					object.raw->PlayFlipbook();
			},
			[](const ScriptObject& object, const std::string& clipName)
			{
				if (object.IsValid())
					return object.raw->PlayFlipbook(clipName);
				return false;
			}));

		lua.set_function("AddFlipbookClip", sol::overload(
			[](const std::string& name, int startFrame, int endFrame)
			{
				if (!s_CurrentObject) return -1;
				return s_CurrentObject->AddFlipbookClip(name, startFrame, endFrame);
			},
			[](const std::string& name, int startFrame, int endFrame, float fps)
			{
				if (!s_CurrentObject) return -1;
				return s_CurrentObject->AddFlipbookClip(name, startFrame, endFrame, fps);
			},
			[](const std::string& name, int startFrame, int endFrame, float fps, bool loop)
			{
				if (!s_CurrentObject) return -1;
				return s_CurrentObject->AddFlipbookClip(name, startFrame, endFrame, fps, loop);
			},
			[](const ScriptObject& object, const std::string& name, int startFrame, int endFrame)
			{
				if (!object.IsValid()) return -1;
				return object.raw->AddFlipbookClip(name, startFrame, endFrame);
			},
			[](const ScriptObject& object, const std::string& name, int startFrame, int endFrame, float fps, bool loop)
			{
				if (!object.IsValid()) return -1;
				return object.raw->AddFlipbookClip(name, startFrame, endFrame, fps, loop);
			}));

		lua.set_function("SetFlipbookClip", sol::overload(
			[](const std::string& name, int startFrame, int endFrame)
			{
				if (!s_CurrentObject) return false;
				return s_CurrentObject->SetFlipbookClip(name, startFrame, endFrame);
			},
			[](const std::string& name, int startFrame, int endFrame, float fps, bool loop)
			{
				if (!s_CurrentObject) return false;
				return s_CurrentObject->SetFlipbookClip(name, startFrame, endFrame, fps, loop);
			},
			[](const ScriptObject& object, const std::string& name, int startFrame, int endFrame, float fps, bool loop)
			{
				if (!object.IsValid()) return false;
				return object.raw->SetFlipbookClip(name, startFrame, endFrame, fps, loop);
			}));

		lua.set_function("RemoveFlipbookClip", sol::overload(
			[](const std::string& name)
			{
				if (!s_CurrentObject) return false;
				return s_CurrentObject->RemoveFlipbookClip(name);
			},
			[](const ScriptObject& object, const std::string& name)
			{
				if (!object.IsValid()) return false;
				return object.raw->RemoveFlipbookClip(name);
			}));

		lua.set_function("ClearFlipbookClips", sol::overload(
			[]()
			{
				if (s_CurrentObject)
					s_CurrentObject->ClearFlipbookClips();
			},
			[](const ScriptObject& object)
			{
				if (object.IsValid())
					object.raw->ClearFlipbookClips();
			}));

		lua.set_function("SetActiveFlipbookClip", sol::overload(
			[](const std::string& name)
			{
				if (!s_CurrentObject) return false;
				return s_CurrentObject->SetActiveFlipbookClip(name);
			},
			[](const ScriptObject& object, const std::string& name)
			{
				if (!object.IsValid()) return false;
				return object.raw->SetActiveFlipbookClip(name);
			}));

		lua.set_function("GetActiveFlipbookClip", sol::overload(
			[]()
			{
				return s_CurrentObject ? s_CurrentObject->GetActiveFlipbookClipName() : std::string{};
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() ? object.raw->GetActiveFlipbookClipName() : std::string{};
			}));

		lua.set_function("HasFlipbookClip", sol::overload(
			[](const std::string& name)
			{
				return s_CurrentObject && s_CurrentObject->FindFlipbookClip(name) >= 0;
			},
			[](const ScriptObject& object, const std::string& name)
			{
				return object.IsValid() && object.raw->FindFlipbookClip(name) >= 0;
			}));

		lua.set_function("GetFlipbookClipCount", sol::overload(
			[]()
			{
				return s_CurrentObject ? s_CurrentObject->GetFlipbookClipCount() : 0;
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() ? object.raw->GetFlipbookClipCount() : 0;
			}));

		lua.set_function("GetFlipbookClipName", sol::overload(
			[](int index)
			{
				if (!s_CurrentObject) return std::string{};
				const FlipbookClip* clip = s_CurrentObject->GetFlipbookClip(index);
				return clip ? clip->name : std::string{};
			},
			[](const ScriptObject& object, int index)
			{
				if (!object.IsValid()) return std::string{};
				const FlipbookClip* clip = object.raw->GetFlipbookClip(index);
				return clip ? clip->name : std::string{};
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

		lua.set_function("SetFlipbookAsset", sol::overload(
			[](const std::string& path)
			{
				if (!s_CurrentObject) return false;
				s_CurrentObject->SetFlipbookAssetPath(path);
				return s_CurrentObject->HasFlipbookAsset();
			},
			[](const ScriptObject& object, const std::string& path)
			{
				if (!object.IsValid()) return false;
				object.raw->SetFlipbookAssetPath(path);
				return object.raw->HasFlipbookAsset();
			}));

		lua.set_function("GetFlipbookAsset", sol::overload(
			[]()
			{
				return s_CurrentObject ? s_CurrentObject->GetFlipbookAssetPath() : std::string{};
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() ? object.raw->GetFlipbookAssetPath() : std::string{};
			}));

		lua.set_function("SaveFlipbookAsset", sol::overload(
			[]()
			{
				return s_CurrentObject && s_CurrentObject->SaveFlipbookAsset();
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() && object.raw->SaveFlipbookAsset();
			}));

		lua.set_function("CreateSaveObject", sol::overload(
			[&lua]()
			{
				return InstantiateSaveClass(lua, "", true);
			},
			[&lua](const std::string& className)
			{
				return InstantiateSaveClass(lua, className, true);
			}));

		lua.set_function("CastSaveObject", [&lua](sol::table data, const std::string& className) -> sol::object
		{
			return CastSaveObjectImpl(lua, data, className);
		});

		lua.set_function("DoesSaveObjectExist", [](const std::string& slotName) -> bool
		{
			return SaveGameSlots::DoesSlotExist(slotName);
		});

		lua.set_function("GetSaveObjectPath", [](const std::string& slotName) -> std::string
		{
			return SaveGameSlots::ResolveSlotPath(slotName);
		});

		lua.set_function("SaveSaveObject", [&lua](const std::string& slotName, sol::table table) -> bool
		{
			(void)lua;
			if (slotName.empty() || !table.valid())
				return false;
			const std::string path = SaveGameSlots::ResolveSlotPath(slotName);
			SaveObjectData data = TableToSaveData(table);
			sol::object classObj = table["_class"];
			if (classObj.is<std::string>())
				data.SetString("_class", classObj.as<std::string>());
			std::error_code ec;
			std::filesystem::path outPath(path);
			if (outPath.has_parent_path())
				std::filesystem::create_directories(outPath.parent_path(), ec);
			return data.SaveToFile(path);
		});

		lua.set_function("LoadSaveObject", sol::overload(
			[&lua](const std::string& slotName) -> sol::object
			{
				const std::string path = SaveGameSlots::ResolveSlotPath(slotName);
				SaveObjectData data;
				if (!data.LoadFromFile(path))
					return sol::lua_nil;
				return SaveDataToTable(lua, data);
			},
			[&lua](const std::string& slotName, const std::string& className) -> sol::object
			{
				const std::string path = SaveGameSlots::ResolveSlotPath(slotName);
				SaveObjectData data;
				if (!data.LoadFromFile(path))
					return sol::lua_nil;
				sol::table raw = SaveDataToTable(lua, data);
				return CastSaveObjectImpl(lua, raw, className);
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

		lua.set_function("GetTileLayerCount", sol::overload(
			[]()
			{
				return s_CurrentObject ? s_CurrentObject->GetTileLayerCount() : 0;
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() ? object.raw->GetTileLayerCount() : 0;
			}));
		lua.set_function("GetActiveTileLayer", sol::overload(
			[]()
			{
				return s_CurrentObject ? s_CurrentObject->GetActiveTileLayer() : 0;
			},
			[](const ScriptObject& object)
			{
				return object.IsValid() ? object.raw->GetActiveTileLayer() : 0;
			}));
		lua.set_function("SetActiveTileLayer", sol::overload(
			[](int index)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetActiveTileLayer(index);
			},
			[](const ScriptObject& object, int index)
			{
				if (object.IsValid())
					object.raw->SetActiveTileLayer(index);
			}));
		lua.set_function("AddTileLayer", sol::overload(
			[](sol::optional<std::string> nameOpt)
			{
				if (!s_CurrentObject)
					return -1;
				return s_CurrentObject->AddTileLayer(nameOpt.value_or("Layer"));
			},
			[](const ScriptObject& object, sol::optional<std::string> nameOpt)
			{
				if (!object.IsValid())
					return -1;
				return object.raw->AddTileLayer(nameOpt.value_or("Layer"));
			}));
		lua.set_function("RemoveTileLayer", sol::overload(
			[](int index)
			{
				return s_CurrentObject && s_CurrentObject->RemoveTileLayer(index);
			},
			[](const ScriptObject& object, int index)
			{
				return object.IsValid() && object.raw->RemoveTileLayer(index);
			}));
		lua.set_function("SetTileLayerVisible", sol::overload(
			[](int index, bool visible)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetTileLayerVisible(index, visible);
			},
			[](const ScriptObject& object, int index, bool visible)
			{
				if (object.IsValid())
					object.raw->SetTileLayerVisible(index, visible);
			}));
		lua.set_function("SetTileLayerCollision", sol::overload(
			[](int index, bool collision)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetTileLayerCollision(index, collision);
			},
			[](const ScriptObject& object, int index, bool collision)
			{
				if (object.IsValid())
					object.raw->SetTileLayerCollision(index, collision);
			}));
		lua.set_function("SetTileLayerOpacity", sol::overload(
			[](int index, float opacity)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetTileLayerOpacity(index, opacity);
			},
			[](const ScriptObject& object, int index, float opacity)
			{
				if (object.IsValid())
					object.raw->SetTileLayerOpacity(index, opacity);
			}));
		lua.set_function("SetTileOnLayer", sol::overload(
			[](int layer, int x, int y, int tileId)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetTileOnLayer(layer, x, y, tileId);
			},
			[](const ScriptObject& object, int layer, int x, int y, int tileId)
			{
				if (object.IsValid())
					object.raw->SetTileOnLayer(layer, x, y, tileId);
			}));
		lua.set_function("GetTileOnLayer", sol::overload(
			[](int layer, int x, int y)
			{
				return s_CurrentObject ? s_CurrentObject->GetTileOnLayer(layer, x, y) : -1;
			},
			[](const ScriptObject& object, int layer, int x, int y)
			{
				return object.IsValid() ? object.raw->GetTileOnLayer(layer, x, y) : -1;
			}));
		lua.set_function("ImportTiledMap", [](const std::string& tiledPath, sol::optional<std::string> outMapOpt)
		{
			namespace fs = std::filesystem;
			fs::path source(tiledPath);
			if (!source.is_absolute() && Project::HasProject())
				source = fs::path(Project::GetDirectory()) / source;
			fs::path outMap;
			if (outMapOpt && !outMapOpt->empty())
			{
				outMap = *outMapOpt;
				if (!outMap.is_absolute() && Project::HasProject())
					outMap = fs::path(Project::GetDirectory()) / outMap;
			}
			else
			{
				fs::path dir = Project::HasProject() ? fs::path(Project::GetContentDirectory()) : source.parent_path();
				outMap = dir / (source.stem().string() + ".map");
			}
			fs::path texDir = Project::HasProject()
				? fs::path(Project::GetContentDirectory()) / "texture"
				: outMap.parent_path();
			std::string error;
			if (!TiledImporter::ImportToMapFile(source.generic_string(), outMap.generic_string(), texDir.generic_string(), &error))
			{
				BZ_INFO("ImportTiledMap failed: {0}", error);
				return std::string{};
			}
			return outMap.generic_string();
		});

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

		lua.set_function("WorldToTile", sol::overload(
			[](float worldX, float worldY)
			{
				int tx = 0, ty = 0;
				if (!s_CurrentObject || !s_CurrentObject->WorldToTile(Camera::m_prop, glm::vec2(worldX, worldY), tx, ty))
					return std::make_tuple(0, 0, false);
				return std::make_tuple(tx, ty, true);
			},
			[](const ScriptObject& object, float worldX, float worldY)
			{
				int tx = 0, ty = 0;
				if (!object.IsValid() || !object.raw->WorldToTile(Camera::m_prop, glm::vec2(worldX, worldY), tx, ty))
					return std::make_tuple(0, 0, false);
				return std::make_tuple(tx, ty, true);
			}));

		lua.set_function("TileCenter", sol::overload(
			[](int tileX, int tileY)
			{
				if (!s_CurrentObject)
					return std::make_tuple(0.0f, 0.0f);
				const glm::vec2 c = s_CurrentObject->TileCenterWorld(Camera::m_prop, tileX, tileY);
				return std::make_tuple(c.x, c.y);
			},
			[](const ScriptObject& object, int tileX, int tileY)
			{
				if (!object.IsValid())
					return std::make_tuple(0.0f, 0.0f);
				const glm::vec2 c = object.raw->TileCenterWorld(Camera::m_prop, tileX, tileY);
				return std::make_tuple(c.x, c.y);
			}));

		lua.set_function("SetTileCollision", sol::overload(
			[](bool enabled)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetTileCollisionEnabled(enabled);
			},
			[](const ScriptObject& object, bool enabled)
			{
				if (object.IsValid())
					object.raw->SetTileCollisionEnabled(enabled);
			}));

		lua.set_function("SetTileSolid", sol::overload(
			[](int tileId, bool solid)
			{
				if (s_CurrentObject)
					s_CurrentObject->SetTilesetSolid(tileId, solid);
			},
			[](const ScriptObject& object, int tileId, bool solid)
			{
				if (object.IsValid())
					object.raw->SetTilesetSolid(tileId, solid);
			}));

		lua.set_function("IsTileSolid", sol::overload(
			[](int tileId)
			{
				return s_CurrentObject ? s_CurrentObject->IsTileIdSolid(tileId) : false;
			},
			[](const ScriptObject& object, int tileId)
			{
				return object.IsValid() ? object.raw->IsTileIdSolid(tileId) : false;
			}));

		lua.set_function("IsSolidAt", sol::overload(
			[](int x, int y)
			{
				return s_CurrentObject ? s_CurrentObject->IsSolidAt(x, y) : false;
			},
			[](const ScriptObject& object, int x, int y)
			{
				return object.IsValid() ? object.raw->IsSolidAt(x, y) : false;
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

	GameObject* ScriptEngine::FindByName(const std::string& name)
	{
		if (!s_Renderer || name.empty())
			return nullptr;
		for (GameObject* object : s_Renderer->GetObjects())
		{
			if (object && s_LiveObjects.find(object) != s_LiveObjects.end() && object->GetName() == name)
				return object;
		}
		return nullptr;
	}

	std::vector<GameObject*> ScriptEngine::FindAllByName(const std::string& name)
	{
		std::vector<GameObject*> result;
		if (!s_Renderer || name.empty())
			return result;
		for (GameObject* object : s_Renderer->GetObjects())
		{
			if (object && s_LiveObjects.find(object) != s_LiveObjects.end() && object->GetName() == name)
				result.push_back(object);
		}
		return result;
	}

	void ScriptEngine::EnableMouse(bool enabled)
	{
		s_MouseEnabled = enabled;
		if (enabled)
			s_CursorVisible = true;
	}

	bool ScriptEngine::IsMouseEnabled()
	{
		return s_MouseEnabled;
	}

	void ScriptEngine::SetCursorVisible(bool visible)
	{
		s_CursorVisible = visible;
		if (visible)
			s_MouseEnabled = true;
	}

	bool ScriptEngine::IsCursorVisible()
	{
		return s_CursorVisible && s_MouseEnabled;
	}

	bool ScriptEngine::WantsCursorVisible()
	{
		return s_MouseEnabled && s_CursorVisible;
	}

	void ScriptEngine::UpdateCameraFollow(float dt)
	{
		if (!s_CameraFollowTarget || !s_Renderer)
			return;
		if (s_LiveObjects.find(s_CameraFollowTarget) == s_LiveObjects.end())
		{
			s_CameraFollowTarget = nullptr;
			return;
		}

		GameObject* cam = nullptr;
		GameObject* fallback = nullptr;
		for (GameObject* object : s_Renderer->GetObjects())
		{
			if (!object || !object->IsCamera())
				continue;
			if (s_LiveObjects.find(object) == s_LiveObjects.end())
				continue;
			if (!fallback)
				fallback = object;
			if (object->DrivesView())
			{
				cam = object;
				break;
			}
		}
		if (!cam)
			cam = fallback;
		if (!cam || cam == s_CameraFollowTarget)
			return;

		const glm::vec3 target = s_CameraFollowTarget->GetWorldPosition();
		glm::vec3 desired(
			target.x + s_CameraFollowOffsetX,
			target.y + s_CameraFollowOffsetY,
			cam->GetPosition().z);

		glm::vec3 pos = cam->GetPosition();
		if (s_CameraFollowSmooth <= 1e-5f || dt <= 0.0f)
		{
			pos.x = desired.x;
			pos.y = desired.y;
		}
		else
		{
			const float alpha = 1.0f - std::exp(-dt / s_CameraFollowSmooth);
			pos.x = pos.x + (desired.x - pos.x) * alpha;
			pos.y = pos.y + (desired.y - pos.y) * alpha;
		}
		cam->SetPosition(pos);
		StorePlayTransform(cam);
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
			object->SetTileCollisionEnabled(true);
			object->SetAllTilesetSolid(true);
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
		ScriptTimers::ClearForObject(object);
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
		DebugDraw2D::Clear();
		ParticleSystem2D::Clear();
		ScriptTimers::ClearAll();
		s_CameraFollowTarget = nullptr;
		s_MouseEnabled = false;
		s_CursorVisible = true;
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
		DebugDraw2D::Update(dt);
		ParticleSystem2D::Update(dt);
		AudioEngine::Update();
		ScriptTimers::Update(dt, [](GameObject* owner)
		{
			s_CurrentObject = owner;
		});
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
