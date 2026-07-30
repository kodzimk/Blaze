#include "OnScreenDebug.h"

#include "../vendor/imgui/src/imgui.h"
#include <deque>

namespace Blaze
{
	struct OnScreenMessage
	{
		std::string text;
		float timeLeft = 0.0f;
		ImU32 color = IM_COL32(255, 230, 25, 255);
	};

	static std::deque<OnScreenMessage> s_Messages;
	static constexpr size_t s_MaxMessages = 24;

	void OnScreenDebug::Print(const std::string& message, float durationSeconds,
		float r, float g, float b, float a)
	{
		if (message.empty())
			return;

		OnScreenMessage msg;
		msg.text = message;
		msg.timeLeft = durationSeconds > 0.0f ? durationSeconds : 4.0f;
		msg.color = IM_COL32(
			(int)(r * 255.0f),
			(int)(g * 255.0f),
			(int)(b * 255.0f),
			(int)(a * 255.0f));

		s_Messages.push_back(std::move(msg));
		while (s_Messages.size() > s_MaxMessages)
			s_Messages.pop_front();
	}

	void OnScreenDebug::Clear()
	{
		s_Messages.clear();
	}

	void OnScreenDebug::Update(float dt)
	{
		if (dt <= 0.0f)
			return;

		for (size_t i = 0; i < s_Messages.size(); )
		{
			s_Messages[i].timeLeft -= dt;
			if (s_Messages[i].timeLeft <= 0.0f)
				s_Messages.erase(s_Messages.begin() + (std::ptrdiff_t)i);
			else
				++i;
		}
	}

	void OnScreenDebug::DrawOverItem()
	{
		if (s_Messages.empty())
			return;

		ImDrawList* draw = ImGui::GetWindowDrawList();
		ImVec2 min = ImGui::GetItemRectMin();
		ImVec2 max = ImGui::GetItemRectMax();

		float x = min.x + 10.0f;
		float y = min.y + 10.0f;
		const float lineHeight = ImGui::GetTextLineHeightWithSpacing();

		for (const OnScreenMessage& msg : s_Messages)
		{
			if (y + lineHeight > max.y - 4.0f)
				break;

			float alpha = 1.0f;
			if (msg.timeLeft < 0.75f)
				alpha = msg.timeLeft / 0.75f;

			ImU32 col = msg.color;
			int a = (int)(((col >> IM_COL32_A_SHIFT) & 0xFF) * alpha);
			col = (col & ~IM_COL32_A_MASK) | ((ImU32)a << IM_COL32_A_SHIFT);

			ImU32 shadow = IM_COL32(0, 0, 0, (int)(180 * alpha));
			draw->AddText(ImVec2(x + 1.0f, y + 1.0f), shadow, msg.text.c_str());
			draw->AddText(ImVec2(x, y), col, msg.text.c_str());
			y += lineHeight;
		}
	}
}
