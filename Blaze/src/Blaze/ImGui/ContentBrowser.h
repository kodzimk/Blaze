#pragma once
#include"bzpch.h"
#include"Blaze/Scripting/SaveObject.h"

struct GLFWwindow;
struct ImVec2;
struct ImGuiIO;
struct ImGuiStyle;
struct ImGuiContext;

namespace Blaze
{
	class Texture;

	class BLAZE_API ContentBrowser
	{
	public:
		ContentBrowser();
		virtual ~ContentBrowser();

		void OnUpdate();
		bool OnFocus() const { return onFocus; }
		bool IsOpen() const { return m_Open; }
		bool* GetOpenPtr() { return &m_Open; }
		static void SetSize(float width, float height);

		void SetRootDirectory(const std::string& path);

		static bool ConsumePendingDrop(std::string& outPath);
		static bool ConsumePendingOpenMap(std::string& outPath);
	private:
		bool IsTextureFile(std::string extension) const;
		bool IsAudioFile(std::string extension) const;
		bool IsFontFile(std::string extension) const;
		bool IsScriptFile(std::string extension) const;
		bool IsSaveScriptFile(const std::string& path) const;
		bool IsMapFile(std::string extension) const;
		bool IsWidgetFile(std::string extension) const;
		bool IsFlipbookFile(std::string extension) const;
		Texture* GetThumbnail(const std::string& path);
		void ImportTexture();
		void ImportAudio();
		void ImportFont();
		void ImportTiledMap();
		void CreateScript();
		void CreateScriptOfKind(ScriptKind kind);
		void CreateMap();
		void CreateWidget();
		void CreateFlipbook();
		void CreateFolder();
		void StartRename(const std::string& path);
		void CommitRename();
		void DrawContextMenu();
		void DrawCreateScriptPopup();
		bool MoveAssetToFolder(const std::string& assetPath, const std::string& folderPath);
		bool AcceptAssetDrop(const std::string& folderPath);
		void InvalidateThumbnail(const std::string& path);
		void DrawAssetRenameLabel(const std::string& fullPath, const std::string& name, float width);
	private:
		bool onFocus;
		bool m_Open = true;
		std::string m_RootDir;
		std::string m_CurrentDir;
		std::unordered_map<std::string, Texture*> m_Thumbnails;

		std::string m_RenamingPath;
		char m_RenameBuffer[256] = {};
		bool m_FocusRename = false;
		bool m_OpenScriptTypePopup = false;

		static std::string s_DraggingPath;
		static std::string s_PendingDropPath;
		static std::string s_PendingOpenMap;
	public:
		static float m_width;
		static float m_height;
	};
}
