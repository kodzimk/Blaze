#include "InputActionsAsset.h"
#include "Blaze/Event/KeyCodes.h"
#include "Blaze/Event/MouseButtonCodes.h"
#include "Blaze/Logging/Log.h"
#include <fstream>
#include <sstream>
#include <cctype>
#include <algorithm>

namespace Blaze
{
	int InputActionsAsset::ResolveKeyCode(std::string key)
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

	int InputActionsAsset::ResolveMouseButton(std::string button)
	{
		for (char& c : button)
			c = (char)std::toupper((unsigned char)c);
		if (button == "LEFT" || button == "MOUSELEFT" || button == "LBUTTON")
			return BZ_MOUSE_BUTTON_LEFT;
		if (button == "RIGHT" || button == "MOUSERIGHT" || button == "RBUTTON")
			return BZ_MOUSE_BUTTON_RIGHT;
		if (button == "MIDDLE" || button == "MOUSEMIDDLE" || button == "MBUTTON")
			return BZ_MOUSE_BUTTON_MIDDLE;
		return -1;
	}

	std::string InputActionsAsset::KeyCodeToName(int code)
	{
		if (code >= BZ_KEY_A && code <= BZ_KEY_Z)
			return std::string(1, (char)('A' + (code - BZ_KEY_A)));
		if (code >= BZ_KEY_0 && code <= BZ_KEY_9)
			return std::string(1, (char)('0' + (code - BZ_KEY_0)));
		switch (code)
		{
		case BZ_KEY_SPACE: return "Space";
		case BZ_KEY_LEFT: return "Left";
		case BZ_KEY_RIGHT: return "Right";
		case BZ_KEY_UP: return "Up";
		case BZ_KEY_DOWN: return "Down";
		case BZ_KEY_ENTER: return "Enter";
		case BZ_KEY_ESCAPE: return "Escape";
		case BZ_KEY_TAB: return "Tab";
		case BZ_KEY_LEFT_SHIFT: return "LeftShift";
		case BZ_KEY_RIGHT_SHIFT: return "RightShift";
		case BZ_KEY_LEFT_CONTROL: return "LeftControl";
		case BZ_KEY_RIGHT_CONTROL: return "RightControl";
		case BZ_KEY_LEFT_ALT: return "LeftAlt";
		case BZ_KEY_RIGHT_ALT: return "RightAlt";
		default: return {};
		}
	}

	std::string InputActionsAsset::MouseButtonToName(int button)
	{
		switch (button)
		{
		case BZ_MOUSE_BUTTON_LEFT: return "Left";
		case BZ_MOUSE_BUTTON_RIGHT: return "Right";
		case BZ_MOUSE_BUTTON_MIDDLE: return "Middle";
		default: return {};
		}
	}

	int InputActionsAsset::FindAction(const std::string& name) const
	{
		for (int i = 0; i < (int)m_Actions.size(); ++i)
		{
			if (m_Actions[i].name == name)
				return i;
		}
		return -1;
	}

	int InputActionsAsset::AddAction(const std::string& name, InputActionType type)
	{
		std::string unique = name.empty() ? "Action" : name;
		if (FindAction(unique) >= 0)
		{
			for (int i = 2; i < 10000; ++i)
			{
				const std::string candidate = unique + "_" + std::to_string(i);
				if (FindAction(candidate) < 0)
				{
					unique = candidate;
					break;
				}
			}
		}
		InputActionDef action;
		action.name = unique;
		action.type = type;
		m_Actions.push_back(action);
		return (int)m_Actions.size() - 1;
	}

	bool InputActionsAsset::RemoveAction(const std::string& name)
	{
		const int index = FindAction(name);
		if (index < 0)
			return false;
		m_Actions.erase(m_Actions.begin() + index);
		return true;
	}

	static InputActionType ParseActionType(const std::string& s)
	{
		std::string t = s;
		for (char& c : t)
			c = (char)std::toupper((unsigned char)c);
		if (t == "AXIS1D" || t == "AXIS")
			return InputActionType::Axis1D;
		if (t == "AXIS2D" || t == "VECTOR" || t == "VECTOR2")
			return InputActionType::Axis2D;
		return InputActionType::Button;
	}

	static const char* ActionTypeToString(InputActionType type)
	{
		switch (type)
		{
		case InputActionType::Axis1D: return "Axis1D";
		case InputActionType::Axis2D: return "Axis2D";
		default: return "Button";
		}
	}

	static InputAxisRole ParseRole(const std::string& s)
	{
		std::string t = s;
		for (char& c : t)
			c = (char)std::toupper((unsigned char)c);
		if (t == "POSITIVE" || t == "POS") return InputAxisRole::Positive;
		if (t == "NEGATIVE" || t == "NEG") return InputAxisRole::Negative;
		if (t == "UP") return InputAxisRole::Up;
		if (t == "DOWN") return InputAxisRole::Down;
		if (t == "LEFT") return InputAxisRole::Left;
		if (t == "RIGHT") return InputAxisRole::Right;
		return InputAxisRole::None;
	}

	static const char* RoleToString(InputAxisRole role)
	{
		switch (role)
		{
		case InputAxisRole::Positive: return "Positive";
		case InputAxisRole::Negative: return "Negative";
		case InputAxisRole::Up: return "Up";
		case InputAxisRole::Down: return "Down";
		case InputAxisRole::Left: return "Left";
		case InputAxisRole::Right: return "Right";
		default: return "";
		}
	}

	bool InputActionsAsset::LoadFromFile(const std::string& path)
	{
		std::ifstream in(path);
		if (!in.is_open())
		{
			BZ_CORE_ERROR("Failed to open input actions: {0}", path);
			return false;
		}

		m_Path = path;
		m_MapName = "Default";
		m_Actions.clear();

		std::string line;
		bool headerOk = false;
		while (std::getline(in, line))
		{
			if (line.empty() || line[0] == '#')
				continue;
			std::istringstream stream(line);
			std::string key;
			stream >> key;
			if (key == "BlazeInputActions")
			{
				headerOk = true;
				continue;
			}
			if (!headerOk)
				continue;

			if (key == "map")
			{
				stream >> m_MapName;
			}
			else if (key == "action")
			{
				std::string name, typeStr;
				stream >> name >> typeStr;
				if (name.empty())
					continue;
				InputActionDef action;
				action.name = name;
				action.type = ParseActionType(typeStr);
				if (FindAction(name) < 0)
					m_Actions.push_back(action);
			}
			else if (key == "bind")
			{
				std::string actionName;
				stream >> actionName;
				int actionIndex = FindAction(actionName);
				if (actionIndex < 0)
					continue;

				std::string token1;
				if (!(stream >> token1))
					continue;

				InputBinding binding;
				InputAxisRole role = ParseRole(token1);
				std::string deviceToken = token1;
				if (role != InputAxisRole::None)
				{
					binding.role = role;
					if (!(stream >> deviceToken))
						continue;
				}

				std::string deviceUpper = deviceToken;
				for (char& c : deviceUpper)
					c = (char)std::toupper((unsigned char)c);

				std::string codeName;
				if (deviceUpper == "KEY" || deviceUpper == "MOUSE")
				{
					if (!(stream >> codeName))
						continue;
					binding.raw = codeName;
					if (deviceUpper == "MOUSE")
					{
						binding.device = InputBindDevice::Mouse;
						binding.code = ResolveMouseButton(codeName);
					}
					else
					{
						binding.device = InputBindDevice::Key;
						binding.code = ResolveKeyCode(codeName);
					}
				}
				else
				{
					binding.raw = deviceToken;
					binding.code = ResolveKeyCode(deviceToken);
					binding.device = InputBindDevice::Key;
					if (binding.code < 0)
					{
						binding.device = InputBindDevice::Mouse;
						binding.code = ResolveMouseButton(deviceToken);
					}
				}

				if (binding.code >= 0)
					m_Actions[actionIndex].bindings.push_back(binding);
			}
		}

		if (!headerOk)
		{
			BZ_CORE_ERROR("Invalid input actions header: {0}", path);
			return false;
		}
		return true;
	}

	bool InputActionsAsset::SaveToFile(const std::string& path) const
	{
		std::ofstream out(path);
		if (!out.is_open())
		{
			BZ_CORE_ERROR("Failed to save input actions: {0}", path);
			return false;
		}

		out << "BlazeInputActions 1\n";
		out << "map " << (m_MapName.empty() ? "Default" : m_MapName) << "\n";
		for (const InputActionDef& action : m_Actions)
		{
			out << "action " << action.name << " " << ActionTypeToString(action.type) << "\n";
			for (const InputBinding& bind : action.bindings)
			{
				out << "bind " << action.name;
				const char* role = RoleToString(bind.role);
				if (role[0] != '\0')
					out << " " << role;
				if (bind.device == InputBindDevice::Mouse)
					out << " Mouse " << (bind.raw.empty() ? MouseButtonToName(bind.code) : bind.raw);
				else
					out << " Key " << (bind.raw.empty() ? KeyCodeToName(bind.code) : bind.raw);
				out << "\n";
			}
		}
		return true;
	}

	InputActionsAsset InputActionsAsset::CreateDefault(const std::string& name)
	{
		InputActionsAsset asset;
		asset.SetMapName(name.empty() ? "Default" : name);

		auto addKey = [](InputActionDef& action, const char* key, InputAxisRole role = InputAxisRole::None)
		{
			InputBinding b;
			b.device = InputBindDevice::Key;
			b.raw = key;
			b.code = ResolveKeyCode(key);
			b.role = role;
			if (b.code >= 0)
				action.bindings.push_back(b);
		};
		auto addMouse = [](InputActionDef& action, const char* button)
		{
			InputBinding b;
			b.device = InputBindDevice::Mouse;
			b.raw = button;
			b.code = ResolveMouseButton(button);
			if (b.code >= 0)
				action.bindings.push_back(b);
		};

		{
			InputActionDef jump;
			jump.name = "Jump";
			jump.type = InputActionType::Button;
			addKey(jump, "Space");
			asset.GetActions().push_back(jump);
		}
		{
			InputActionDef fire;
			fire.name = "Fire";
			fire.type = InputActionType::Button;
			addMouse(fire, "Left");
			asset.GetActions().push_back(fire);
		}
		{
			InputActionDef move;
			move.name = "Move";
			move.type = InputActionType::Axis2D;
			addKey(move, "W", InputAxisRole::Up);
			addKey(move, "S", InputAxisRole::Down);
			addKey(move, "A", InputAxisRole::Left);
			addKey(move, "D", InputAxisRole::Right);
			addKey(move, "Up", InputAxisRole::Up);
			addKey(move, "Down", InputAxisRole::Down);
			addKey(move, "Left", InputAxisRole::Left);
			addKey(move, "Right", InputAxisRole::Right);
			asset.GetActions().push_back(move);
		}
		{
			InputActionDef horizontal;
			horizontal.name = "Horizontal";
			horizontal.type = InputActionType::Axis1D;
			addKey(horizontal, "A", InputAxisRole::Negative);
			addKey(horizontal, "D", InputAxisRole::Positive);
			addKey(horizontal, "Left", InputAxisRole::Negative);
			addKey(horizontal, "Right", InputAxisRole::Positive);
			asset.GetActions().push_back(horizontal);
		}
		{
			InputActionDef vertical;
			vertical.name = "Vertical";
			vertical.type = InputActionType::Axis1D;
			addKey(vertical, "S", InputAxisRole::Negative);
			addKey(vertical, "W", InputAxisRole::Positive);
			addKey(vertical, "Down", InputAxisRole::Negative);
			addKey(vertical, "Up", InputAxisRole::Positive);
			asset.GetActions().push_back(vertical);
		}

		return asset;
	}
}
