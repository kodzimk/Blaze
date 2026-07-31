#pragma once
#include "Blaze/Core/Core.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace Blaze
{
	enum class ScriptKind
	{
		Component = 0,
		SaveObject = 1
	};

	// Simple string/number/bool bag used by Save Scripts (UE SaveGame-like).
	class BLAZE_API SaveObjectData
	{
	public:
		void Clear();
		void SetNumber(const std::string& key, double value);
		void SetString(const std::string& key, const std::string& value);
		void SetBool(const std::string& key, bool value);
		bool Has(const std::string& key) const;
		double GetNumber(const std::string& key, double fallback = 0.0) const;
		std::string GetString(const std::string& key, const std::string& fallback = {}) const;
		bool GetBool(const std::string& key, bool fallback = false) const;
		bool IsNumber(const std::string& key) const;
		bool IsString(const std::string& key) const;
		bool IsBool(const std::string& key) const;
		std::vector<std::string> GetKeys() const;

		bool LoadFromFile(const std::string& path);
		bool SaveToFile(const std::string& path) const;

	private:
		enum class ValueType { Number, String, Bool };
		struct Value
		{
			ValueType type = ValueType::Number;
			double number = 0.0;
			std::string text;
			bool boolean = false;
		};
		std::unordered_map<std::string, Value> m_Values;
	};

	// UE-style save slots: SaveSaveObject("Slot1", data) → content/saves/Slot1.sav
	class BLAZE_API SaveGameSlots
	{
	public:
		static std::string SanitizeSlotName(const std::string& name);
		static std::string ResolveSlotPath(const std::string& nameOrPath);
		static bool DoesSlotExist(const std::string& nameOrPath);
		static std::string FindSaveScriptPath(const std::string& classNameOrPath);
		static std::string ClassStem(const std::string& classNameOrPath);
		static bool ClassNamesMatch(const std::string& a, const std::string& b);
	};

	class BLAZE_API ScriptTemplates
	{
	public:
		static bool WriteScriptFile(const std::string& path, ScriptKind kind, const std::string& hintName = {});
		static const char* KindDisplayName(ScriptKind kind);
	};
}
