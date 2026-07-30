#include "ProjectSettingsWindow.h"
#include "Blaze/Core/Project.h"
#include "Blaze/Logging/Log.h"
#include "../vendor/imgui/src/imgui.h"
#include <cstring>
#include <algorithm>
#include <filesystem>

namespace fs = std::filesystem;

namespace Blaze
{
	std::string ProjectSettingsWindow::s_PendingOpenMap;

	ProjectSettingsWindow::ProjectSettingsWindow() = default;
	ProjectSettingsWindow::~ProjectSettingsWindow() = default;

	bool ProjectSettingsWindow::ConsumePendingOpenMap(std::string& outPath)
	{
		if (s_PendingOpenMap.empty())
			return false;
		outPath = s_PendingOpenMap;
		s_PendingOpenMap.clear();
		return true;
	}

	void ProjectSettingsWindow::Open()
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

	void ProjectSettingsWindow::SyncFromProject()
	{
		std::memset(m_GameNameBuffer, 0, sizeof(m_GameNameBuffer));
		std::strncpy(m_GameNameBuffer, Project::GetGameName().c_str(), sizeof(m_GameNameBuffer) - 1);

		m_MapFiles = Project::ListMapFiles();
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
			m_MapFiles.push_back(startingRel);

		m_NeedsSync = false;
	}

	void ProjectSettingsWindow::ApplyToProject()
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
			Project::SetStartingMap(m_MapFiles[m_SelectedMapIndex]);
		else if (Project::GetStartingMap().empty())
			Project::SetStartingMap(Project::GetDefaultMapPath());

		if (Project::SaveProjectSettings())
		{
			BZ_INFO("Project settings saved (game: {0}, start map: {1})",
				Project::GetGameName(), Project::GetStartingMap());

			std::string mapToOpen = Project::GetStartingMap();
			std::error_code ec;
			fs::path resolved = mapToOpen;
			if (!fs::exists(resolved, ec) && !Project::GetDirectory().empty())
				resolved = fs::path(Project::GetDirectory()) / mapToOpen;
			if (fs::exists(resolved, ec))
				s_PendingOpenMap = resolved.string();
		}
		else
			BZ_INFO("Failed to save project settings");
	}

	void ProjectSettingsWindow::OnUpdate()
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

		ImGui::SetNextWindowSize(ImVec2(420.0f, 260.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Project Settings", &m_Open))
		{
			ImGui::End();
			return;
		}

		ImGui::TextUnformatted("General");
		ImGui::Separator();
		ImGui::Spacing();

		ImGui::TextUnformatted("Game Name");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputText("##GameName", m_GameNameBuffer, sizeof(m_GameNameBuffer));

		ImGui::Spacing();
		ImGui::TextUnformatted("Starting Map");
		ImGui::TextDisabled("Loaded when the project opens");
		ImGui::SetNextItemWidth(-1.0f);

		const char* preview = m_MapFiles.empty()
			? "(no maps found)"
			: m_MapFiles[std::clamp(m_SelectedMapIndex, 0, (int)m_MapFiles.size() - 1)].c_str();

		if (ImGui::BeginCombo("##StartingMap", preview))
		{
			for (int i = 0; i < (int)m_MapFiles.size(); ++i)
			{
				const bool selected = (i == m_SelectedMapIndex);
				if (ImGui::Selectable(m_MapFiles[i].c_str(), selected))
					m_SelectedMapIndex = i;
				if (selected)
					ImGui::SetItemDefaultFocus();
			}
			ImGui::EndCombo();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button("Apply", ImVec2(100.0f, 0.0f)))
			ApplyToProject();
		ImGui::SameLine();
		if (ImGui::Button("Close", ImVec2(100.0f, 0.0f)))
			m_Open = false;

		ImGui::End();
	}
}
