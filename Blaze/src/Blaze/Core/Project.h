#pragma once
#include"bzpch.h"
#include"Blaze/Core/Core.h"

namespace Blaze
{
	class Renderer;
	class ObjectWindow;

	class BLAZE_API Project
	{
	public:
		static bool NewProject();
		static bool OpenProjectDialog();
		static bool SaveScene(Renderer& renderer, ObjectWindow* objectsWindow = nullptr);
		static bool SaveSceneAs(Renderer& renderer, ObjectWindow* objectsWindow = nullptr);
		static bool LoadScene(Renderer& renderer, const std::vector<float>& vertices, const std::vector<unsigned int>& indices, ObjectWindow* objectsWindow = nullptr);
		static bool LoadMap(const std::string& mapPath, Renderer& renderer, const std::vector<float>& vertices, const std::vector<unsigned int>& indices, ObjectWindow* objectsWindow = nullptr);
		static bool SaveMap(const std::string& mapPath, Renderer& renderer, ObjectWindow* objectsWindow = nullptr);
		static bool SaveProjectSettings();
		static bool BuildGame();

		static bool HasProject();
		static void CloseProject();
		static const std::string& GetProjectFile();
		static const std::string& GetDirectory();
		static std::string GetContentDirectory();
		static const std::string& GetName();
		static const std::string& GetGameName();
		static void SetGameName(const std::string& name);
		static const std::string& GetActiveMap();
		static void SetActiveMap(const std::string& mapPath);
		static const std::string& GetStartingMap();
		static void SetStartingMap(const std::string& mapPath);
		static const std::string& GetMapScript();
		static void SetMapScript(const std::string& scriptPath);
		static void ClearMapScript();
		// Creates content/scripts/maps/<MapStem>_Map.lua and returns a project-relative path.
		static std::string CreateMapScriptForMap(const std::string& mapPath);
		// If the active map has no script, create and assign one.
		static std::string EnsureMapScript();
		static std::string GetDefaultMapPath();
		static std::vector<std::string> ListMapFiles();
		// Resolves "Main", "Main.map", or a relative path to an absolute/usable map file.
		static std::string ResolveMapPath(const std::string& mapNameOrPath);

		static void SetProjectFromFile(const std::string& projectFile);
		static std::vector<std::string> GetRecentProjects();
		static void RegisterRecentProject(const std::string& projectFile);
	private:
		static bool SetupProjectAt(const std::string& pickedFile);
		static std::string StoreTexturePath(const std::string& texturePath);
		static bool WriteSceneFile(const std::string& path, Renderer& renderer, ObjectWindow* objectsWindow, bool asMap);
		static bool ReadSceneFile(const std::string& path, Renderer& renderer, const std::vector<float>& vertices, const std::vector<unsigned int>& indices, ObjectWindow* objectsWindow);
		static bool WriteProjectPointer();
		static std::string MakeProjectRelative(const std::string& path);
	private:
		static std::string s_ProjectFile;
		static std::string s_Directory;
		static std::string s_Name;
		static std::string s_GameName;
		static std::string s_ActiveMapFile;
		static std::string s_StartingMap;
		static std::string s_MapScriptPath;
	};
}
