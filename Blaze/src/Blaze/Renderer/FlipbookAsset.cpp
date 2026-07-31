#include "FlipbookAsset.h"
#include "Blaze/Logging/Log.h"
#include <fstream>
#include <sstream>

namespace Blaze
{
	int FlipbookAsset::FindClip(const std::string& name) const
	{
		for (int i = 0; i < (int)m_Clips.size(); ++i)
		{
			if (m_Clips[i].name == name)
				return i;
		}
		return -1;
	}

	int FlipbookAsset::AddClip(const std::string& name, int startFrame, int endFrame, float fps, bool loop)
	{
		std::string unique = name.empty() ? "Clip" : name;
		if (FindClip(unique) >= 0)
		{
			for (int i = 2; i < 10000; ++i)
			{
				const std::string candidate = unique + "_" + std::to_string(i);
				if (FindClip(candidate) < 0)
				{
					unique = candidate;
					break;
				}
			}
		}

		FlipbookClip clip;
		clip.name = unique;
		clip.startFrame = startFrame < 0 ? 0 : startFrame;
		clip.endFrame = endFrame < clip.startFrame ? clip.startFrame : endFrame;
		clip.fps = fps;
		clip.loop = loop;
		m_Clips.push_back(clip);
		return (int)m_Clips.size() - 1;
	}

	bool FlipbookAsset::RemoveClip(const std::string& name)
	{
		const int index = FindClip(name);
		if (index < 0)
			return false;
		m_Clips.erase(m_Clips.begin() + index);
		return true;
	}

	bool FlipbookAsset::LoadFromFile(const std::string& path)
	{
		std::ifstream in(path);
		if (!in.is_open())
		{
			BZ_CORE_ERROR("Failed to open flipbook: {0}", path);
			return false;
		}

		m_Path = path;
		m_Clips.clear();
		m_Columns = 1;
		m_Rows = 1;
		m_FrameCount = 0;
		m_Fps = 12.0f;

		std::string line;
		bool headerOk = false;
		while (std::getline(in, line))
		{
			if (line.empty() || line[0] == '#')
				continue;

			std::istringstream stream(line);
			std::string key;
			stream >> key;
			if (key == "BlazeFlipbook")
			{
				headerOk = true;
				continue;
			}
			if (!headerOk)
				continue;

			if (key == "columns")
			{
				int v = 1;
				stream >> v;
				SetColumns(v);
			}
			else if (key == "rows")
			{
				int v = 1;
				stream >> v;
				SetRows(v);
			}
			else if (key == "framecount")
			{
				int v = 0;
				stream >> v;
				SetFrameCount(v);
			}
			else if (key == "fps")
			{
				float v = 12.0f;
				stream >> v;
				SetFps(v);
			}
			else if (key == "clip")
			{
				std::string name;
				int start = 0, end = 0, loop = 1;
				float fps = -1.0f;
				stream >> name >> start >> end >> fps >> loop;
				if (!name.empty())
					AddClip(name, start, end, fps, loop != 0);
			}
		}

		if (!headerOk)
		{
			BZ_CORE_ERROR("Invalid flipbook header: {0}", path);
			return false;
		}
		return true;
	}

	bool FlipbookAsset::SaveToFile(const std::string& path) const
	{
		std::ofstream out(path);
		if (!out.is_open())
		{
			BZ_CORE_ERROR("Failed to save flipbook: {0}", path);
			return false;
		}

		out << "BlazeFlipbook 1\n";
		out << "columns " << m_Columns << "\n";
		out << "rows " << m_Rows << "\n";
		out << "framecount " << m_FrameCount << "\n";
		out << "fps " << m_Fps << "\n";
		for (const FlipbookClip& clip : m_Clips)
		{
			out << "clip " << clip.name
				<< " " << clip.startFrame
				<< " " << clip.endFrame
				<< " " << clip.fps
				<< " " << (clip.loop ? 1 : 0) << "\n";
		}
		return true;
	}

	FlipbookAsset FlipbookAsset::CreateDefault(const std::string& name)
	{
		FlipbookAsset asset;
		asset.SetColumns(4);
		asset.SetRows(2);
		asset.SetFps(12.0f);
		asset.AddClip("Idle", 0, 3, -1.0f, true);
		asset.AddClip("Run", 4, 7, -1.0f, true);
		(void)name;
		return asset;
	}
}
