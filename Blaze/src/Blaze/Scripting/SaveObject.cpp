#include "SaveObject.h"
#include "Blaze/Logging/Log.h"
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace Blaze
{
	void SaveObjectData::Clear()
	{
		m_Values.clear();
	}

	void SaveObjectData::SetNumber(const std::string& key, double value)
	{
		Value v;
		v.type = ValueType::Number;
		v.number = value;
		m_Values[key] = v;
	}

	void SaveObjectData::SetString(const std::string& key, const std::string& value)
	{
		Value v;
		v.type = ValueType::String;
		v.text = value;
		m_Values[key] = v;
	}

	void SaveObjectData::SetBool(const std::string& key, bool value)
	{
		Value v;
		v.type = ValueType::Bool;
		v.boolean = value;
		m_Values[key] = v;
	}

	bool SaveObjectData::Has(const std::string& key) const
	{
		return m_Values.find(key) != m_Values.end();
	}

	double SaveObjectData::GetNumber(const std::string& key, double fallback) const
	{
		auto it = m_Values.find(key);
		if (it == m_Values.end() || it->second.type != ValueType::Number)
			return fallback;
		return it->second.number;
	}

	std::string SaveObjectData::GetString(const std::string& key, const std::string& fallback) const
	{
		auto it = m_Values.find(key);
		if (it == m_Values.end() || it->second.type != ValueType::String)
			return fallback;
		return it->second.text;
	}

	bool SaveObjectData::GetBool(const std::string& key, bool fallback) const
	{
		auto it = m_Values.find(key);
		if (it == m_Values.end() || it->second.type != ValueType::Bool)
			return fallback;
		return it->second.boolean;
	}

	bool SaveObjectData::IsNumber(const std::string& key) const
	{
		auto it = m_Values.find(key);
		return it != m_Values.end() && it->second.type == ValueType::Number;
	}

	bool SaveObjectData::IsString(const std::string& key) const
	{
		auto it = m_Values.find(key);
		return it != m_Values.end() && it->second.type == ValueType::String;
	}

	bool SaveObjectData::IsBool(const std::string& key) const
	{
		auto it = m_Values.find(key);
		return it != m_Values.end() && it->second.type == ValueType::Bool;
	}

	std::vector<std::string> SaveObjectData::GetKeys() const
	{
		std::vector<std::string> keys;
		keys.reserve(m_Values.size());
		for (const auto& pair : m_Values)
			keys.push_back(pair.first);
		return keys;
	}

	bool SaveObjectData::LoadFromFile(const std::string& path)
	{
		std::ifstream in(path);
		if (!in.is_open())
			return false;

		Clear();
		std::string line;
		bool headerOk = false;
		while (std::getline(in, line))
		{
			if (line.empty() || line[0] == '#')
				continue;
			std::istringstream stream(line);
			std::string key;
			stream >> key;
			if (key == "BlazeSave")
			{
				headerOk = true;
				continue;
			}
			if (!headerOk)
				continue;

			std::string type;
			stream >> type;
			if (type == "number")
			{
				double value = 0.0;
				stream >> value;
				SetNumber(key, value);
			}
			else if (type == "bool")
			{
				int value = 0;
				stream >> value;
				SetBool(key, value != 0);
			}
			else if (type == "string")
			{
				std::string rest;
				std::getline(stream, rest);
				if (!rest.empty() && rest[0] == ' ')
					rest.erase(rest.begin());
				SetString(key, rest);
			}
		}
		return headerOk;
	}

	bool SaveObjectData::SaveToFile(const std::string& path) const
	{
		std::ofstream out(path);
		if (!out.is_open())
			return false;

		out << "BlazeSave 1\n";
		for (const auto& pair : m_Values)
		{
			switch (pair.second.type)
			{
			case ValueType::Number:
				out << pair.first << " number " << pair.second.number << "\n";
				break;
			case ValueType::Bool:
				out << pair.first << " bool " << (pair.second.boolean ? 1 : 0) << "\n";
				break;
			case ValueType::String:
				out << pair.first << " string " << pair.second.text << "\n";
				break;
			}
		}
		return true;
	}

	const char* ScriptTemplates::KindDisplayName(ScriptKind kind)
	{
		switch (kind)
		{
		case ScriptKind::SaveObject: return "Save Script";
		case ScriptKind::Component:
		default: return "Component Script";
		}
	}

	std::string SaveGameSlots::SanitizeSlotName(const std::string& name)
	{
		std::string out;
		out.reserve(name.size());
		for (char c : name)
		{
			if (std::isalnum((unsigned char)c) || c == '_' || c == '-' || c == ' ')
				out.push_back(c == ' ' ? '_' : c);
		}
		if (out.empty())
			out = "Slot";
		return out;
	}

	std::string SaveGameSlots::ResolveSlotPath(const std::string& nameOrPath)
	{
		if (nameOrPath.empty())
			return {};

		const bool looksLikePath =
			nameOrPath.find('/') != std::string::npos ||
			nameOrPath.find('\\') != std::string::npos ||
			nameOrPath.find(".sav") != std::string::npos ||
			nameOrPath.find(".SAV") != std::string::npos;

		if (looksLikePath)
			return nameOrPath;

		return (fs::path("content") / "saves" / (SanitizeSlotName(nameOrPath) + ".sav")).generic_string();
	}

	bool SaveGameSlots::DoesSlotExist(const std::string& nameOrPath)
	{
		const std::string path = ResolveSlotPath(nameOrPath);
		if (path.empty())
			return false;
		std::error_code ec;
		return fs::exists(path, ec) && fs::is_regular_file(path, ec);
	}

	std::string SaveGameSlots::ClassStem(const std::string& classNameOrPath)
	{
		if (classNameOrPath.empty())
			return {};
		fs::path p(classNameOrPath);
		if (p.has_extension() && p.extension() == ".lua")
			return p.stem().string();
		if (classNameOrPath.find('/') != std::string::npos || classNameOrPath.find('\\') != std::string::npos)
			return p.stem().string();
		return classNameOrPath;
	}

	bool SaveGameSlots::ClassNamesMatch(const std::string& a, const std::string& b)
	{
		return ClassStem(a) == ClassStem(b);
	}

	std::string SaveGameSlots::FindSaveScriptPath(const std::string& classNameOrPath)
	{
		if (classNameOrPath.empty())
			return {};

		std::error_code ec;
		fs::path asPath(classNameOrPath);
		if (asPath.extension() == ".lua" && fs::exists(asPath, ec))
			return asPath.generic_string();
		if (fs::exists(asPath, ec) && fs::is_regular_file(asPath, ec))
			return asPath.generic_string();

		const std::string stem = ClassStem(classNameOrPath);
		const std::string stemLua = stem + ".lua";
		std::string fallback;

		if (fs::exists("content", ec))
		{
			for (auto it = fs::recursive_directory_iterator("content", ec);
				it != fs::recursive_directory_iterator(); ++it)
			{
				if (ec) break;
				if (!it->is_regular_file()) continue;
				if (it->path().extension() != ".lua") continue;
				if (it->path().filename().string() != stemLua && it->path().stem().string() != stem)
					continue;

				const std::string found = it->path().generic_string();
				std::ifstream in(found);
				std::string line;
				bool isSave = false;
				int n = 0;
				while (n < 24 && std::getline(in, line))
				{
					++n;
					if (line.find("Blaze Save Script") != std::string::npos ||
						line.find("SaveObject") != std::string::npos)
					{
						isSave = true;
						break;
					}
				}
				if (isSave)
					return found;
				if (fallback.empty())
					fallback = found;
			}
		}
		return fallback;
	}

	bool ScriptTemplates::WriteScriptFile(const std::string& path, ScriptKind kind, const std::string& hintName)
	{
		std::ofstream out(path);
		if (!out.is_open())
		{
			BZ_CORE_ERROR("Failed to create script: {0}", path);
			return false;
		}

		if (kind == ScriptKind::SaveObject)
		{
			out << "-- Blaze Save Script (SaveObject)\n";
			out << "-- Data class — not attached to GameObjects.\n";
			out << "-- UE-style usage from a Component script:\n";
			out << "--   local data = CreateSaveObject(\"PlayerSave\")\n";
			out << "--   data.coins = 10\n";
			out << "--   SaveSaveObject(\"Slot1\", data)\n";
			out << "--   local loaded = LoadSaveObject(\"Slot1\")\n";
			out << "--   local save = CastSaveObject(loaded, \"PlayerSave\")\n";
			out << "--   -- or: local save = LoadSaveObject(\"Slot1\", \"PlayerSave\")\n\n";
			out << "SaveObject = {\n";
			out << "\tversion = 1,\n";
			if (!hintName.empty())
				out << "\tname = \"" << hintName << "\",\n";
			out << "\tcoins = 0,\n";
			out << "\tlevel = 1,\n";
			out << "}\n\n";
			out << "function SaveObject:OnCreate()\n";
			out << "\t-- called when CreateSaveObject(\"ThisClass\") builds a new instance\n";
			out << "end\n\n";
			out << "function SaveObject:OnLoad()\n";
			out << "\t-- called after CastSaveObject / LoadSaveObject(..., class)\n";
			out << "end\n";
		}
		else
		{
			if (!hintName.empty())
				out << "-- Blaze Component Script for " << hintName << "\n\n";
			else
				out << "-- Blaze Component Script\n";
			out << "-- Attach to a GameObject (Components → Script).\n\n";
			out << "function OnStart()\n";
			out << "\tprint(\"Script started on\", selfName)\n";
			out << "end\n\n";
			out << "function OnUpdate(dt)\n";
			out << "\tif Input.IsKeyDown(\"A\") then\n";
			out << "\t\tprint(\"A held\")\n";
			out << "\tend\n";
			out << "end\n";
		}

		out.close();
		return true;
	}
}
