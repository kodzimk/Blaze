#pragma once
#include"bzpch.h"
#include"Blaze/Core/Core.h"

namespace Blaze
{
	class Renderer;

	class BLAZE_API Project
	{
	public:
		static bool NewProject();
		static bool OpenProjectDialog();
		static bool SaveScene(Renderer& renderer);
		static bool SaveSceneAs(Renderer& renderer);
		static bool LoadScene(Renderer& renderer, const std::vector<float>& vertices, const std::vector<unsigned int>& indices);

		static bool HasProject();
		static void CloseProject();
		static const std::string& GetProjectFile();
		static const std::string& GetDirectory();
		static std::string GetContentDirectory();
		static const std::string& GetName();

		static void SetProjectFromFile(const std::string& projectFile);
		static std::vector<std::string> GetRecentProjects();
		static void RegisterRecentProject(const std::string& projectFile);
	private:
		static bool SetupProjectAt(const std::string& pickedFile);
		static std::string StoreTexturePath(const std::string& texturePath);
	private:
		static std::string s_ProjectFile;
		static std::string s_Directory;
		static std::string s_Name;
	};
}
