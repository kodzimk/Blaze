#include "Pathfinder2D.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Renderer/Camera.h"
#include "Blaze/Renderer/DebugDraw2D.h"
#include <cmath>
#include <queue>
#include <limits>
#include <algorithm>

namespace Blaze
{
	namespace
	{
		struct OpenItem
		{
			float f = 0.0f;
			int index = 0;
			bool operator>(const OpenItem& other) const { return f > other.f; }
		};

		static float Heuristic(int ax, int ay, int bx, int by, bool diagonal)
		{
			const float dx = (float)std::abs(ax - bx);
			const float dy = (float)std::abs(ay - by);
			if (diagonal)
				return std::max(dx, dy) + (1.41421356f - 1.0f) * std::min(dx, dy);
			return dx + dy;
		}

		static bool InBounds(const GameObject& map, int x, int y)
		{
			return x >= 0 && y >= 0 && x < map.GetTilemapColumns() && y < map.GetTilemapRows();
		}

		static bool IsWalkable(const GameObject& map, int x, int y, int startX, int startY, int endX, int endY)
		{
			if (!InBounds(map, x, y))
				return false;
			// Allow standing on / targeting a currently solid cell (edge cases).
			if ((x == startX && y == startY) || (x == endX && y == endY))
				return true;
			return !map.IsSolidAt(x, y);
		}
	}

	GameObject* Pathfinder2D::PickTilemap(
		const std::vector<GameObject*>& objects,
		const CameraProp& prop,
		float startX, float startY,
		float endX, float endY)
	{
		GameObject* both = nullptr;
		GameObject* startOnly = nullptr;
		GameObject* endOnly = nullptr;

		for (GameObject* object : objects)
		{
			if (!object || !object->IsTilemap() || !object->IsVisible())
				continue;
			if (!object->IsTileCollisionEnabled())
				continue;
			if (object->GetTilemapColumns() <= 0 || object->GetTilemapRows() <= 0)
				continue;

			int sx = 0, sy = 0, ex = 0, ey = 0;
			const bool hasStart = object->WorldToTile(prop, glm::vec2(startX, startY), sx, sy);
			const bool hasEnd = object->WorldToTile(prop, glm::vec2(endX, endY), ex, ey);
			if (hasStart && hasEnd)
			{
				both = object;
				break;
			}
			if (hasStart && !startOnly)
				startOnly = object;
			if (hasEnd && !endOnly)
				endOnly = object;
		}

		if (both)
			return both;
		if (startOnly)
			return startOnly;
		return endOnly;
	}

	std::vector<PathPoint> Pathfinder2D::FindPathOn(
		GameObject& tilemap,
		const CameraProp& prop,
		float startX, float startY,
		float endX, float endY,
		bool allowDiagonal)
	{
		std::vector<PathPoint> empty;
		if (!tilemap.IsTilemap())
			return empty;

		const int cols = tilemap.GetTilemapColumns();
		const int rows = tilemap.GetTilemapRows();
		if (cols <= 0 || rows <= 0)
			return empty;

		int startTX = 0, startTY = 0, endTX = 0, endTY = 0;
		if (!tilemap.WorldToTile(prop, glm::vec2(startX, startY), startTX, startTY))
			return empty;
		if (!tilemap.WorldToTile(prop, glm::vec2(endX, endY), endTX, endTY))
			return empty;

		if (!IsWalkable(tilemap, startTX, startTY, startTX, startTY, endTX, endTY))
			return empty;
		if (!IsWalkable(tilemap, endTX, endTY, startTX, startTY, endTX, endTY))
			return empty;

		if (startTX == endTX && startTY == endTY)
		{
			const glm::vec2 c = tilemap.TileCenterWorld(prop, startTX, startTY);
			return { PathPoint{ c.x, c.y } };
		}

		const int cellCount = cols * rows;
		std::vector<float> bestG((size_t)cellCount, std::numeric_limits<float>::infinity());
		std::vector<int> cameFrom((size_t)cellCount, -1);
		std::vector<uint8_t> closed((size_t)cellCount, 0);

		auto indexOf = [cols](int x, int y) { return y * cols + x; };

		std::priority_queue<OpenItem, std::vector<OpenItem>, std::greater<OpenItem>> open;
		const int startIndex = indexOf(startTX, startTY);
		bestG[(size_t)startIndex] = 0.0f;
		open.push(OpenItem{ Heuristic(startTX, startTY, endTX, endTY, allowDiagonal), startIndex });

		static const int kOrthoDX[4] = { 1, -1, 0, 0 };
		static const int kOrthoDY[4] = { 0, 0, 1, -1 };
		static const int kDiagDX[8] = { 1, -1, 0, 0, 1, 1, -1, -1 };
		static const int kDiagDY[8] = { 0, 0, 1, -1, 1, -1, 1, -1 };

		const int neighborCount = allowDiagonal ? 8 : 4;
		const int* dx = allowDiagonal ? kDiagDX : kOrthoDX;
		const int* dy = allowDiagonal ? kDiagDY : kOrthoDY;

		bool found = false;
		int endIndex = indexOf(endTX, endTY);
		int iterations = 0;
		const int maxIterations = cellCount * 8 + 64;

		while (!open.empty() && iterations++ < maxIterations)
		{
			const OpenItem current = open.top();
			open.pop();
			const int ci = current.index;
			if (closed[(size_t)ci])
				continue;
			closed[(size_t)ci] = 1;

			if (ci == endIndex)
			{
				found = true;
				break;
			}

			const int cx = ci % cols;
			const int cy = ci / cols;
			const float gHere = bestG[(size_t)ci];

			for (int n = 0; n < neighborCount; ++n)
			{
				const int nx = cx + dx[n];
				const int ny = cy + dy[n];
				if (!IsWalkable(tilemap, nx, ny, startTX, startTY, endTX, endTY))
					continue;

				// No corner cutting on diagonals.
				if (allowDiagonal && dx[n] != 0 && dy[n] != 0)
				{
					if (!IsWalkable(tilemap, cx + dx[n], cy, startTX, startTY, endTX, endTY) ||
						!IsWalkable(tilemap, cx, cy + dy[n], startTX, startTY, endTX, endTY))
						continue;
				}

				const int ni = indexOf(nx, ny);
				if (closed[(size_t)ni])
					continue;

				const bool diagonalStep = (dx[n] != 0 && dy[n] != 0);
				const float stepCost = diagonalStep ? 1.41421356f : 1.0f;
				const float tentative = gHere + stepCost;
				if (tentative >= bestG[(size_t)ni])
					continue;

				bestG[(size_t)ni] = tentative;
				cameFrom[(size_t)ni] = ci;
				const float f = tentative + Heuristic(nx, ny, endTX, endTY, allowDiagonal);
				open.push(OpenItem{ f, ni });
			}
		}

		if (!found)
			return empty;

		std::vector<int> indices;
		for (int at = endIndex; at >= 0; at = cameFrom[(size_t)at])
		{
			indices.push_back(at);
			if (at == startIndex)
				break;
		}
		std::reverse(indices.begin(), indices.end());

		std::vector<PathPoint> path;
		path.reserve(indices.size());
		for (int idx : indices)
		{
			const int tx = idx % cols;
			const int ty = idx / cols;
			const glm::vec2 c = tilemap.TileCenterWorld(prop, tx, ty);
			path.push_back(PathPoint{ c.x, c.y });
		}
		return path;
	}

	std::vector<PathPoint> Pathfinder2D::FindPath(
		const std::vector<GameObject*>& objects,
		const CameraProp& prop,
		float startX, float startY,
		float endX, float endY,
		bool allowDiagonal)
	{
		GameObject* map = PickTilemap(objects, prop, startX, startY, endX, endY);
		if (!map)
			return {};
		return FindPathOn(*map, prop, startX, startY, endX, endY, allowDiagonal);
	}

	void Pathfinder2D::DrawPath(const std::vector<PathPoint>& path, float duration, const glm::vec4& color)
	{
		if (path.empty())
			return;

		for (size_t i = 0; i + 1 < path.size(); ++i)
		{
			DebugDraw2D::DrawLine(
				glm::vec2(path[i].x, path[i].y),
				glm::vec2(path[i + 1].x, path[i + 1].y),
				color, duration, 2.5f);
		}
		for (const PathPoint& p : path)
			DebugDraw2D::DrawPoint(glm::vec2(p.x, p.y), color, duration, 4.0f);
	}
}
