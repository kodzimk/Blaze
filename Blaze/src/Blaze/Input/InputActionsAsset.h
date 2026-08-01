#pragma once
#include "Blaze/Core/Core.h"
#include <string>
#include <vector>

namespace Blaze
{
	enum class InputActionType
	{
		Button = 0,
		Axis1D,
		Axis2D
	};

	enum class InputBindDevice
	{
		Key = 0,
		Mouse
	};

	enum class InputAxisRole
	{
		None = 0,
		Positive,
		Negative,
		Up,
		Down,
		Left,
		Right
	};

	struct BLAZE_API InputBinding
	{
		InputBindDevice device = InputBindDevice::Key;
		int code = -1;
		InputAxisRole role = InputAxisRole::None;
		std::string raw; // original token for save (e.g. "Space", "Left")
	};

	struct BLAZE_API InputActionDef
	{
		std::string name;
		InputActionType type = InputActionType::Button;
		std::vector<InputBinding> bindings;
	};

	class BLAZE_API InputActionsAsset
	{
	public:
		const std::string& GetPath() const { return m_Path; }
		void SetPath(const std::string& path) { m_Path = path; }
		const std::string& GetMapName() const { return m_MapName; }
		void SetMapName(const std::string& name) { m_MapName = name; }

		std::vector<InputActionDef>& GetActions() { return m_Actions; }
		const std::vector<InputActionDef>& GetActions() const { return m_Actions; }

		int FindAction(const std::string& name) const;
		int AddAction(const std::string& name, InputActionType type);
		bool RemoveAction(const std::string& name);

		bool LoadFromFile(const std::string& path);
		bool SaveToFile(const std::string& path) const;

		static InputActionsAsset CreateDefault(const std::string& name = "Default");

		static int ResolveKeyCode(std::string key);
		static int ResolveMouseButton(std::string button);
		static std::string KeyCodeToName(int code);
		static std::string MouseButtonToName(int button);

	private:
		std::string m_Path;
		std::string m_MapName = "Default";
		std::vector<InputActionDef> m_Actions;
	};
}
