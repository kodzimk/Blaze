#include "Project.h"
#include "Blaze/Renderer/Renderer.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Logging/Log.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>

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
		LeaveProjectDirectory();
	}

	const std::string& Project::GetProjectFile() { return s_ProjectFile; }
	const std::string& Project::GetDirectory() { return s_Directory; }
	const std::string& Project::GetName() { return s_Name; }

	std::string Project::GetContentDirectory()
	{
		return s_ContentFolder;
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
		s_ProjectFile = (projectDir / (name + ".blaze")).string();
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

	bool Project::SaveScene(Renderer& renderer)
	{
		if (!HasProject())
			return SaveSceneAs(renderer);

		std::ofstream out(s_ProjectFile, std::ios::trunc);
		if (!out.is_open())
		{
			BZ_INFO("Could not write project file: {0}", s_ProjectFile);
			return false;
		}

		out << "BlazeProject 1\n";
		for (GameObject* object : renderer.GetObjects())
		{
			if (!object)
				continue;

			out << "object\n";
			out << "name " << object->GetName() << "\n";
			out << "type " << (int)object->GetType() << "\n";

			glm::vec3 pos = object->GetPosition();
			out << "pos " << pos.x << " " << pos.y << " " << pos.z << "\n";
			out << "rot " << object->GetRotation() << "\n";

			glm::vec2 scale = object->GetScale();
			out << "scale " << scale.x << " " << scale.y << "\n";

			if (object->IsCamera())
			{
				out << "zoom " << object->GetZoom() << "\n";
				out << "drives " << (object->DrivesView() ? 1 : 0) << "\n";
			}

			if (!object->GetTexturePath().empty())
				out << "texture " << StoreTexturePath(object->GetTexturePath()) << "\n";

			out << "end\n";
		}

		BZ_INFO("Project saved: {0}", s_ProjectFile);
		return true;
	}

	bool Project::SaveSceneAs(Renderer& renderer)
	{
		std::string picked;
		if (!PickProjectLocation(picked, "Choose where to save the project"))
			return false;
		if (!SetupProjectAt(picked))
			return false;

		return SaveScene(renderer);
	}

	bool Project::LoadScene(Renderer& renderer, const std::vector<float>& vertices, const std::vector<unsigned int>& indices)
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

		while (!renderer.GetObjects().empty())
			renderer.DestroyObject(renderer.GetObjects().back());

		GameObject* current = nullptr;
		std::string line;
		while (std::getline(in, line))
		{
			while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
				line.pop_back();

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
				current->SetPosition(pos);
			}
			else if (key == "rot")
			{
				float rotation = 0.0f;
				stream >> rotation;
				current->SetRotation(rotation);
			}
			else if (key == "scale")
			{
				glm::vec2 scale(1.0f);
				stream >> scale.x >> scale.y;
				current->SetScale(scale);
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
		}

		BZ_INFO("Project loaded: {0} ({1} objects)", s_ProjectFile, renderer.GetObjects().size());
		return true;
	}
}
