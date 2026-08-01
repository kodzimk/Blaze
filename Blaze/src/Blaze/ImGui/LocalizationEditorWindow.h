#pragma once
#include "bzpch.h"
#include "Blaze/Localization/LocalizationAsset.h"

namespace Blaze
{
	class BLAZE_API LocalizationEditorWindow
	{
	public:
		LocalizationEditorWindow();
		~LocalizationEditorWindow();

		void OnUpdate();
		bool IsOpen() const { return m_Open; }
		bool* GetOpenPtr() { return &m_Open; }

		static void OpenFile(const std::string& path);

	private:
		void Open(const std::string& path);
		bool SaveFile();
		void DrawEntryRow(int index);

		bool m_Open = false;
		bool m_Dirty = false;
		LocalizationAsset m_Asset;
		char m_NewKey[128] = {};
		char m_NewValue[256] = {};
		char m_LangBuf[32] = {};

		static LocalizationEditorWindow* s_Instance;
	};
}
