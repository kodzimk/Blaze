#pragma once
#include "Blaze/Core/Core.h"
#include "bzpch.h"
#include <functional>

namespace Blaze
{
	class BLAZE_API BuildSettingsWindow
	{
	public:
		BuildSettingsWindow();
		virtual ~BuildSettingsWindow();

		void OnUpdate();
		void Open();
		bool IsOpen() const { return m_Open; }
		bool* GetOpenPtr() { return &m_Open; }

		void SetSaveSceneCallback(std::function<bool()> fn) { m_SaveScene = std::move(fn); }

	private:
		void SyncFromProject();
		void ApplyToProject();
		bool RunBuild(bool promptIfNoPath);
		void BrowseOutputExe();

		bool m_Open = false;
		bool m_NeedsSync = true;
		char m_GameNameBuffer[256] = {};
		char m_OutputExeBuffer[260] = {};
		int m_SelectedMapIndex = 0;
		std::vector<std::string> m_MapFiles;
		std::vector<char> m_MapIncluded;
		std::function<bool()> m_SaveScene;
	};
}
