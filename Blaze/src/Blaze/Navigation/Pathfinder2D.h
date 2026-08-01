#pragma once
#include "Blaze/Core/Core.h"
#include "glm.hpp"
#include <vector>

namespace Blaze
{
	struct CameraProp;
	class GameObject;

	struct BLAZE_API PathPoint
	{
		float x = 0.0f;
		float y = 0.0f;
	};

	// Grid A* over tilemap collision (IsSolidAt). Waypoints are script world space
	// (same as GetLocation / SetLocation / TileCenter).
	class BLAZE_API Pathfinder2D
	{
	public:
		static std::vector<PathPoint> FindPathOn(
			GameObject& tilemap,
			const CameraProp& prop,
			float startX, float startY,
			float endX, float endY,
			bool allowDiagonal = false);

		static std::vector<PathPoint> FindPath(
			const std::vector<GameObject*>& objects,
			const CameraProp& prop,
			float startX, float startY,
			float endX, float endY,
			bool allowDiagonal = false);

		static void DrawPath(
			const std::vector<PathPoint>& path,
			float duration = 0.0f,
			const glm::vec4& color = glm::vec4(0.2f, 1.0f, 0.45f, 1.0f));

	private:
		static GameObject* PickTilemap(
			const std::vector<GameObject*>& objects,
			const CameraProp& prop,
			float startX, float startY,
			float endX, float endY);
	};
}
