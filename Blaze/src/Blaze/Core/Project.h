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

		struct BuildGameOptions
		{
			std::string outputExePath;              // empty + promptForOutputPath → save dialog
			std::string startingMap;                // empty → project starting map
			std::string gameName;                   // empty → project game name
			std::vector<std::string> includedMaps;  // empty → keep all maps in content/
			bool promptForOutputPath = false;
		};
		static bool BuildGame(const BuildGameOptions& options);

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

		struct GameObjectWriteFlags
		{
			bool objectMarkers = true; // "object" / "end"
			bool name = true;
			bool type = true;
			bool transform = true; // pos / rot / scale
			bool identity = true;  // layer / visible / color / tags
			bool parent = true;
			bool camera = true;
			bool cameraDrives = true;
			bool light = true;
			bool texture = true;
			bool flipbook = true;
			bool anim = true;
			bool tilemap = true;
			bool tilemapTiles = true;
			bool text = true;
			bool collision = true;
			bool audio = true;
			bool particle = true;
			bool script = true;

			static GameObjectWriteFlags All();
			static GameObjectWriteFlags ComponentPreset();
		};

		// Shared GameObject block I/O (maps + prefabs + component presets).
		// parentIndex < 0 omits the parent field (also gated by flags.parent).
		static void WriteGameObject(std::ostream& out, class GameObject* object, int parentIndex = -1,
			const GameObjectWriteFlags& flags = GameObjectWriteFlags::All());
		static bool ApplyGameObjectProperty(class GameObject* object, const std::string& line, int* outParentIndex = nullptr);
		static std::string StoreTexturePath(const std::string& texturePath);
		static std::string MakeProjectRelative(const std::string& path);
	private:
		static bool SetupProjectAt(const std::string& pickedFile);
		static bool WriteSceneFile(const std::string& path, Renderer& renderer, ObjectWindow* objectsWindow, bool asMap);
		static bool ReadSceneFile(const std::string& path, Renderer& renderer, const std::vector<float>& vertices, const std::vector<unsigned int>& indices, ObjectWindow* objectsWindow);
		static bool WriteProjectPointer();
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
