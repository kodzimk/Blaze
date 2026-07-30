#include "ScriptEngine.h"
#include "OnScreenDebug.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Core/Input.h"
#include "Blaze/Event/KeyCodes.h"
#include "Blaze/Event/MouseButtonCodes.h"
#include "Blaze/Logging/Log.h"

// Soft safeties: full SOL_ALL_SAFETIES_ON can abort() the process on script errors.
#define SOL_SAFE_FUNCTIONS 1
#define SOL_SAFE_USERTYPE 1
#define SOL_EXCEPTIONS_SAFE_PROPAGATION 1
#include "sol.hpp"

#include "../vendor/imgui/src/imgui.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <memory>
#include <regex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

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
		bool started = false;
	};

	static std::unique_ptr<sol::state> s_Lua;
	static bool s_Ready = false;
	static std::unordered_map<GameObject*, ScriptInstance> s_Instances;
	static std::unordered_map<int, bool> s_KeyWasDown;
	static std::unordered_map<int, bool> s_MouseWasDown;
	static GameObject* s_CurrentObject = nullptr;
	static std::vector<GameObject*> s_PendingDestroy;
	static std::unordered_set<GameObject*> s_LiveObjects;

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
		entry.objectName = object ? object->GetName() : "<none>";
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
		t.position = object->GetPosition();
		t.rotation = object->GetRotation();
		t.scale = object->GetScale();
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
			glm::vec3 pos = t.position;
			object->SetPosition(pos);
			object->SetRotation(t.rotation);
			object->SetScale(t.scale);
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
		// Snapshot current key/mouse state after scripts so next frame edges work.
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

	static void BindApi(sol::state& lua)
	{
		lua.open_libraries(sol::lib::base, sol::lib::math, sol::lib::string, sol::lib::table);

		lua.new_usertype<ScriptObject>("GameObject",
			sol::no_constructor,
			"GetName", &ScriptObject::GetName,
			"GetLocation", &ScriptObject::GetLocation,
			"GetRotation", &ScriptObject::GetRotation,
			"GetScale", &ScriptObject::GetScale,
			"IsValid", &ScriptObject::IsValid);

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
		// True only on the frame the key goes down (tap), not while held.
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

		// Alias for SetLocation
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
		s_Lua.reset();
		s_Ready = false;
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
		s_PlayTransforms.clear();
		s_KeyWasDown.clear();
		s_MouseWasDown.clear();
		s_PendingDestroy.clear();
		s_LiveObjects.clear();
		s_CurrentObject = nullptr;
		OnScreenDebug::Clear();
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

			s_Instances[object] = std::move(instance);
		}

		if (!compileOk)
		{
			BZ_ERROR("Play cancelled — fix Lua errors before Start");
			std::fprintf(stderr, "\n*** Play cancelled — fix Lua errors before Start ***\n\n");
			std::fflush(stderr);
			s_Instances.clear();
			s_PlayTransforms.clear();
			s_CurrentObject = nullptr;
			return false;
		}

		ApplyPlayTransforms();
		try
		{
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
				const char* phase = begin ? "OnCollisionBegin" : "OnCollisionEnd";

				auto itA = s_Instances.find(ev.a);
				if (itA != s_Instances.end())
				{
					sol::protected_function& fn = begin ? itA->second.onCollisionBegin : itA->second.onCollisionEnd;
					CallCollisionCallback(itA->second, fn, ev.b, phase);
				}

				auto itB = s_Instances.find(ev.b);
				if (itB != s_Instances.end())
				{
					sol::protected_function& fn = begin ? itB->second.onCollisionBegin : itB->second.onCollisionEnd;
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
