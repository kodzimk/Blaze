#pragma once
#include "Blaze/Core/Core.h"
#include "glm.hpp"
#include <vector>

namespace Blaze
{
	class Camera;

	// Timed world-space debug primitives (UE DrawDebugLine style). Flushed into the Game view.
	class BLAZE_API DebugDraw2D
	{
	public:
		static void DrawLine(const glm::vec2& a, const glm::vec2& b,
			const glm::vec4& color = glm::vec4(0.2f, 1.0f, 0.3f, 1.0f), float duration = 0.0f, float thickness = 2.0f);
		static void DrawPoint(const glm::vec2& p,
			const glm::vec4& color = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f), float duration = 0.0f, float radius = 5.0f);
		static void DrawRaycast(const glm::vec2& start, const glm::vec2& end, bool hit, const glm::vec2& hitPoint,
			float duration = 0.0f);

		static void Update(float dt);
		static void Flush(const Camera& camera);
		static void Clear();

	private:
		struct Line
		{
			glm::vec2 a{};
			glm::vec2 b{};
			glm::vec4 color{ 1.0f };
			float thickness = 2.0f;
			float timeLeft = 0.0f;
		};
		struct Point
		{
			glm::vec2 p{};
			glm::vec4 color{ 1.0f };
			float radius = 5.0f;
			float timeLeft = 0.0f;
		};

		static std::vector<Line> s_Lines;
		static std::vector<Point> s_Points;
	};
}
