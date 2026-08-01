#include "InputActions.h"
#include "Blaze/Core/Input.h"
#include "Blaze/Core/Project.h"
#include "Blaze/Logging/Log.h"
#include <filesystem>
#include <cmath>

namespace fs = std::filesystem;

namespace Blaze
{
	InputActionsAsset InputActions::s_Asset;
	bool InputActions::s_Loaded = false;
	std::unordered_map<std::string, InputActions::RuntimeState> InputActions::s_States;

	bool InputActions::EvalBindingHeld(const InputBinding& binding)
	{
		if (binding.code < 0)
			return false;
		if (binding.device == InputBindDevice::Mouse)
			return Input::IsMouseButtonPressed(binding.code);
		return Input::IsKeyPressed(binding.code);
	}

	void InputActions::RebuildRuntime()
	{
		s_States.clear();
		for (const InputActionDef& action : s_Asset.GetActions())
			s_States[action.name] = RuntimeState{};
	}

	bool InputActions::Load(const std::string& path)
	{
		InputActionsAsset asset;
		if (!asset.LoadFromFile(path))
			return false;
		s_Asset = std::move(asset);
		s_Loaded = true;
		RebuildRuntime();
		BZ_INFO("Loaded input actions: {0}", path);
		return true;
	}

	bool InputActions::LoadDefault()
	{
		fs::path path;
		if (Project::HasProject() && !Project::GetDirectory().empty())
			path = fs::path(Project::GetDirectory()) / "content" / "input" / "Default.inputactions";
		else
			path = fs::path("content") / "input" / "Default.inputactions";

		std::error_code ec;
		if (!fs::exists(path, ec))
		{
			fs::create_directories(path.parent_path(), ec);
			InputActionsAsset asset = InputActionsAsset::CreateDefault("Default");
			asset.SetPath(path.string());
			if (!asset.SaveToFile(path.string()))
			{
				// Still use in-memory defaults.
				s_Asset = std::move(asset);
				s_Loaded = true;
				RebuildRuntime();
				BZ_INFO("Using in-memory default input actions (save failed)");
				return true;
			}
			BZ_INFO("Created default input actions: {0}", path.string());
		}
		return Load(path.string());
	}

	bool InputActions::EnsureLoaded()
	{
		if (s_Loaded)
			return true;
		return LoadDefault();
	}

	void InputActions::Unload()
	{
		s_Asset = InputActionsAsset{};
		s_States.clear();
		s_Loaded = false;
	}

	void InputActions::ResetEdges()
	{
		for (auto& pair : s_States)
		{
			pair.second.wasHeld = pair.second.held;
			pair.second.held = false;
			pair.second.axis = 0.0f;
			pair.second.vector = glm::vec2(0.0f);
		}
	}

	void InputActions::Tick()
	{
		if (!s_Loaded)
			EnsureLoaded();
		if (!s_Loaded)
			return;

		for (const InputActionDef& action : s_Asset.GetActions())
		{
			RuntimeState& state = s_States[action.name];
			state.wasHeld = state.held;

			if (action.type == InputActionType::Button)
			{
				bool held = false;
				for (const InputBinding& bind : action.bindings)
				{
					if (EvalBindingHeld(bind))
					{
						held = true;
						break;
					}
				}
				state.held = held;
				state.axis = held ? 1.0f : 0.0f;
				state.vector = glm::vec2(state.axis, 0.0f);
			}
			else if (action.type == InputActionType::Axis1D)
			{
				float positive = 0.0f;
				float negative = 0.0f;
				for (const InputBinding& bind : action.bindings)
				{
					if (!EvalBindingHeld(bind))
						continue;
					if (bind.role == InputAxisRole::Negative)
						negative = 1.0f;
					else
						positive = 1.0f; // Positive or None
				}
				state.axis = positive - negative;
				state.held = std::fabs(state.axis) > 1e-4f;
				state.vector = glm::vec2(state.axis, 0.0f);
			}
			else // Axis2D
			{
				float up = 0.0f, down = 0.0f, left = 0.0f, right = 0.0f;
				for (const InputBinding& bind : action.bindings)
				{
					if (!EvalBindingHeld(bind))
						continue;
					switch (bind.role)
					{
					case InputAxisRole::Up: up = 1.0f; break;
					case InputAxisRole::Down: down = 1.0f; break;
					case InputAxisRole::Left: left = 1.0f; break;
					case InputAxisRole::Right: right = 1.0f; break;
					case InputAxisRole::Positive: right = 1.0f; break;
					case InputAxisRole::Negative: left = 1.0f; break;
					default: break;
					}
				}
				glm::vec2 v(right - left, up - down);
				const float len = glm::length(v);
				if (len > 1.0f)
					v /= len;
				state.vector = v;
				state.axis = v.x;
				state.held = len > 1e-4f;
			}
		}
	}

	bool InputActions::WasPressed(const std::string& action)
	{
		auto it = s_States.find(action);
		if (it == s_States.end())
			return false;
		return it->second.held && !it->second.wasHeld;
	}

	bool InputActions::WasReleased(const std::string& action)
	{
		auto it = s_States.find(action);
		if (it == s_States.end())
			return false;
		return !it->second.held && it->second.wasHeld;
	}

	bool InputActions::IsHeld(const std::string& action)
	{
		auto it = s_States.find(action);
		if (it == s_States.end())
			return false;
		return it->second.held;
	}

	float InputActions::GetAxis(const std::string& action)
	{
		auto it = s_States.find(action);
		if (it == s_States.end())
			return 0.0f;
		return it->second.axis;
	}

	glm::vec2 InputActions::GetVector(const std::string& action)
	{
		auto it = s_States.find(action);
		if (it == s_States.end())
			return glm::vec2(0.0f);
		return it->second.vector;
	}

	bool InputActions::HasAction(const std::string& action)
	{
		return s_States.find(action) != s_States.end();
	}

	const std::string& InputActions::GetLoadedPath()
	{
		return s_Asset.GetPath();
	}

	bool InputActions::IsLoaded()
	{
		return s_Loaded;
	}

	const InputActionsAsset& InputActions::GetAsset()
	{
		return s_Asset;
	}
}
