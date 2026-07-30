#pragma once
#include "bzpch.h"
#include "Blaze/Core/Core.h"
#include <vector>

struct ImGuiInputTextCallbackData;
struct ImVec2;

namespace Blaze
{
	class BLAZE_API ScriptEditorWindow
	{
	public:
		ScriptEditorWindow();
		~ScriptEditorWindow();

		static void OpenFile(const std::string& path);

		void Open(const std::string& path);
		void OnUpdate();
		bool IsOpen() const { return m_Open; }
		bool* GetOpenPtr() { return &m_Open; }

		static bool IsFocused();
		static bool IsBlockingMainEditor();

	private:
		struct CompletionItem
		{
			const char* insert;
			const char* label;
			const char* detail;
		};

		struct EditUserData
		{
			std::vector<char>* buf = nullptr;
			ScriptEditorWindow* editor = nullptr;
		};

		bool LoadFile(const std::string& path);
		bool SaveFile();
		void SyncEditBufferFromString();
		void SyncStringFromEditBuffer();

		void RebuildCompletionFilter(const char* buf, int cursor);
		void AcceptCompletion(int index = -1);
		bool ApplyCompletionToCallback(ImGuiInputTextCallbackData* data);
		void ApplyCompletionToBuffer();
		void DrawCompletionsPopup();
		void DrawLuaSyntaxHighlight(const char* text, const ImVec2& textPos);

		static int ScriptEditCallback(ImGuiInputTextCallbackData* data);
		static const CompletionItem* GetCompletionCatalog(int& outCount);

		static ScriptEditorWindow* s_Instance;

		bool m_Open = false;
		bool m_Dirty = false;
		bool m_FocusRequest = false;
		bool m_PlaceFloating = false;
		bool m_WindowFocused = false;

		std::string m_Path;
		std::string m_Buffer;
		std::vector<char> m_EditBuf;

		bool m_ShowCompletions = false;
		bool m_ForceCompletions = false;
		bool m_AcceptCompletion = false;
		bool m_SuppressCompletions = false;
		bool m_RefocusEditor = false;
		bool m_HasPopupAnchor = false;
		float m_PopupAnchorX = 0.0f;
		float m_PopupAnchorY = 0.0f;
		int m_CursorPos = 0;
		int m_WordStart = 0;
		int m_SelectedCompletion = 0;
		std::string m_Prefix;
		std::string m_PendingInsert;
		std::vector<const CompletionItem*> m_Filtered;
		EditUserData m_EditUserData;
	};
}
