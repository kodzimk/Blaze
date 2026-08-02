#include "Project.h"
#include "Blaze/Renderer/Renderer.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Input/InputActionsAsset.h"
#include "Blaze/Localization/LocalizationAsset.h"
#include "Blaze/ImGui/ObjectWindow.h"
#include "Blaze/UI/UISystem.h"
#include "Blaze/Logging/Log.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdio>
#include <vector>

#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#pragma comment(lib, "Comdlg32.lib")

namespace fs = std::filesystem;

namespace Blaze
{
	std::string Project::s_ProjectFile;
	std::string Project::s_Directory;
	std::string Project::s_Name;
	std::string Project::s_GameName;
	std::string Project::s_ActiveMapFile;
	std::string Project::s_StartingMap;
	std::string Project::s_MapScriptPath;

	static const char* s_ProjectFilter = "Blaze Project (*.blaze)\0*.blaze\0All Files (*.*)\0*.*\0";
	static const char* s_ContentFolder = "content";
	static std::string s_OriginalCwd;
	static std::string s_RecentProjectsPath;

	static void EnsureOriginalCwd()
	{
		if (!s_OriginalCwd.empty())
			return;
		std::error_code ec;
		s_OriginalCwd = fs::current_path(ec).string();
		s_RecentProjectsPath = (fs::path(s_OriginalCwd) / "blaze_projects.txt").string();
	}

	static void EnterProjectDirectory()
	{
		EnsureOriginalCwd();
		if (Project::GetDirectory().empty())
			return;
		std::error_code ec;
		fs::current_path(Project::GetDirectory(), ec);
	}

	static void LeaveProjectDirectory()
	{
		EnsureOriginalCwd();
		std::error_code ec;
		fs::current_path(s_OriginalCwd, ec);
	}

	bool Project::HasProject() { return !s_ProjectFile.empty(); }

	void Project::CloseProject()
	{
		s_ProjectFile.clear();
		s_Directory.clear();
		s_Name.clear();
		s_GameName.clear();
		s_ActiveMapFile.clear();
		s_StartingMap.clear();
		s_MapScriptPath.clear();
		LeaveProjectDirectory();
	}

	const std::string& Project::GetProjectFile() { return s_ProjectFile; }
	const std::string& Project::GetDirectory() { return s_Directory; }
	const std::string& Project::GetName() { return s_Name; }
	const std::string& Project::GetGameName() { return s_GameName.empty() ? s_Name : s_GameName; }
	const std::string& Project::GetActiveMap() { return s_ActiveMapFile; }
	const std::string& Project::GetStartingMap() { return s_StartingMap; }
	const std::string& Project::GetMapScript() { return s_MapScriptPath; }

	void Project::SetGameName(const std::string& name)
	{
		s_GameName = name;
	}

	void Project::SetActiveMap(const std::string& mapPath)
	{
		s_ActiveMapFile = mapPath;
	}

	void Project::SetStartingMap(const std::string& mapPath)
	{
		s_StartingMap = mapPath;
	}

	void Project::SetMapScript(const std::string& scriptPath)
	{
		s_MapScriptPath = scriptPath;
	}

	void Project::ClearMapScript()
	{
		s_MapScriptPath.clear();
	}

	std::string Project::CreateMapScriptForMap(const std::string& mapPath)
	{
		if (mapPath.empty())
			return {};

		const std::string mapName = fs::path(mapPath).stem().string();
		fs::path scriptsDir;
		std::error_code ec;
		if (!s_Directory.empty())
			scriptsDir = fs::path(s_Directory) / s_ContentFolder / "scripts" / "maps";
		else
			scriptsDir = fs::path(s_ContentFolder) / "scripts" / "maps";

		fs::create_directories(scriptsDir, ec);

		fs::path scriptPath = scriptsDir / (mapName + "_Map.lua");
		int suffix = 1;
		while (fs::exists(scriptPath, ec))
		{
			scriptPath = scriptsDir / (mapName + "_Map_" + std::to_string(suffix) + ".lua");
			++suffix;
		}

		std::ofstream out(scriptPath);
		if (!out.is_open())
		{
			BZ_INFO("Failed to create map script: {0}", scriptPath.string());
			return {};
		}

		out << "-- Map script (Level Blueprint) for " << mapName << "\n";
		out << "-- Runs for the whole map. Use FindByTag / Spawn like object scripts.\n\n";
		out << "function OnStart()\n";
		out << "\tprint(\"Map started:\", mapName)\n";
		out << "end\n\n";
		out << "function OnUpdate(dt)\n";
		out << "end\n";
		out.close();

		std::string relative = MakeProjectRelative(scriptPath.string());
		if (relative.empty())
			relative = (fs::path(s_ContentFolder) / "scripts" / "maps" / scriptPath.filename()).generic_string();

		BZ_INFO("Created map script: {0}", relative);
		return relative;
	}

	std::string Project::EnsureMapScript()
	{
		std::error_code ec;
		if (!s_MapScriptPath.empty() && fs::exists(s_MapScriptPath, ec))
			return s_MapScriptPath;

		if (!s_MapScriptPath.empty() && !s_Directory.empty())
		{
			fs::path full = fs::path(s_Directory) / s_MapScriptPath;
			if (fs::exists(full, ec))
			{
				s_MapScriptPath = full.generic_string();
				return s_MapScriptPath;
			}
		}

		if (s_ActiveMapFile.empty())
			return {};

		s_MapScriptPath = CreateMapScriptForMap(s_ActiveMapFile);
		return s_MapScriptPath;
	}

	std::string Project::GetDefaultMapPath()
	{
		return (fs::path(s_ContentFolder) / "maps" / "Main.map").generic_string();
	}

	std::string Project::GetContentDirectory()
	{
		return s_ContentFolder;
	}

	std::string Project::MakeProjectRelative(const std::string& path)
	{
		if (path.empty() || s_Directory.empty())
			return path;

		std::error_code ec;
		fs::path absPath = fs::absolute(path, ec);
		fs::path projectDir = fs::absolute(s_Directory, ec);
		fs::path rel = fs::relative(absPath, projectDir, ec);
		if (!ec && !rel.empty() && rel.generic_string().rfind("..", 0) != 0)
			return rel.generic_string();
		return path;
	}

	std::vector<std::string> Project::ListMapFiles()
	{
		std::vector<std::string> maps;
		if (s_Directory.empty())
			return maps;

		std::error_code ec;
		fs::path root = fs::path(s_Directory) / s_ContentFolder;
		if (!fs::exists(root, ec))
			return maps;

		for (auto it = fs::recursive_directory_iterator(root, ec); it != fs::recursive_directory_iterator(); it.increment(ec))
		{
			if (ec)
				break;
			if (!it->is_regular_file(ec))
				continue;
			if (it->path().extension() != ".map")
				continue;

			fs::path rel = fs::relative(it->path(), s_Directory, ec);
			if (!ec && !rel.empty())
				maps.push_back(rel.generic_string());
			else
				maps.push_back(it->path().generic_string());
		}

		std::sort(maps.begin(), maps.end());
		return maps;
	}

	std::string Project::ResolveMapPath(const std::string& mapNameOrPath)
	{
		if (mapNameOrPath.empty())
			return {};

		std::error_code ec;
		fs::path input(mapNameOrPath);

		auto existsResolved = [&](const fs::path& candidate) -> std::string
		{
			if (fs::exists(candidate, ec))
				return fs::absolute(candidate, ec).string();
			if (!s_Directory.empty())
			{
				fs::path underProject = fs::path(s_Directory) / candidate;
				if (fs::exists(underProject, ec))
					return fs::absolute(underProject, ec).string();
			}
			return {};
		};

		if (std::string found = existsResolved(input); !found.empty())
			return found;

		std::string stem = input.stem().string();
		if (stem.empty())
			stem = mapNameOrPath;
		std::string withExt = stem;
		if (input.extension() != ".map")
			withExt = stem + ".map";

		const fs::path candidates[] = {
			fs::path(s_ContentFolder) / "maps" / withExt,
			fs::path(s_ContentFolder) / withExt,
			fs::path("maps") / withExt,
			fs::path(withExt)
		};
		for (const fs::path& candidate : candidates)
		{
			if (std::string found = existsResolved(candidate); !found.empty())
				return found;
		}

		for (const std::string& listed : ListMapFiles())
		{
			fs::path listedPath(listed);
			if (listedPath.stem() == stem || listedPath.filename() == withExt || listedPath.filename() == input.filename())
			{
				if (std::string found = existsResolved(listedPath); !found.empty())
					return found;
			}
		}

		return {};
	}

	bool Project::SaveProjectSettings()
	{
		return WriteProjectPointer();
	}

	static bool CopyPathRecursive(const fs::path& from, const fs::path& to)
	{
		std::error_code ec;
		if (!fs::exists(from, ec))
			return false;

		fs::create_directories(to.parent_path(), ec);
		if (fs::is_directory(from, ec))
		{
			fs::create_directories(to, ec);
			for (auto it = fs::recursive_directory_iterator(from, ec); it != fs::recursive_directory_iterator(); it.increment(ec))
			{
				if (ec)
					return false;
				const fs::path rel = fs::relative(it->path(), from, ec);
				const fs::path dest = to / rel;
				if (it->is_directory(ec))
					fs::create_directories(dest, ec);
				else if (it->is_regular_file(ec))
				{
					fs::create_directories(dest.parent_path(), ec);
					fs::copy_file(it->path(), dest, fs::copy_options::overwrite_existing, ec);
					if (ec)
						return false;
				}
			}
			return true;
		}

		fs::copy_file(from, to, fs::copy_options::overwrite_existing, ec);
		return !ec;
	}

	static fs::path FindRuntimeExecutable()
	{
		char modulePath[MAX_PATH] = {};
		if (!GetModuleFileNameA(nullptr, modulePath, MAX_PATH))
			return {};

		const fs::path sandboxExe = modulePath;
		const fs::path sandboxDir = sandboxExe.parent_path();
		const fs::path configDir = sandboxDir.parent_path();

		const fs::path candidates[] = {
			configDir / "Runtime" / "Runtime.exe",
			sandboxDir / "Runtime.exe",
			sandboxDir / "BlazeRuntime.exe"
		};

		std::error_code ec;
		for (const fs::path& path : candidates)
		{
			if (fs::exists(path, ec))
				return path;
		}
		return {};
	}

	bool Project::BuildGame()
	{
		BuildGameOptions options;
		options.promptForOutputPath = true;
		return BuildGame(options);
	}

	bool Project::BuildGame(const BuildGameOptions& options)
	{
		if (!HasProject())
		{
			BZ_INFO("Build failed: no project open");
			return false;
		}

		const fs::path runtimeExe = FindRuntimeExecutable();
		if (runtimeExe.empty())
		{
			BZ_INFO("Build failed: Runtime.exe not found. Build the Runtime project first.");
			return false;
		}

		std::string gameName = options.gameName.empty() ? GetGameName() : options.gameName;
		if (gameName.empty())
			gameName = s_Name;
		for (char& c : gameName)
		{
			if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*')
				c = '_';
		}

		fs::path outExe;
		if (!options.outputExePath.empty())
		{
			outExe = options.outputExePath;
			if (outExe.extension().empty())
				outExe += ".exe";
		}
		else if (options.promptForOutputPath)
		{
			char filePath[MAX_PATH] = {};
			std::snprintf(filePath, MAX_PATH, "%s.exe", gameName.c_str());

			OPENFILENAMEA ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.lpstrFile = filePath;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrFilter = "Executable (*.exe)\0*.exe\0All Files (*.*)\0*.*\0";
			ofn.nFilterIndex = 1;
			ofn.lpstrDefExt = "exe";
			ofn.lpstrTitle = "Build game executable";
			ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

			if (!GetSaveFileNameA(&ofn))
				return false;
			outExe = filePath;
		}
		else
		{
			BZ_INFO("Build failed: no output path");
			return false;
		}

		const fs::path outDir = outExe.parent_path();
		std::error_code ec;
		fs::create_directories(outDir, ec);

		if (!CopyPathRecursive(runtimeExe, outExe))
		{
			BZ_INFO("Build failed: could not copy Runtime.exe to {0}", outExe.string());
			return false;
		}

		char modulePath[MAX_PATH] = {};
		GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
		fs::path blazeDll = fs::path(modulePath).parent_path() / "Blaze.dll";
		if (!fs::exists(blazeDll, ec))
			blazeDll = runtimeExe.parent_path() / "Blaze.dll";
		if (!fs::exists(blazeDll, ec))
			blazeDll = runtimeExe.parent_path().parent_path() / "Blaze" / "Blaze.dll";

		if (!CopyPathRecursive(blazeDll, outDir / "Blaze.dll"))
		{
			BZ_INFO("Build failed: Blaze.dll not found near the editor/runtime");
			return false;
		}

		const fs::path contentSrc = fs::path(s_Directory) / s_ContentFolder;
		if (fs::exists(contentSrc, ec))
		{
			if (!CopyPathRecursive(contentSrc, outDir / s_ContentFolder))
			{
				BZ_INFO("Build failed: could not copy content folder");
				return false;
			}
		}
		else
		{
			BZ_INFO("Build warning: content folder missing at {0}", contentSrc.string());
		}

		std::string starting = options.startingMap.empty()
			? (s_StartingMap.empty() ? GetDefaultMapPath() : MakeProjectRelative(s_StartingMap))
			: MakeProjectRelative(options.startingMap);
		{
			fs::path startAbs = fs::path(s_Directory) / starting;
			if (!fs::exists(startAbs, ec) && !options.startingMap.empty())
				startAbs = options.startingMap;
			if (!fs::exists(startAbs, ec) && !s_StartingMap.empty())
				startAbs = s_StartingMap;
			if (fs::exists(startAbs, ec))
			{
				const fs::path destMap = outDir / MakeProjectRelative(startAbs.string());
				fs::create_directories(destMap.parent_path(), ec);
				CopyPathRecursive(startAbs, destMap);
			}
		}

		// Optionally strip maps not listed in includedMaps (assets stay).
		if (!options.includedMaps.empty())
		{
			auto mapIncluded = [&](const fs::path& mapPathInBuild) -> bool
			{
				const std::string fileName = mapPathInBuild.filename().string();
				std::string relFromContent;
				{
					std::error_code ecRel;
					fs::path contentOutPath = outDir / s_ContentFolder;
					fs::path rel = fs::relative(mapPathInBuild, contentOutPath, ecRel);
					if (!ecRel)
						relFromContent = (fs::path(s_ContentFolder) / rel).generic_string();
				}

				for (const std::string& included : options.includedMaps)
				{
					if (included.empty())
						continue;
					if (fs::path(included).filename() == fileName)
						return true;
					if (!relFromContent.empty() && included == relFromContent)
						return true;
					if (included == fileName)
						return true;
				}
				return false;
			};

			const fs::path contentOutStrip = outDir / s_ContentFolder;
			if (fs::exists(contentOutStrip, ec))
			{
				std::vector<fs::path> toRemove;
				for (auto it = fs::recursive_directory_iterator(contentOutStrip, ec); it != fs::recursive_directory_iterator(); ++it)
				{
					if (ec || !it->is_regular_file())
						continue;
					std::string ext = it->path().extension().string();
					for (char& c : ext)
						c = (char)std::tolower((unsigned char)c);
					if (ext != ".map")
						continue;
					if (!mapIncluded(it->path()))
						toRemove.push_back(it->path());
				}
				for (const fs::path& path : toRemove)
					fs::remove(path, ec);
			}
		}

		int widgetCount = 0;
		int scriptCount = 0;
		int mapCount = 0;
		const fs::path contentOut = outDir / s_ContentFolder;
		if (fs::exists(contentOut, ec))
		{
			for (auto it = fs::recursive_directory_iterator(contentOut, ec); it != fs::recursive_directory_iterator(); ++it)
			{
				if (ec || !it->is_regular_file())
					continue;
				std::string ext = it->path().extension().string();
				for (char& c : ext)
					c = (char)std::tolower((unsigned char)c);
				if (ext == ".widget")
					++widgetCount;
				else if (ext == ".lua")
					++scriptCount;
				else if (ext == ".map")
					++mapCount;
			}
		}

		std::ofstream gameProject(outDir / "game.blaze", std::ios::trunc);
		if (!gameProject.is_open())
		{
			BZ_INFO("Build failed: could not write game.blaze");
			return false;
		}

		gameProject << "BlazeProject 1\n";
		gameProject << "name " << gameName << "\n";
		gameProject << "startingMap " << starting << "\n";
		gameProject << "map " << starting << "\n";
		gameProject.close();

		BZ_INFO("Game built to: {0}", outDir.string());
		BZ_INFO("Executable: {0}", outExe.string());
		BZ_INFO("Packaged: {0} maps, {1} scripts, {2} widgets (+ textures/audio in content/)",
			mapCount, scriptCount, widgetCount);
		return true;
	}

	std::vector<std::string> Project::GetRecentProjects()
	{
		EnsureOriginalCwd();
		std::vector<std::string> projects;
		std::ifstream in(s_RecentProjectsPath);
		if (!in.is_open())
			return projects;

		std::string line;
		while (std::getline(in, line))
		{
			while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
				line.pop_back();
			if (line.empty())
				continue;

			std::error_code ec;
			if (fs::exists(line, ec))
				projects.push_back(line);
		}
		return projects;
	}

	void Project::RegisterRecentProject(const std::string& projectFile)
	{
		EnsureOriginalCwd();
		std::vector<std::string> projects = GetRecentProjects();
		projects.erase(std::remove(projects.begin(), projects.end(), projectFile), projects.end());
		projects.insert(projects.begin(), projectFile);
		if (projects.size() > 10)
			projects.resize(10);

		std::ofstream out(s_RecentProjectsPath, std::ios::trunc);
		for (const std::string& path : projects)
			out << path << "\n";
	}

	void Project::SetProjectFromFile(const std::string& projectFile)
	{
		fs::path file(projectFile);
		s_ProjectFile = file.string();
		s_Directory = file.parent_path().string();
		s_Name = file.stem().string();
		s_GameName = s_Name;
		s_StartingMap.clear();
		s_ActiveMapFile.clear();
		s_MapScriptPath.clear();
		RegisterRecentProject(s_ProjectFile);
		EnterProjectDirectory();
	}

	static bool PickProjectLocation(std::string& outPath, const char* title)
	{
		char filePath[MAX_PATH] = {};

		OPENFILENAMEA ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFile = filePath;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter = s_ProjectFilter;
		ofn.nFilterIndex = 1;
		ofn.lpstrDefExt = "blaze";
		ofn.lpstrTitle = title;
		ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

		if (!GetSaveFileNameA(&ofn))
			return false;

		outPath = filePath;
		return true;
	}

	bool Project::SetupProjectAt(const std::string& pickedFile)
	{
		fs::path picked(pickedFile);
		std::string name = picked.stem().string();
		fs::path projectDir = picked.parent_path() / name;

		std::error_code ec;
		fs::create_directories(projectDir / s_ContentFolder, ec);
		if (ec)
		{
			BZ_INFO("Failed to create project folder: {0}", projectDir.string());
			return false;
		}

		s_Directory = projectDir.string();
		s_Name = name;
		s_GameName = name;
		s_ProjectFile = (projectDir / (name + ".blaze")).string();
		fs::create_directories(projectDir / s_ContentFolder / "maps", ec);
		fs::create_directories(projectDir / s_ContentFolder / "input", ec);
		fs::create_directories(projectDir / s_ContentFolder / "prefabs", ec);
		fs::create_directories(projectDir / s_ContentFolder / "presets", ec);
		fs::create_directories(projectDir / s_ContentFolder / "localization", ec);
		{
			fs::path inputPath = projectDir / s_ContentFolder / "input" / "Default.inputactions";
			if (!fs::exists(inputPath, ec))
			{
				InputActionsAsset asset = InputActionsAsset::CreateDefault("Default");
				asset.SetPath(inputPath.string());
				asset.SaveToFile(inputPath.string());
			}
		}
		{
			fs::path locPath = projectDir / s_ContentFolder / "localization" / "en.loc";
			if (!fs::exists(locPath, ec))
			{
				LocalizationAsset asset = LocalizationAsset::CreateDefault("en");
				asset.SetPath(locPath.string());
				asset.SaveToFile(locPath.string());
			}
		}
		s_StartingMap = GetDefaultMapPath();
		s_ActiveMapFile = (projectDir / s_StartingMap).string();
		s_MapScriptPath = CreateMapScriptForMap(s_ActiveMapFile);
		RegisterRecentProject(s_ProjectFile);
		EnterProjectDirectory();
		return true;
	}

	bool Project::NewProject()
	{
		std::string picked;
		if (!PickProjectLocation(picked, "Choose where to create the new project"))
			return false;
		if (!SetupProjectAt(picked))
			return false;

		BZ_INFO("Created project '{0}' at {1}", s_Name, s_Directory);
		return true;
	}

	bool Project::OpenProjectDialog()
	{
		char filePath[MAX_PATH] = {};

		OPENFILENAMEA ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFile = filePath;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter = s_ProjectFilter;
		ofn.nFilterIndex = 1;
		ofn.lpstrTitle = "Open Blaze project";
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (!GetOpenFileNameA(&ofn))
			return false;

		SetProjectFromFile(filePath);
		BZ_INFO("Opened project '{0}' at {1}", s_Name, s_Directory);
		return true;
	}

	std::string Project::StoreTexturePath(const std::string& texturePath)
	{
		std::error_code ec;
		fs::path source = fs::absolute(texturePath, ec);
		fs::path projectDir = fs::absolute(s_Directory, ec);

		fs::path rel = fs::relative(source, projectDir, ec);
		if (!ec && !rel.empty() && rel.generic_string().rfind("..", 0) != 0)
			return rel.generic_string();

		fs::path target = projectDir / s_ContentFolder / source.filename();
		fs::create_directories(target.parent_path(), ec);
		fs::copy_file(source, target, fs::copy_options::overwrite_existing, ec);
		if (ec)
		{
			BZ_INFO("Could not copy '{0}' into the project, keeping the original path", texturePath);
			return texturePath;
		}
		return (fs::path(s_ContentFolder) / source.filename()).generic_string();
	}

	bool Project::WriteProjectPointer()
	{
		if (s_ProjectFile.empty())
			return false;

		std::ofstream out(s_ProjectFile, std::ios::trunc);
		if (!out.is_open())
		{
			BZ_INFO("Could not write project file: {0}", s_ProjectFile);
			return false;
		}

		out << "BlazeProject 1\n";
		out << "name " << (s_GameName.empty() ? s_Name : s_GameName) << "\n";

		std::string starting = s_StartingMap.empty() ? GetDefaultMapPath() : s_StartingMap;
		out << "startingMap " << MakeProjectRelative(starting) << "\n";

		if (!s_ActiveMapFile.empty())
			out << "map " << MakeProjectRelative(s_ActiveMapFile) << "\n";
		else if (!starting.empty())
			out << "map " << MakeProjectRelative(starting) << "\n";

		return true;
	}

	Project::GameObjectWriteFlags Project::GameObjectWriteFlags::All()
	{
		return GameObjectWriteFlags{};
	}

	Project::GameObjectWriteFlags Project::GameObjectWriteFlags::ComponentPreset()
	{
		GameObjectWriteFlags flags;
		flags.name = false;
		flags.type = false;
		flags.transform = false;
		flags.parent = false;
		flags.cameraDrives = false;
		flags.tilemapTiles = false;
		return flags;
	}

	void Project::WriteGameObject(std::ostream& out, GameObject* object, int parentIndex, const GameObjectWriteFlags& flags)
	{
		if (!object)
			return;

		if (flags.objectMarkers)
			out << "object\n";

		if (flags.name)
			out << "name " << object->GetName() << "\n";
		if (flags.type)
			out << "type " << (int)object->GetType() << "\n";

		if (flags.transform)
		{
			glm::vec3 pos = object->GetLocalPosition();
			out << "pos " << pos.x << " " << pos.y << " " << pos.z << "\n";
			out << "rot " << object->GetLocalRotation() << "\n";

			glm::vec2 scale = object->GetLocalScale();
			out << "scale " << scale.x << " " << scale.y << "\n";
		}

		if (flags.identity)
		{
			out << "layer " << object->GetLayer() << "\n";
			out << "visible " << (object->IsVisible() ? 1 : 0) << "\n";
			{
				glm::vec4 color = object->GetColor();
				out << "color " << color.r << " " << color.g << " " << color.b << " " << color.a << "\n";
			}
			if (!object->GetTags().empty())
				out << "tags " << object->GetTagsString() << "\n";
		}

		if (flags.camera && object->IsCamera())
		{
			out << "zoom " << object->GetZoom() << "\n";
			if (flags.cameraDrives)
				out << "drives " << (object->DrivesView() ? 1 : 0) << "\n";
		}

		if (flags.light && object->IsLight())
		{
			out << "lightradius " << object->GetLightRadius() << "\n";
			out << "lightintensity " << object->GetLightIntensity() << "\n";
			out << "lightenabled " << (object->IsLightEnabled() ? 1 : 0) << "\n";
			out << "lightkind " << (object->GetLightKind() == LightKind::Spot ? "spot" : "point") << "\n";
			if (object->GetLightKind() == LightKind::Spot)
			{
				out << "lightinner " << object->GetLightInnerAngle() << "\n";
				out << "lightouter " << object->GetLightOuterAngle() << "\n";
			}
		}

		if (flags.postprocess && object->IsPostProcess())
		{
			out << "ppenabled " << (object->IsPostProcessEnabled() ? 1 : 0) << "\n";
			out << "ppbrightness " << object->GetPostProcessBrightness() << "\n";
			out << "ppcontrast " << object->GetPostProcessContrast() << "\n";
			out << "ppsaturation " << object->GetPostProcessSaturation() << "\n";
			out << "ppvignette " << object->GetPostProcessVignette() << "\n";
			const glm::vec3 tint = object->GetPostProcessTint();
			out << "pptint " << tint.r << " " << tint.g << " " << tint.b << "\n";
		}

		if (flags.texture && !object->GetTexturePath().empty())
			out << "texture " << StoreTexturePath(object->GetTexturePath()) << "\n";

		if (flags.flipbook)
		{
			if (object->GetFlipbookColumns() != 1 || object->GetFlipbookRows() != 1 ||
				object->GetFlipbookFrameCount() != 0 || object->IsFlipbookPlaying() ||
				object->GetFlipbookFrame() != 0 || !object->GetFlipbookLoop() ||
				object->GetFlipbookFps() < 11.999f || object->GetFlipbookFps() > 12.001f ||
				object->GetFlipbookClipCount() > 0 || object->GetActiveFlipbookClipIndex() >= 0)
			{
				out << "flipbook " << object->GetFlipbookColumns()
					<< " " << object->GetFlipbookRows()
					<< " " << object->GetFlipbookFrameCount()
					<< " " << object->GetFlipbookFps()
					<< " " << (object->IsFlipbookPlaying() ? 1 : 0)
					<< " " << (object->GetFlipbookLoop() ? 1 : 0)
					<< " " << object->GetFlipbookFrame() << "\n";
			}

			for (const FlipbookClip& clip : object->GetFlipbookClips())
			{
				if (object->HasFlipbookAsset())
					break;
				out << "flipclip " << clip.name
					<< " " << clip.startFrame
					<< " " << clip.endFrame
					<< " " << clip.fps
					<< " " << (clip.loop ? 1 : 0) << "\n";
			}
			if (object->HasFlipbookAsset())
				out << "flipbookasset " << StoreTexturePath(object->GetFlipbookAssetPath()) << "\n";
			if (object->GetActiveFlipbookClipIndex() >= 0)
			{
				const std::string active = object->GetActiveFlipbookClipName();
				if (!active.empty())
					out << "flipclipactive " << active << "\n";
			}
		}

		if (flags.anim && object->HasAnimAsset())
		{
			out << "animasset " << StoreTexturePath(object->GetAnimAssetPath()) << "\n";
			out << "animenabled " << (object->IsAnimEnabled() ? 1 : 0) << "\n";
		}

		if (flags.tilemap && object->IsTilemap())
		{
			out << "tilemap " << object->GetTilemapColumns()
				<< " " << object->GetTilemapRows()
				<< " " << object->GetTileCellSize() << "\n";
			out << "tilesetgrid " << object->GetTilesetColumns()
				<< " " << object->GetTilesetRows() << "\n";
			out << "tilecollision " << (object->IsTileCollisionEnabled() ? 1 : 0) << "\n";
			out << "tilesetsolid";
			for (uint8_t flag : object->GetTilesetSolid())
				out << " " << (int)flag;
			out << "\n";
			out << "tilelayers " << object->GetTileLayerCount() << "\n";
			for (int li = 0; li < object->GetTileLayerCount(); ++li)
			{
				const TilemapLayer* layer = object->GetTileLayer(li);
				if (!layer)
					continue;
				out << "tilelayer " << (layer->visible ? 1 : 0)
					<< " " << (layer->collision ? 1 : 0)
					<< " " << layer->opacity
					<< " " << layer->name << "\n";
				if (flags.tilemapTiles)
				{
					out << "tiles";
					for (int tile : layer->tiles)
						out << " " << tile;
					out << "\n";
				}
			}
			out << "tilelayeractive " << object->GetActiveTileLayer() << "\n";
		}

		if (flags.text && object->IsText())
		{
			out << "text " << object->GetText() << "\n";
			out << "fontsize " << object->GetFontSize() << "\n";
			if (!object->GetFontPath().empty())
				out << "fontpath " << StoreTexturePath(object->GetFontPath()) << "\n";
		}

		if (flags.collision && object->HasCollision())
		{
			out << "collision " << (int)object->GetCollisionType() << "\n";
			out << "collidershape " << (int)object->GetColliderShape() << "\n";
			out << "collisionradius " << object->GetCollisionRadius() << "\n";
			glm::vec2 csize = object->GetCollisionSize();
			out << "collisionsize " << csize.x << " " << csize.y << "\n";
			out << "rbmass " << object->GetRigidbodyMass() << "\n";
			out << "rbgravity " << object->GetRigidbodyGravityScale() << "\n";
			out << "rbkinematic " << (object->IsRigidbodyKinematic() ? 1 : 0) << "\n";
			out << "trigger " << (object->IsTrigger() ? 1 : 0) << "\n";
			out << "collisionlayer " << object->GetCollisionLayer() << "\n";
			out << "collisionmask " << object->GetCollisionMask() << "\n";
			if (object->GetColliderShape() == ColliderShape::Polygon)
			{
				const auto& verts = object->GetColliderVertices();
				out << "colliderverts " << verts.size();
				for (const glm::vec2& v : verts)
					out << " " << v.x << " " << v.y;
				out << "\n";
			}
		}

		if (flags.audio && object->HasAudio())
		{
			out << "audio 1\n";
			if (!object->GetAudioPath().empty())
				out << "audiopath " << StoreTexturePath(object->GetAudioPath()) << "\n";
			out << "audiovolume " << object->GetAudioVolume() << "\n";
			out << "audioloop " << (object->GetAudioLoop() ? 1 : 0) << "\n";
			out << "audioplayonstart " << (object->GetAudioPlayOnStart() ? 1 : 0) << "\n";
		}

		if (flags.particle && object->HasParticleEmitter())
		{
			out << "particle 1\n";
			if (!object->GetParticlePath().empty())
				out << "particlepath " << StoreTexturePath(object->GetParticlePath()) << "\n";
			out << "particleplayonstart " << (object->GetParticlePlayOnStart() ? 1 : 0) << "\n";
		}

		if (flags.navagent && object->HasNavAgent())
		{
			out << "navagent 1\n";
			out << "navspeed " << object->GetNavSpeed() << "\n";
			out << "navdiagonal " << (object->GetNavAllowDiagonal() ? 1 : 0) << "\n";
			out << "navstopping " << object->GetNavStoppingDistance() << "\n";
			out << "navdebug " << (object->GetNavDebugDraw() ? 1 : 0) << "\n";
		}

		if (flags.script && object->HasScript())
		{
			out << "script 1\n";
			if (!object->GetScriptPath().empty())
				out << "scriptpath " << StoreTexturePath(object->GetScriptPath()) << "\n";
		}

		if (flags.parent && parentIndex >= 0)
			out << "parent " << parentIndex << "\n";

		if (flags.objectMarkers)
			out << "end\n";
	}

	bool Project::WriteSceneFile(const std::string& path, Renderer& renderer, ObjectWindow* objectsWindow, bool asMap)
	{
		if (path.empty())
			return false;

		std::error_code ec;
		fs::create_directories(fs::path(path).parent_path(), ec);

		std::ofstream out(path, std::ios::trunc);
		if (!out.is_open())
		{
			BZ_INFO("Could not write scene file: {0}", path);
			return false;
		}

		out << (asMap ? "BlazeMap 1\n" : "BlazeProject 1\n");
		if (asMap && !s_MapScriptPath.empty())
			out << "mapscript " << MakeProjectRelative(s_MapScriptPath) << "\n";
		for (GameObject* object : renderer.GetObjects())
		{
			if (!object)
				continue;

			int parentIndex = -1;
			if (GameObject* parent = object->GetParent())
			{
				const auto& objs = renderer.GetObjects();
				for (size_t i = 0; i < objs.size(); ++i)
				{
					if (objs[i] == parent)
					{
						parentIndex = (int)i;
						break;
					}
				}
			}
			WriteGameObject(out, object, parentIndex);
		}

		if (objectsWindow)
		{
			objectsWindow->SetObjects(renderer.GetObjects());
			objectsWindow->SaveHierarchy(out);
		}

		BZ_INFO("Scene saved: {0}", path);
		return true;
	}

	bool Project::ApplyGameObjectProperty(GameObject* current, const std::string& line, int* outParentIndex)
	{
		if (!current)
			return false;

		std::istringstream stream(line);
		std::string key;
		stream >> key;

		if (key == "name" && line.size() > 5)
		{
			current->SetName(line.substr(5));
		}
		else if (key == "type")
		{
			int type = 0;
			stream >> type;
			current->SetType((ObjectType)type);
		}
		else if (key == "pos")
		{
			glm::vec3 pos(0.0f);
			stream >> pos.x >> pos.y >> pos.z;
			current->SetLocalPosition(pos);
		}
		else if (key == "rot")
		{
			float rotation = 0.0f;
			stream >> rotation;
			current->SetLocalRotation(rotation);
		}
		else if (key == "scale")
		{
			glm::vec2 scale(1.0f);
			stream >> scale.x >> scale.y;
			current->SetLocalScale(scale);
		}
		else if (key == "layer")
		{
			int layer = 0;
			stream >> layer;
			current->SetLayer(layer);
		}
		else if (key == "visible")
		{
			int visible = 1;
			stream >> visible;
			current->SetVisible(visible != 0);
		}
		else if (key == "color")
		{
			glm::vec4 color(1.0f);
			stream >> color.r >> color.g >> color.b >> color.a;
			current->SetColor(color);
		}
		else if (key == "tags" && line.size() > 5)
		{
			current->SetTagsFromString(line.substr(5));
		}
		else if (key == "tag" && line.size() > 4)
		{
			current->SetTag(line.substr(4));
		}
		else if (key == "zoom")
		{
			float zoom = 1.0f;
			stream >> zoom;
			current->SetZoom(zoom);
		}
		else if (key == "drives")
		{
			int drives = 0;
			stream >> drives;
			current->SetDrivesView(drives != 0);
		}
		else if (key == "lightradius")
		{
			float radius = 300.0f;
			stream >> radius;
			if (current->GetType() != ObjectType::Light)
				current->SetType(ObjectType::Light);
			current->SetLightRadius(radius);
		}
		else if (key == "lightintensity")
		{
			float intensity = 1.0f;
			stream >> intensity;
			if (current->GetType() != ObjectType::Light)
				current->SetType(ObjectType::Light);
			current->SetLightIntensity(intensity);
		}
		else if (key == "lightenabled")
		{
			int enabled = 1;
			stream >> enabled;
			if (current->GetType() != ObjectType::Light)
				current->SetType(ObjectType::Light);
			current->SetLightEnabled(enabled != 0);
		}
		else if (key == "lightkind")
		{
			std::string kind;
			stream >> kind;
			if (current->GetType() != ObjectType::Light)
				current->SetType(ObjectType::Light);
			if (kind == "spot" || kind == "1")
				current->SetLightKind(LightKind::Spot);
			else
				current->SetLightKind(LightKind::Point);
		}
		else if (key == "lightinner")
		{
			float angle = 20.0f;
			stream >> angle;
			if (current->GetType() != ObjectType::Light)
				current->SetType(ObjectType::Light);
			current->SetLightInnerAngle(angle);
		}
		else if (key == "lightouter")
		{
			float angle = 35.0f;
			stream >> angle;
			if (current->GetType() != ObjectType::Light)
				current->SetType(ObjectType::Light);
			current->SetLightOuterAngle(angle);
		}
		else if (key == "ppenabled")
		{
			int enabled = 1;
			stream >> enabled;
			if (current->GetType() != ObjectType::PostProcess)
				current->SetType(ObjectType::PostProcess);
			current->SetPostProcessEnabled(enabled != 0);
		}
		else if (key == "ppbrightness")
		{
			float v = 0.0f;
			stream >> v;
			if (current->GetType() != ObjectType::PostProcess)
				current->SetType(ObjectType::PostProcess);
			current->SetPostProcessBrightness(v);
		}
		else if (key == "ppcontrast")
		{
			float v = 1.0f;
			stream >> v;
			if (current->GetType() != ObjectType::PostProcess)
				current->SetType(ObjectType::PostProcess);
			current->SetPostProcessContrast(v);
		}
		else if (key == "ppsaturation")
		{
			float v = 1.0f;
			stream >> v;
			if (current->GetType() != ObjectType::PostProcess)
				current->SetType(ObjectType::PostProcess);
			current->SetPostProcessSaturation(v);
		}
		else if (key == "ppvignette")
		{
			float v = 0.0f;
			stream >> v;
			if (current->GetType() != ObjectType::PostProcess)
				current->SetType(ObjectType::PostProcess);
			current->SetPostProcessVignette(v);
		}
		else if (key == "pptint")
		{
			float r = 1.0f, g = 1.0f, b = 1.0f;
			stream >> r >> g >> b;
			if (current->GetType() != ObjectType::PostProcess)
				current->SetType(ObjectType::PostProcess);
			current->SetPostProcessTint(r, g, b);
		}
		else if (key == "texture" && line.size() > 8)
		{
			std::string rel = line.substr(8);
			if (rel.rfind("res/", 0) == 0 || rel.rfind("res\\", 0) == 0)
				rel = std::string(s_ContentFolder) + rel.substr(3);

			std::error_code ec;
			if (fs::exists(rel, ec))
				current->SetTexture(rel);
			else
			{
				fs::path full = fs::path(s_Directory) / rel;
				if (fs::exists(full, ec))
					current->SetTexture(full.generic_string());
				else
					current->SetTexture(rel);
			}
		}
		else if (key == "fontsize")
		{
			float size = 48.0f;
			stream >> size;
			current->SetFontSize(size);
		}
		else if (key == "fontpath")
		{
			std::string rel;
			stream >> rel;
			std::error_code ec;
			if (!s_Directory.empty())
			{
				fs::path full = fs::path(s_Directory) / rel;
				if (fs::exists(full, ec))
					current->SetFontPath(full.generic_string());
				else
					current->SetFontPath(rel);
			}
			else
				current->SetFontPath(rel);
		}
		else if (key == "text" && line.size() > 5)
		{
			current->SetText(line.substr(5));
		}
		else if (key == "flipbook")
		{
			int cols = 1, rows = 1, frameCount = 0, playing = 0, loop = 1, frame = 0;
			float fps = 12.0f;
			stream >> cols >> rows >> frameCount >> fps >> playing >> loop >> frame;
			current->SetFlipbookColumns(cols);
			current->SetFlipbookRows(rows);
			current->SetFlipbookFrameCount(frameCount);
			current->SetFlipbookFps(fps);
			current->SetFlipbookLoop(loop != 0);
			current->SetFlipbookFrame(frame);
			current->SetFlipbookPlaying(playing != 0);
		}
		else if (key == "flipclip")
		{
			std::string name;
			int start = 0, end = 0, loop = 1;
			float fps = -1.0f;
			stream >> name >> start >> end >> fps >> loop;
			if (!name.empty())
				current->AddFlipbookClip(name, start, end, fps, loop != 0);
		}
		else if (key == "flipclipactive")
		{
			std::string name;
			stream >> name;
			if (!name.empty())
				current->SetActiveFlipbookClip(name);
		}
		else if (key == "flipbookasset")
		{
			std::string rel;
			stream >> rel;
			if (!rel.empty())
			{
				fs::path resolved = Project::GetDirectory().empty()
					? fs::path(rel)
					: fs::path(Project::GetDirectory()) / rel;
				if (!fs::exists(resolved))
					resolved = rel;
				current->SetFlipbookAssetPath(resolved.generic_string());
			}
		}
		else if (key == "animasset")
		{
			std::string rel;
			stream >> rel;
			if (!rel.empty())
			{
				fs::path resolved = Project::GetDirectory().empty()
					? fs::path(rel)
					: fs::path(Project::GetDirectory()) / rel;
				if (!fs::exists(resolved))
					resolved = rel;
				current->SetAnimAssetPath(resolved.generic_string());
			}
		}
		else if (key == "animenabled")
		{
			int enabled = 1;
			stream >> enabled;
			current->SetAnimEnabled(enabled != 0);
		}
		else if (key == "tilemap")
		{
			int cols = 16, rows = 12;
			float cell = 32.0f;
			stream >> cols >> rows >> cell;
			current->SetType(ObjectType::Tilemap);
			current->ClearTileLayers();
			current->SetTilemapSize(cols, rows);
			current->SetTileCellSize(cell);
		}
		else if (key == "tilesetgrid")
		{
			int cols = 1, rows = 1;
			stream >> cols >> rows;
			current->SetTilesetGrid(cols, rows);
		}
		else if (key == "tilecollision")
		{
			int enabled = 1;
			stream >> enabled;
			current->SetTileCollisionEnabled(enabled != 0);
		}
		else if (key == "tilesetsolid")
		{
			std::vector<uint8_t> solids;
			int value = 0;
			while (stream >> value)
				solids.push_back(value ? 1 : 0);
			if (current->GetType() != ObjectType::Tilemap)
				current->SetType(ObjectType::Tilemap);
			current->SetTilesetSolidFromList(solids);
		}
		else if (key == "tilelayers")
		{
			int count = 1;
			stream >> count;
			if (current->GetType() != ObjectType::Tilemap)
				current->SetType(ObjectType::Tilemap);
			current->ClearTileLayers();
			(void)count;
		}
		else if (key == "tilelayer")
		{
			int visible = 1;
			int collision = 1;
			float opacity = 1.0f;
			stream >> visible >> collision >> opacity;
			std::string name;
			std::getline(stream, name);
			while (!name.empty() && (name.front() == ' ' || name.front() == '\t'))
				name.erase(name.begin());
			while (!name.empty() && (name.back() == '\r' || name.back() == ' ' || name.back() == '\t'))
				name.pop_back();
			if (name.empty())
				name = "Layer";
			if (current->GetType() != ObjectType::Tilemap)
				current->SetType(ObjectType::Tilemap);
			const int index = current->AddTileLayer(name);
			current->SetTileLayerVisible(index, visible != 0);
			current->SetTileLayerCollision(index, collision != 0);
			current->SetTileLayerOpacity(index, opacity);
		}
		else if (key == "tilelayeractive")
		{
			int active = 0;
			stream >> active;
			current->SetActiveTileLayer(active);
		}
		else if (key == "tiles")
		{
			std::vector<int> tiles;
			int value = 0;
			while (stream >> value)
				tiles.push_back(value);
			if (current->GetType() != ObjectType::Tilemap)
				current->SetType(ObjectType::Tilemap);
			if (current->GetTileLayerCount() <= 0)
				current->AddTileLayer("Layer 0");
			current->SetTilesFromList(tiles);
		}
		else if (key == "collision")
		{
			int type = 0;
			stream >> type;
			current->AddCollision((CollisionType)type);
		}
		else if (key == "collidershape")
		{
			int shape = 0;
			stream >> shape;
			current->SetColliderShape((ColliderShape)shape);
		}
		else if (key == "collisionradius")
		{
			float radius = 50.0f;
			stream >> radius;
			current->SetCollisionRadius(radius);
		}
		else if (key == "collisionsize")
		{
			glm::vec2 size(100.0f);
			stream >> size.x >> size.y;
			current->SetCollisionSize(size);
		}
		else if (key == "colliderverts")
		{
			size_t count = 0;
			stream >> count;
			std::vector<glm::vec2> verts;
			verts.reserve(std::min(count, (size_t)8));
			for (size_t i = 0; i < count && i < 8; ++i)
			{
				glm::vec2 v(0.0f);
				stream >> v.x >> v.y;
				verts.push_back(v);
			}
			current->SetColliderVertices(verts);
		}
		else if (key == "rbmass")
		{
			float mass = 1.0f;
			stream >> mass;
			current->SetRigidbodyMass(mass);
		}
		else if (key == "rbgravity")
		{
			float gravity = 1.0f;
			stream >> gravity;
			current->SetRigidbodyGravityScale(gravity);
		}
		else if (key == "rbkinematic")
		{
			int kinematic = 0;
			stream >> kinematic;
			current->SetRigidbodyKinematic(kinematic != 0);
		}
		else if (key == "trigger")
		{
			int trigger = 0;
			stream >> trigger;
			current->SetIsTrigger(trigger != 0);
		}
		else if (key == "collisionlayer")
		{
			int layer = 0;
			stream >> layer;
			current->SetCollisionLayer(layer);
		}
		else if (key == "collisionmask")
		{
			uint32_t mask = 0xFFFFFFFFu;
			stream >> mask;
			current->SetCollisionMask(mask);
		}
		else if (key == "audio")
		{
			current->AddAudio();
		}
		else if (key == "audiopath" && line.size() > 10)
		{
			std::string rel = line.substr(10);
			if (rel.rfind("res/", 0) == 0 || rel.rfind("res\\", 0) == 0)
				rel = std::string(s_ContentFolder) + rel.substr(3);

			std::error_code ec;
			if (fs::exists(rel, ec))
				current->SetAudioPath(rel);
			else
			{
				fs::path full = fs::path(s_Directory) / rel;
				if (fs::exists(full, ec))
					current->SetAudioPath(full.generic_string());
				else
					current->SetAudioPath(rel);
			}
		}
		else if (key == "audiovolume")
		{
			float volume = 1.0f;
			stream >> volume;
			current->SetAudioVolume(volume);
		}
		else if (key == "audioloop")
		{
			int loop = 0;
			stream >> loop;
			current->SetAudioLoop(loop != 0);
		}
		else if (key == "audioplayonstart")
		{
			int play = 1;
			stream >> play;
			current->SetAudioPlayOnStart(play != 0);
		}
		else if (key == "particle")
		{
			current->AddParticleEmitter();
		}
		else if (key == "particlepath" && line.size() > 13)
		{
			std::string rel = line.substr(13);
			if (rel.rfind("res/", 0) == 0 || rel.rfind("res\\", 0) == 0)
				rel = std::string(s_ContentFolder) + rel.substr(3);

			std::error_code ec;
			if (fs::exists(rel, ec))
				current->SetParticlePath(rel);
			else
			{
				fs::path full = fs::path(s_Directory) / rel;
				if (fs::exists(full, ec))
					current->SetParticlePath(full.generic_string());
				else
					current->SetParticlePath(rel);
			}
		}
		else if (key == "particleplayonstart")
		{
			int play = 1;
			stream >> play;
			current->SetParticlePlayOnStart(play != 0);
		}
		else if (key == "navagent")
		{
			current->AddNavAgent();
		}
		else if (key == "navspeed")
		{
			float speed = 180.0f;
			stream >> speed;
			current->SetNavSpeed(speed);
		}
		else if (key == "navdiagonal")
		{
			int d = 0;
			stream >> d;
			current->SetNavAllowDiagonal(d != 0);
		}
		else if (key == "navstopping")
		{
			float stop = 8.0f;
			stream >> stop;
			current->SetNavStoppingDistance(stop);
		}
		else if (key == "navdebug")
		{
			int d = 0;
			stream >> d;
			current->SetNavDebugDraw(d != 0);
		}
		else if (key == "script")
		{
			current->AddScript();
		}
		else if (key == "scriptpath" && line.size() > 11)
		{
			std::string rel = line.substr(11);
			if (rel.rfind("res/", 0) == 0 || rel.rfind("res\\", 0) == 0)
				rel = std::string(s_ContentFolder) + rel.substr(3);

			std::error_code ec;
			if (fs::exists(rel, ec))
				current->SetScriptPath(rel);
			else
			{
				fs::path full = fs::path(s_Directory) / rel;
				if (fs::exists(full, ec))
					current->SetScriptPath(full.generic_string());
				else
					current->SetScriptPath(rel);
			}
		}
		else if (key == "parent")
		{
			int parentIndex = -1;
			stream >> parentIndex;
			if (outParentIndex && parentIndex >= 0)
				*outParentIndex = parentIndex;
			return true;
		}
		else
		{
			return false;
		}

		return true;

	}

	bool Project::ReadSceneFile(const std::string& path, Renderer& renderer, const std::vector<float>& vertices, const std::vector<unsigned int>& indices, ObjectWindow* objectsWindow)
	{
		std::ifstream in(path);
		if (!in.is_open())
		{
			BZ_INFO("Could not open scene file: {0}", path);
			return false;
		}

		std::string header;
		std::getline(in, header);
		if (header.rfind("BlazeProject", 0) != 0 && header.rfind("BlazeMap", 0) != 0)
		{
			BZ_INFO("Not a Blaze scene/map file: {0}", path);
			return false;
		}

		while (!renderer.GetObjects().empty())
			renderer.DestroyObject(renderer.GetObjects().back());

		if (objectsWindow)
			objectsWindow->ClearHierarchy();

		s_MapScriptPath.clear();

		GameObject* current = nullptr;
		std::vector<std::pair<GameObject*, int>> pendingParents;
		std::string line;
		while (std::getline(in, line))
		{
			while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
				line.pop_back();

			if (line.rfind("map ", 0) == 0)
				continue;

			if (line.rfind("mapscript ", 0) == 0)
			{
				std::string rel = line.substr(10);
				while (!rel.empty() && (rel.front() == ' ' || rel.front() == '\t'))
					rel.erase(rel.begin());

				if (rel.rfind("res/", 0) == 0 || rel.rfind("res\\", 0) == 0)
					rel = std::string(s_ContentFolder) + rel.substr(3);

				std::error_code ec;
				if (fs::exists(rel, ec))
					s_MapScriptPath = rel;
				else
				{
					fs::path full = fs::path(s_Directory) / rel;
					if (fs::exists(full, ec))
						s_MapScriptPath = full.generic_string();
					else
						s_MapScriptPath = rel;
				}
				continue;
			}

			if (line == "hierarchy")
			{
				if (objectsWindow)
					objectsWindow->LoadHierarchy(in, renderer.GetObjects());
				continue;
			}
			if (line == "object")
			{
				renderer.CreateObject(vertices, indices);
				current = renderer.GetObjects().back();
				continue;
			}
			if (!current)
				continue;
			if (line == "end")
			{
				current = nullptr;
				continue;
			}

			int parentIndex = -1;
			if (ApplyGameObjectProperty(current, line, &parentIndex))
			{
				if (parentIndex >= 0)
					pendingParents.push_back({ current, parentIndex });
			}
		}

		{
			const auto& objs = renderer.GetObjects();
			for (const auto& entry : pendingParents)
			{
				if (!entry.first)
					continue;
				if (entry.second < 0 || entry.second >= (int)objs.size())
					continue;
				entry.first->SetParent(objs[entry.second], false);
			}
		}

		BZ_INFO("Scene loaded: {0} ({1} objects)", path, renderer.GetObjects().size());

		if (header.rfind("BlazeMap", 0) == 0)
			EnsureMapScript();

		return true;
	}

	bool Project::SaveScene(Renderer& renderer, ObjectWindow* objectsWindow)
	{
		if (!HasProject())
			return SaveSceneAs(renderer, objectsWindow);

		const bool useMap = !s_ActiveMapFile.empty();
		const std::string scenePath = useMap ? s_ActiveMapFile : s_ProjectFile;
		if (!WriteSceneFile(scenePath, renderer, objectsWindow, useMap))
			return false;

		if (useMap)
			WriteProjectPointer();
		return true;
	}

	bool Project::SaveMap(const std::string& mapPath, Renderer& renderer, ObjectWindow* objectsWindow)
	{
		if (mapPath.empty())
			return false;
		s_ActiveMapFile = mapPath;
		if (!WriteSceneFile(mapPath, renderer, objectsWindow, true))
			return false;
		if (HasProject())
			WriteProjectPointer();
		return true;
	}

	bool Project::SaveSceneAs(Renderer& renderer, ObjectWindow* objectsWindow)
	{
		std::string picked;
		if (!PickProjectLocation(picked, "Choose where to save the project"))
			return false;
		if (!SetupProjectAt(picked))
			return false;

		return SaveScene(renderer, objectsWindow);
	}

	bool Project::LoadMap(const std::string& mapPath, Renderer& renderer, const std::vector<float>& vertices, const std::vector<unsigned int>& indices, ObjectWindow* objectsWindow)
	{
		if (mapPath.empty())
			return false;

		// Drop previous map's UI so widgets don't linger across OpenMap / map changes.
		UISystem::HideAll();

		std::error_code ec;
		fs::path resolved = mapPath;
		if (!fs::exists(resolved, ec) && !s_Directory.empty())
			resolved = fs::path(s_Directory) / mapPath;

		if (!ReadSceneFile(resolved.string(), renderer, vertices, indices, objectsWindow))
			return false;

		s_ActiveMapFile = resolved.string();
		if (HasProject())
			WriteProjectPointer();
		BZ_INFO("Active map: {0}", s_ActiveMapFile);
		return true;
	}

	bool Project::LoadScene(Renderer& renderer, const std::vector<float>& vertices, const std::vector<unsigned int>& indices, ObjectWindow* objectsWindow)
	{
		std::ifstream in(s_ProjectFile);
		if (!in.is_open())
		{
			BZ_INFO("Could not open project file: {0}", s_ProjectFile);
			return false;
		}

		std::string header;
		std::getline(in, header);
		if (header.rfind("BlazeProject", 0) != 0)
		{
			BZ_INFO("Not a Blaze project file: {0}", s_ProjectFile);
			return false;
		}

		std::string mapRef;
		std::string startingRef;
		std::string line;
		bool hasInlineObjects = false;
		s_GameName.clear();
		s_StartingMap.clear();
		while (std::getline(in, line))
		{
			while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
				line.pop_back();

			auto trimValue = [](std::string value)
			{
				while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
					value.erase(value.begin());
				return value;
			};

			if (line.rfind("name ", 0) == 0 || line.rfind("gameName ", 0) == 0)
			{
				const size_t prefix = line.rfind("gameName ", 0) == 0 ? 9 : 5;
				s_GameName = trimValue(line.substr(prefix));
			}
			else if (line.rfind("startingMap ", 0) == 0)
			{
				startingRef = trimValue(line.substr(12));
				s_StartingMap = startingRef;
			}
			else if (line.rfind("map ", 0) == 0)
			{
				mapRef = trimValue(line.substr(4));
			}
			else if (line == "object" || line == "hierarchy")
			{
				hasInlineObjects = true;
				break;
			}
		}

		if (s_GameName.empty())
			s_GameName = s_Name;
		if (s_StartingMap.empty())
			s_StartingMap = startingRef.empty() ? (mapRef.empty() ? GetDefaultMapPath() : mapRef) : startingRef;

		// Always open the project's Starting Map (not the last editor map).
		const std::string loadRef = !s_StartingMap.empty() ? s_StartingMap : mapRef;
		if (!loadRef.empty() && !hasInlineObjects)
		{
			fs::path mapPath = loadRef;
			std::error_code ec;
			if (!fs::exists(mapPath, ec))
				mapPath = fs::path(s_Directory) / loadRef;
			if (!fs::exists(mapPath, ec))
			{
				BZ_INFO("Starting map not found: {0}", loadRef);
				return false;
			}
			return LoadMap(mapPath.string(), renderer, vertices, indices, objectsWindow);
		}

		s_ActiveMapFile.clear();
		return ReadSceneFile(s_ProjectFile, renderer, vertices, indices, objectsWindow);
	}
}
