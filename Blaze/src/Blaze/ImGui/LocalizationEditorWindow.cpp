#include "LocalizationEditorWindow.h"
#include "../vendor/imgui/src/imgui.h"
#include "Blaze/Localization/Localization.h"
#include "Blaze/Logging/Log.h"
#include <cstdio>
#include <cstring>

namespace Blaze
{
	LocalizationEditorWindow* LocalizationEditorWindow::s_Instance = nullptr;

	LocalizationEditorWindow::LocalizationEditorWindow()
	{
		s_Instance = this;
	}

	LocalizationEditorWindow::~LocalizationEditorWindow()
	{
		if (s_Instance == this)
			s_Instance = nullptr;
	}

	void LocalizationEditorWindow::OpenFile(const std::string& path)
	{
		if (s_Instance)
			s_Instance->Open(path);
	}

	void LocalizationEditorWindow::Open(const std::string& path)
	{
		if (path.empty())
			return;
		if (m_Dirty && !m_Asset.GetPath().empty() && m_Asset.GetPath() != path)
			SaveFile();

		if (!m_Asset.LoadFromFile(path))
		{
			BZ_INFO("Failed to open localization: {0}", path);
			return;
		}

		std::snprintf(m_LangBuf, sizeof(m_LangBuf), "%s", m_Asset.GetLanguage().c_str());
		m_NewKey[0] = '\0';
		m_NewValue[0] = '\0';
		m_Dirty = false;
		m_Open = true;
	}

	bool LocalizationEditorWindow::SaveFile()
	{
		if (m_Asset.GetPath().empty())
			return false;
		m_Asset.SetLanguage(m_LangBuf);
		if (!m_Asset.SaveToFile(m_Asset.GetPath()))
			return false;
		m_Dirty = false;
		BZ_INFO("Saved localization: {0}", m_Asset.GetPath());
		return true;
	}

	void LocalizationEditorWindow::DrawEntryRow(int index)
	{
		auto& entries = m_Asset.GetEntries();
		if (index < 0 || index >= (int)entries.size())
			return;

		ImGui::PushID(index);
		char keyBuf[128];
		char valBuf[512];
		std::snprintf(keyBuf, sizeof(keyBuf), "%s", entries[index].first.c_str());
		std::snprintf(valBuf, sizeof(valBuf), "%s", entries[index].second.c_str());

		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.35f);
		if (ImGui::InputText("##key", keyBuf, sizeof(keyBuf)))
		{
			entries[index].first = keyBuf;
			m_Dirty = true;
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70.0f);
		if (ImGui::InputText("##val", valBuf, sizeof(valBuf)))
		{
			entries[index].second = valBuf;
			m_Dirty = true;
		}
		ImGui::SameLine();
		if (ImGui::SmallButton("X"))
		{
			entries.erase(entries.begin() + index);
			m_Dirty = true;
		}
		ImGui::PopID();
	}

	void LocalizationEditorWindow::OnUpdate()
	{
		if (!m_Open)
			return;

		ImGui::SetNextWindowSize(ImVec2(640.0f, 480.0f), ImGuiCond_FirstUseEver);
		std::string title = "Localization";
		if (!m_Asset.GetPath().empty())
			title += " - " + m_Asset.GetPath();
		if (m_Dirty)
			title += " *";
		title += "###LocalizationEditor";

		if (!ImGui::Begin(title.c_str(), &m_Open))
		{
			ImGui::End();
			return;
		}

		if (ImGui::Button("Save"))
			SaveFile();
		ImGui::SameLine();
		if (ImGui::Button("Reload") && !m_Asset.GetPath().empty())
			Open(m_Asset.GetPath());
		ImGui::SameLine();
		if (ImGui::Button("Load Language"))
		{
			if (m_Dirty)
				SaveFile();
			Localization::Load(m_Asset.GetPath());
		}

		ImGui::Separator();
		ImGui::SetNextItemWidth(120.0f);
		if (ImGui::InputText("Language", m_LangBuf, sizeof(m_LangBuf)))
			m_Dirty = true;

		ImGui::TextDisabled("Use {0}, {1}, ... for Loc.Format placeholders. Keys: ui.*, world.*, ...");
		ImGui::Separator();

		ImGui::TextUnformatted("Add entry");
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.35f);
		ImGui::InputText("##newkey", m_NewKey, sizeof(m_NewKey));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 80.0f);
		ImGui::InputText("##newval", m_NewValue, sizeof(m_NewValue));
		ImGui::SameLine();
		if (ImGui::Button("Add") && m_NewKey[0] != '\0')
		{
			m_Asset.SetEntry(m_NewKey, m_NewValue);
			m_NewKey[0] = '\0';
			m_NewValue[0] = '\0';
			m_Dirty = true;
		}

		ImGui::Separator();
		ImGui::BeginChild("##entries", ImVec2(0, 0), true);
		ImGui::TextUnformatted("Key");
		ImGui::SameLine(ImGui::GetContentRegionAvail().x * 0.35f + 16.0f);
		ImGui::TextUnformatted("Value");
		ImGui::Separator();

		for (int i = 0; i < (int)m_Asset.GetEntries().size(); )
		{
			const size_t before = m_Asset.GetEntries().size();
			DrawEntryRow(i);
			if (m_Asset.GetEntries().size() < before)
				continue; // removed
			++i;
		}
		ImGui::EndChild();

		ImGui::End();
	}
}
