#pragma once
#include "Blaze/Core/Core.h"
#include <string>
#include <iosfwd>

namespace Blaze
{
	class GameObject;

	// Filtered component pack (.compreset). Applies onto an existing GameObject
	// without changing name, type, transform, or parent. Prefabs spawn trees;
	// presets paint components.
	class BLAZE_API ComponentPresetAsset
	{
	public:
		const std::string& GetPath() const { return m_Path; }
		void SetPath(const std::string& path) { m_Path = path; }
		const std::string& GetPresetName() const { return m_PresetName; }
		void SetPresetName(const std::string& name) { m_PresetName = name; }

		bool LoadFromFile(const std::string& path);
		bool SaveToFile(const std::string& path) const;
		bool SaveFromObject(GameObject* object, const std::string& path);

		static bool ApplyToObject(const std::string& path, GameObject* object);
		static bool ApplyFromStream(std::istream& in, GameObject* object);

		static ComponentPresetAsset CreateDefault(const std::string& name = "Preset");

	private:
		std::string m_Path;
		std::string m_PresetName = "Preset";
	};
}
