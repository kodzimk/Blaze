#include "NavAgentSystem.h"
#include "Pathfinder2D.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Renderer/Camera.h"
#include "Blaze/Logging/Log.h"
#include <unordered_map>
#include <cmath>

namespace Blaze
{
	const std::vector<GameObject*>* NavAgentSystem::s_Objects = nullptr;

	struct AgentRuntime
	{
		std::vector<PathPoint> path;
		int waypoint = 0;
	};

	static std::unordered_map<GameObject*, AgentRuntime> s_Runtime;

	void NavAgentSystem::SetObjects(const std::vector<GameObject*>* objects)
	{
		s_Objects = objects;
	}

	void NavAgentSystem::Clear()
	{
		s_Runtime.clear();
	}

	void NavAgentSystem::RemoveObject(GameObject* object)
	{
		if (!object)
			return;
		s_Runtime.erase(object);
	}

	bool NavAgentSystem::SetDestination(GameObject* agent, float worldX, float worldY)
	{
		if (!agent || !agent->HasNavAgent())
			return false;
		if (!s_Objects)
		{
			BZ_CORE_ERROR("NavAgent: no scene context (press Play, or SetObjects)");
			return false;
		}

		const glm::vec3 pos = agent->GetWorldPosition();
		std::vector<PathPoint> path = Pathfinder2D::FindPath(
			*s_Objects, Camera::m_prop,
			pos.x, pos.y, worldX, worldY,
			agent->GetNavAllowDiagonal());

		if (path.empty())
		{
			BZ_INFO("NavAgent: no path for '{0}'", agent->GetName());
			s_Runtime.erase(agent);
			return false;
		}

		AgentRuntime& state = s_Runtime[agent];
		state.path = std::move(path);
		state.waypoint = 0;
		if (agent->GetNavDebugDraw())
			Pathfinder2D::DrawPath(state.path, 2.0f);
		return true;
	}

	void NavAgentSystem::Stop(GameObject* agent)
	{
		if (!agent)
			return;
		s_Runtime.erase(agent);
	}

	bool NavAgentSystem::IsMoving(GameObject* agent)
	{
		if (!agent)
			return false;
		auto it = s_Runtime.find(agent);
		return it != s_Runtime.end() && !it->second.path.empty()
			&& it->second.waypoint < (int)it->second.path.size();
	}

	int NavAgentSystem::GetWaypointCount(GameObject* agent)
	{
		if (!agent)
			return 0;
		auto it = s_Runtime.find(agent);
		return it == s_Runtime.end() ? 0 : (int)it->second.path.size();
	}

	void NavAgentSystem::Update(float dt)
	{
		if (dt <= 0.0f || s_Runtime.empty())
			return;

		const float sx = Camera::m_prop.projection[0][0];
		const float sy = Camera::m_prop.projection[1][1];
		(void)sx;
		(void)sy;

		for (auto it = s_Runtime.begin(); it != s_Runtime.end(); )
		{
			GameObject* agent = it->first;
			AgentRuntime& state = it->second;
			if (!agent || !agent->HasNavAgent() || !agent->IsActive() || state.path.empty())
			{
				it = s_Runtime.erase(it);
				continue;
			}

			if (state.waypoint >= (int)state.path.size())
			{
				it = s_Runtime.erase(it);
				continue;
			}

			const float speedPx = glm::max(agent->GetNavSpeed(), 0.0f);
			const float stopPx = glm::max(agent->GetNavStoppingDistance(), 1.0f);
			const float speedWorld = speedPx;
			const float stopWorld = stopPx;

			const PathPoint& wp = state.path[(size_t)state.waypoint];
			glm::vec3 pos = agent->GetWorldPosition();
			glm::vec2 delta(wp.x - pos.x, wp.y - pos.y);
			const float dist = glm::length(delta);

			if (dist <= stopWorld || dist < 1e-6f)
			{
				++state.waypoint;
				if (state.waypoint >= (int)state.path.size())
				{
					glm::vec3 snap(wp.x, wp.y, pos.z);
					agent->SetPosition(snap);
					it = s_Runtime.erase(it);
					continue;
				}
				++it;
				continue;
			}

			const float step = speedWorld * dt;
			if (step >= dist)
			{
				pos.x = wp.x;
				pos.y = wp.y;
				agent->SetPosition(pos);
				++state.waypoint;
				if (state.waypoint >= (int)state.path.size())
				{
					it = s_Runtime.erase(it);
					continue;
				}
			}
			else
			{
				const glm::vec2 dir = delta / dist;
				pos.x += dir.x * step;
				pos.y += dir.y * step;
				agent->SetPosition(pos);
			}
			++it;
		}
	}
}
