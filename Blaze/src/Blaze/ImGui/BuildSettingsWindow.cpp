#include "BuildSettingsWindow.h"
#include "Blaze/Core/Project.h"
#include "Blaze/Logging/Log.h"
#include "../vendor/imgui/src/imgui.h"
#include <cstring>
#include <algorithm>
#include <filesystem>
#include <windows.h>
#include <commdlg.h>

namespace fs = std::filesystem;

namespace Blaze
{
	BuildSettingsWindow::BuildSettingsWindow() = default;
	BuildSettingsWindow::~BuildSettingsWindow() = default;

	void BuildSettingsWindow::Open()
	{
		m_Open = true;
		m_NeedsSync = true;
	}

	static bool SameMapPath(const std::string& a, const std::string& b)
	{
		if (a.empty() || b.empty())
			return false;
		if (a == b)
			return true;

		std::error_code ec;
		fs::path pa = fs::weakly_canonical(a, ec);
		if (ec) pa = fs::path(a);
		fs::path pb = fs::weakly_canonical(b, ec);
		if (ec) pb = fs::path(b);
		return pa.generic_string() == pb.generic_string()
			|| pa.filename() == pb.filename();
	}

	void BuildSettingsWindow::SyncFromProject()
	{
		std::memset(m_GameNameBuffer, 0, sizeof(m_GameNameBuffer));
		std::strncpy(m_GameNameBuffer, Project::GetGameName().c_str(), sizeof(m_GameNameBuffer) - 1);

		m_MapFiles = Project::ListMapFiles();
		m_MapIncluded.assign(m_MapFiles.size(), 1);
		m_SelectedMapIndex = 0;

		const std::string& starting = Project::GetStartingMap();
		std::string startingRel = starting;
		if (!starting.empty() && !Project::GetDirectory().empty())
		{
			std::error_code ec;
			fs::path abs = fs::absolute(starting, ec);
			fs::path rel = fs::relative(abs, Project::GetDirectory(), ec);
			if (!ec && !rel.empty() && rel.generic_string().rfind("..", 0) != 0)
				startingRel = rel.generic_string();
		}

		for (int i = 0; i < (int)m_MapFiles.size(); ++i)
		{
			if (SameMapPath(m_MapFiles[i], startingRel) || SameMapPath(m_MapFiles[i], starting))
			{
				m_SelectedMapIndex = i;
				break;
			}
		}

		if (m_MapFiles.empty() && !startingRel.empty())
		{
			m_MapFiles.push_back(startingRel);
			m_MapIncluded.push_back(1);
		}

		if (m_OutputExeBuffer[0] == '\0' && !Project::GetDirectory().empty())
		{
			std::string name = Project::GetGameName();
			if (name.empty())
				name = Project::GetName();
			if (name.empty())
				name = "Game";
			for (char& c : name)
			{
				if (c == '<' || c == '>' || c == ':' || c == '"' || c == '/' || c == '\\' || c == '|' || c == '?' || c == '*')
					c = '_';
			}
			const std::string defaultPath = (fs::path(Project::GetDirectory()) / "Builds" / (name + ".exe")).string();
			std::strncpy(m_OutputExeBuffer, defaultPath.c_str(), sizeof(m_OutputExeBuffer) - 1);
		}

		m_NeedsSync = false;
	}

	void BuildSettingsWindow::ApplyToProject()
	{
		std::string gameName = m_GameNameBuffer;
		while (!gameName.empty() && (gameName.front() == ' ' || gameName.front() == '\t'))
			gameName.erase(gameName.begin());
		while (!gameName.empty() && (gameName.back() == ' ' || gameName.back() == '\t'))
			gameName.pop_back();
		if (gameName.empty())
			gameName = Project::GetName();
		Project::SetGameName(gameName);

		if (!m_MapFiles.empty() && m_SelectedMapIndex >= 0 && m_SelectedMapIndex < (int)m_MapFiles.size())
		{
			if (m_SelectedMapIndex < (int)m_MapIncluded.size())
				m_MapIncluded[m_SelectedMapIndex] = 1;
			Project::SetStartingMap(m_MapFiles[m_SelectedMapIndex]);
		}
		else if (Project::GetStartingMap().empty())
			Project::SetStartingMap(Project::GetDefaultMapPath());

		if (Project::SaveProjectSettings())
			BZ_INFO("Build settings applied (game: {0}, start map: {1})",
				Project::GetGameName(), Project::GetStartingMap());
		else
			BZ_INFO("Failed to save project settings");
	}

	void BuildSettingsWindow::BrowseOutputExe()
	{
		char filePath[MAX_PATH] = {};
		if (m_OutputExeBuffer[0] != '\0')
			std::strncpy(filePath, m_OutputExeBuffer, MAX_PATH - 1);
		else
		{
			std::string name = m_GameNameBuffer[0] ? m_GameNameBuffer : "Game";
			std::snprintf(filePath, MAX_PATH, "%s.exe", name.c_str());
		}

		OPENFILENAMEA ofn = {};
		ofn.lStructSize = sizeof(ofn);
		ofn.lpstrFile = filePath;
		ofn.nMaxFile = MAX_PATH;
		ofn.lpstrFilter = "Executable (*.exe)\0*.exe\0All Files (*.*)\0*.*\0";
		ofn.nFilterIndex = 1;
		ofn.lpstrDefExt = "exe";
		ofn.lpstrTitle = "Build output executable";
		ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
		if (GetSaveFileNameA(&ofn))
		{
			std::memset(m_OutputExeBuffer, 0, sizeof(m_OutputExeBuffer));
			std::strncpy(m_OutputExeBuffer, filePath, sizeof(m_OutputExeBuffer) - 1);
		}
	}

	bool BuildSettingsWindow::RunBuild(bool promptIfNoPath)
	{
		ApplyToProject();

		if (m_SaveScene && !m_SaveScene())
		{
			BZ_INFO("Build cancelled: failed to save scene");
			return false;
		}

		Project::BuildGameOptions options;
		options.gameName = m_GameNameBuffer;
		if (!m_MapFiles.empty() && m_SelectedMapIndex >= 0 && m_SelectedMapIndex < (int)m_MapFiles.size())
			options.startingMap = m_MapFiles[m_SelectedMapIndex];

		bool anyExcluded = false;
		for (char included : m_MapIncluded)
		{
			if (!included)
			{
				anyExcluded = true;
				break;
			}
		}
		if (anyExcluded)
		{
			for (int i = 0; i < (int)m_MapFiles.size(); ++i)
			{
				if (i < (int)m_MapIncluded.size() && m_MapIncluded[i])
					options.includedMaps.push_back(m_MapFiles[i]);
			}
			if (!options.startingMap.empty())
			{
				bool startListed = false;
				for (const std::string& m : options.includedMaps)
				{
					if (SameMapPath(m, options.startingMap))
					{
						startListed = true;
						break;
					}
				}
				if (!startListed)
					options.includedMaps.push_back(options.startingMap);
			}
		}

		if (m_OutputExeBuffer[0] != '\0')
		{
			options.outputExePath = m_OutputExeBuffer;
			options.promptForOutputPath = false;
		}
		else
		{
			options.promptForOutputPath = promptIfNoPath;
		}

		return Project::BuildGame(options);
	}

	void BuildSettingsWindow::OnUpdate()
	{
		if (!m_Open)
			return;

		if (!Project::HasProject())
		{
			m_Open = false;
			return;
		}

		if (m_NeedsSync)
			SyncFromProject();

		ImGui::SetNextWindowSize(ImVec2(520.0f, 480.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Build Settings", &m_Open))
		{
			ImGui::End();
			return;
		}

		ImGui::TextUnformatted("Player Settings");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextUnformatted("Game Name");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputText("##BuildGameName", m_GameNameBuffer, sizeof(m_GameNameBuffer));

		ImGui::Spacing();
		ImGui::TextUnformatted("Scenes In Build");
		ImGui::TextDisabled("Unchecked maps are stripped from the package (other content stays).");
		ImGui::BeginChild("##ScenesInBuild", ImVec2(0.0f, 160.0f), true);
		for (int i = 0; i < (int)m_MapFiles.size(); ++i)
		{
			ImGui::PushID(i);
			bool included = i < (int)m_MapIncluded.size() ? (m_MapIncluded[i] != 0) : true;
			const bool isStart = (i == m_SelectedMapIndex);
			if (ImGui::Checkbox("##inc", &included))
			{
				if (i >= (int)m_MapIncluded.size())
					m_MapIncluded.resize(m_MapFiles.size(), 1);
				// Starting scene must stay included.
				if (isStart)
					included = true;
				m_MapIncluded[i] = included ? 1 : 0;
			}
			ImGui::SameLine();
			if (ImGui::Selectable(m_MapFiles[i].c_str(), isStart))
			{
				m_SelectedMapIndex = i;
				if (i < (int)m_MapIncluded.size())
					m_MapIncluded[i] = 1;
			}
			if (isStart)
			{
				ImGui::SameLine();
				ImGui::TextDisabled("(start)");
			}
			ImGui::PopID();
		}
		if (m_MapFiles.empty())
			ImGui::TextDisabled("No .map files under content/");
		ImGui::EndChild();

		ImGui::Spacing();
		ImGui::TextUnformatted("Starting Scene");
		ImGui::SetNextItemWidth(-1.0f);
		const char* preview = m_MapFiles.empty()
			? "(no maps)"
			: m_MapFiles[std::clamp(m_SelectedMapIndex, 0, (int)m_MapFiles.size() - 1)].c_str();
		if (ImGui::BeginCombo("##BuildStartMap", preview))
		{
			for (int i = 0; i < (int)m_MapFiles.size(); ++i)
			{
				const bool selected = (i == m_SelectedMapIndex);
				if (ImGui::Selectable(m_MapFiles[i].c_str(), selected))
				{
					m_SelectedMapIndex = i;
					if (i < (int)m_MapIncluded.size())
						m_MapIncluded[i] = 1;
				}
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::Spacing();
		ImGui::TextUnformatted("Output");
		ImGui::Separator();
		ImGui::TextUnformatted("Executable Path");
		ImGui::SetNextItemWidth(-80.0f);
		ImGui::InputText("##BuildOutExe", m_OutputExeBuffer, sizeof(m_OutputExeBuffer));
		ImGui::SameLine();
		if (ImGui::Button("Browse..."))
			BrowseOutputExe();
		ImGui::TextDisabled("Packages Runtime.exe → your .exe, Blaze.dll, content/, game.blaze");

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Apply", ImVec2(100.0f, 0.0f)))
			ApplyToProject();
		ImGui::SameLine();
		if (ImGui::Button("Build Game", ImVec2(120.0f, 0.0f)))
			RunBuild(true);
		ImGui::SameLine();
		if (ImGui::Button("Close", ImVec2(100.0f, 0.0f)))
			m_Open = false;

		ImGui::End();
	}
}
