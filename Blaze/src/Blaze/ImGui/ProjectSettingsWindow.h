#pragma once
#include"Blaze/Core/Core.h"
#include"bzpch.h"

namespace Blaze
{
	class BLAZE_API ProjectSettingsWindow
	{
	public:
		ProjectSettingsWindow();
		virtual ~ProjectSettingsWindow();

		void OnUpdate();
		void Open();
		bool IsOpen() const { return m_Open; }
		bool* GetOpenPtr() { return &m_Open; }

		static bool ConsumePendingOpenMap(std::string& outPath);

	private:
		void SyncFromProject();
		void ApplyToProject();

		bool m_Open = false;
		bool m_NeedsSync = true;
		char m_GameNameBuffer[256] = {};
		int m_SelectedMapIndex = 0;
		std::vector<std::string> m_MapFiles;
		static std::string s_PendingOpenMap;
	};
}
