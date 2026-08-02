#pragma once
#include "Blaze/Core/Core.h"
#include <string>
#include <vector>

namespace Blaze
{
	struct BLAZE_API FlipbookClip
	{
		std::string name;
		int startFrame = 0;
		int endFrame = 0; // inclusive
		float fps = -1.0f; // <= 0 = use sheet default FPS
		bool loop = true;
	};

	class BLAZE_API FlipbookAsset
	{
	public:
		FlipbookAsset() = default;

		const std::string& GetPath() const { return m_Path; }
		void SetPath(const std::string& path) { m_Path = path; }

		int GetColumns() const { return m_Columns; }
		int GetRows() const { return m_Rows; }
		int GetFrameCount() const { return m_FrameCount; }
		float GetFps() const { return m_Fps; }
		void SetColumns(int columns) { m_Columns = columns > 0 ? columns : 1; }
		void SetRows(int rows) { m_Rows = rows > 0 ? rows : 1; }
		void SetFrameCount(int count) { m_FrameCount = count > 0 ? count : 0; }
		void SetFps(float fps) { m_Fps = fps > 0.01f ? fps : 12.0f; }

		std::vector<FlipbookClip>& GetClips() { return m_Clips; }
		const std::vector<FlipbookClip>& GetClips() const { return m_Clips; }
		void SetClips(const std::vector<FlipbookClip>& clips) { m_Clips = clips; }

		int FindClip(const std::string& name) const;
		int AddClip(const std::string& name, int startFrame, int endFrame, float fps = -1.0f, bool loop = true);
		bool RemoveClip(const std::string& name);

		bool LoadFromFile(const std::string& path);
		bool SaveToFile(const std::string& path) const;

		static FlipbookAsset CreateDefault(const std::string& name = "Flipbook");

	private:
		std::string m_Path;
		int m_Columns = 1;
		int m_Rows = 1;
		int m_FrameCount = 0;
		float m_Fps = 12.0f;
		std::vector<FlipbookClip> m_Clips;
	};
}
