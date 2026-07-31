#include "DebugDraw2D.h"
#include "Blaze/Renderer/Camera.h"
#include "Blaze/Renderer/Gizmo.h"
#include <algorithm>

namespace Blaze
{
	std::vector<DebugDraw2D::Line> DebugDraw2D::s_Lines;
	std::vector<DebugDraw2D::Point> DebugDraw2D::s_Points;

	void DebugDraw2D::DrawLine(const glm::vec2& a, const glm::vec2& b, const glm::vec4& color, float duration, float thickness)
	{
		Line line;
		line.a = a;
		line.b = b;
		line.color = color;
		line.thickness = glm::max(thickness, 0.5f);
		line.timeLeft = duration;
		s_Lines.push_back(line);
	}

	void DebugDraw2D::DrawPoint(const glm::vec2& p, const glm::vec4& color, float duration, float radius)
	{
		Point point;
		point.p = p;
		point.color = color;
		point.radius = glm::max(radius, 1.0f);
		point.timeLeft = duration;
		s_Points.push_back(point);
	}

	void DebugDraw2D::DrawRaycast(const glm::vec2& start, const glm::vec2& end, bool hit, const glm::vec2& hitPoint, float duration)
	{
		if (hit)
		{
			DrawLine(start, hitPoint, glm::vec4(0.15f, 1.0f, 0.35f, 1.0f), duration, 2.5f);
			DrawLine(hitPoint, end, glm::vec4(1.0f, 0.25f, 0.2f, 0.55f), duration, 1.5f);
			DrawPoint(hitPoint, glm::vec4(1.0f, 0.85f, 0.15f, 1.0f), duration, 6.0f);
		}
		else
		{
			DrawLine(start, end, glm::vec4(1.0f, 0.3f, 0.25f, 1.0f), duration, 2.5f);
		}
	}

	void DebugDraw2D::Update(float dt)
	{
		if (dt <= 0.0f)
			return;

		auto expireLines = [&](std::vector<Line>& items)
		{
			for (Line& item : items)
			{
				if (item.timeLeft > 0.0f)
					item.timeLeft -= dt;
			}
			items.erase(std::remove_if(items.begin(), items.end(),
				[](const Line& item) { return item.timeLeft < 0.0f; }), items.end());
		};
		auto expirePoints = [&](std::vector<Point>& items)
		{
			for (Point& item : items)
			{
				if (item.timeLeft > 0.0f)
					item.timeLeft -= dt;
			}
			items.erase(std::remove_if(items.begin(), items.end(),
				[](const Point& item) { return item.timeLeft < 0.0f; }), items.end());
		};
		expireLines(s_Lines);
		expirePoints(s_Points);
	}

	void DebugDraw2D::Flush(const Camera& camera)
	{
		// One-shot (duration==0) items are drawn once then removed after flush.
		static Gizmo2D s_Drawer;

		for (const Line& line : s_Lines)
			s_Drawer.DrawDebugLine(camera, line.a, line.b, line.color, line.thickness);
		for (const Point& point : s_Points)
			s_Drawer.DrawDebugPoint(camera, point.p, point.color, point.radius);

		s_Lines.erase(std::remove_if(s_Lines.begin(), s_Lines.end(),
			[](const Line& item) { return item.timeLeft <= 0.0f; }), s_Lines.end());
		s_Points.erase(std::remove_if(s_Points.begin(), s_Points.end(),
			[](const Point& item) { return item.timeLeft <= 0.0f; }), s_Points.end());
	}

	void DebugDraw2D::Clear()
	{
		s_Lines.clear();
		s_Points.clear();
	}
}
