#pragma once
#include "Blaze/Core/Core.h"
#include "Blaze/Input/InputActionsAsset.h"
#include "glm.hpp"
#include <string>
#include <unordered_map>
#include <vector>

namespace Blaze
{
	// Named input actions loaded from .inputactions. Call Tick() once per play frame
	// before scripts so WasPressed / axes match the current frame.
	class BLAZE_API InputActions
	{
	public:
		static bool Load(const std::string& path);
		static bool LoadDefault(); // content/input/Default.inputactions (creates if missing)
		static bool EnsureLoaded();
		static void Unload();
		static void ResetEdges();

		static void Tick();

		static bool WasPressed(const std::string& action);
		static bool WasReleased(const std::string& action);
		static bool IsHeld(const std::string& action);
		static float GetAxis(const std::string& action);
		static glm::vec2 GetVector(const std::string& action);

		static bool HasAction(const std::string& action);
		static const std::string& GetLoadedPath();
		static bool IsLoaded();
		static const InputActionsAsset& GetAsset();

	private:
		struct RuntimeState
		{
			bool held = false;
			bool wasHeld = false;
			float axis = 0.0f;
			glm::vec2 vector{ 0.0f };
		};

		static bool EvalBindingHeld(const InputBinding& binding);
		static void RebuildRuntime();

		static InputActionsAsset s_Asset;
		static bool s_Loaded;
		static std::unordered_map<std::string, RuntimeState> s_States;
	};
}
