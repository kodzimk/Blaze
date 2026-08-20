#include "EnemyAISystem.h"
#include "NavAgentSystem.h"
#include "Blaze/Renderer/GameObject.h"
#include <unordered_map>
#include <cmath>

namespace Blaze
{
	const std::vector<GameObject*>* EnemyAISystem::s_Objects = nullptr;

	struct EnemyRuntime
	{
		EnemyAIState state = EnemyAIState::Patrol;
		int patrolIndex = 0;
		float waitTimer = 0.0f;
		float repathTimer = 0.0f;
		glm::vec3 homePos = glm::vec3(0.0f);
		bool homeCaptured = false;
	};

	static std::unordered_map<GameObject*, EnemyRuntime> s_Runtime;

	static GameObject* FindTarget(const std::vector<GameObject*>* objects, const std::string& tag)
	{
		if (!objects || tag.empty())
			return nullptr;
		for (GameObject* object : *objects)
		{
			if (object && object->IsActive() && object->HasTag(tag))
				return object;
		}
		return nullptr;
	}

	static float Distance2D(const glm::vec3& a, const glm::vec3& b)
	{
		const float dx = a.x - b.x;
		const float dy = a.y - b.y;
		return std::sqrt(dx * dx + dy * dy);
	}

	static void MoveDirect(GameObject* agent, float targetX, float targetY, float speedPx, float stopPx, float dt)
	{
		if (!agent || dt <= 0.0f)
			return;

		const float speed = speedPx;
		const float stop = stopPx;

		glm::vec3 pos = agent->GetWorldPosition();
		glm::vec2 delta(targetX - pos.x, targetY - pos.y);
		const float dist = glm::length(delta);
		if (dist <= stop || dist < 1e-6f)
			return;

		const float step = speed * dt;
		if (step >= dist)
		{
			pos.x = targetX;
			pos.y = targetY;
		}
		else
		{
			const glm::vec2 dir = delta / dist;
			pos.x += dir.x * step;
			pos.y += dir.y * step;
		}
		agent->SetPosition(pos);
	}

	static bool NearPoint(GameObject* agent, float x, float y, float stopPx)
	{
		return Distance2D(agent->GetWorldPosition(), glm::vec3(x, y, 0.0f)) <= stopPx;
	}

	static void GoTo(GameObject* agent, float x, float y, float speedPx)
	{
		if (!agent)
			return;
		if (agent->HasNavAgent())
		{
			agent->SetNavSpeed(speedPx);
			agent->SetNavDestination(x, y);
		}
	}

	void EnemyAISystem::SetObjects(const std::vector<GameObject*>* objects)
	{
		s_Objects = objects;
	}

	void EnemyAISystem::Clear()
	{
		s_Runtime.clear();
	}

	void EnemyAISystem::RemoveObject(GameObject* object)
	{
		if (!object)
			return;
		s_Runtime.erase(object);
	}

	EnemyAIState EnemyAISystem::GetState(GameObject* object)
	{
		if (!object)
			return EnemyAIState::Patrol;
		auto it = s_Runtime.find(object);
		return it == s_Runtime.end() ? EnemyAIState::Patrol : it->second.state;
	}

	const char* EnemyAISystem::GetStateName(GameObject* object)
	{
		switch (GetState(object))
		{
		case EnemyAIState::Chase: return "chase";
		case EnemyAIState::Return: return "return";
		default: return "patrol";
		}
	}

	void EnemyAISystem::Update(float dt)
	{
		if (dt <= 0.0f || !s_Objects)
			return;

		for (GameObject* agent : *s_Objects)
		{
			if (!agent || !agent->HasEnemyAI() || !agent->IsEnemyAIEnabled() || !agent->IsActive())
			{
				if (agent)
					s_Runtime.erase(agent);
				continue;
			}

			EnemyRuntime& rt = s_Runtime[agent];
			if (!rt.homeCaptured)
			{
				rt.homePos = agent->GetWorldPosition();
				rt.homeCaptured = true;
			}

			const float detect = glm::max(agent->GetEnemyDetectRange(), 1.0f);
			const float lose = glm::max(agent->GetEnemyLoseRange(), detect);
			const float stopPx = agent->HasNavAgent() ? agent->GetNavStoppingDistance() : 8.0f;
			const float patrolSpeed = agent->GetEnemyPatrolSpeed();
			const float chaseSpeed = agent->GetEnemyChaseSpeed();
			const bool useNav = agent->HasNavAgent();

			GameObject* target = FindTarget(s_Objects, agent->GetEnemyTargetTag());
			float distToTarget = 1.0e9f;
			if (target)
				distToTarget = Distance2D(agent->GetWorldPosition(), target->GetWorldPosition());

			if (rt.state != EnemyAIState::Chase && target && distToTarget <= detect)
			{
				rt.state = EnemyAIState::Chase;
				rt.repathTimer = 0.0f;
			}
			else if (rt.state == EnemyAIState::Chase && (!target || distToTarget > lose))
			{
				rt.state = EnemyAIState::Return;
				if (useNav)
					GoTo(agent, rt.homePos.x, rt.homePos.y, patrolSpeed);
			}

			if (rt.state == EnemyAIState::Chase && target)
			{
				rt.repathTimer -= dt;
				const glm::vec3 tp = target->GetWorldPosition();
				if (useNav)
				{
					if (rt.repathTimer <= 0.0f || !agent->IsNavMoving())
					{
						GoTo(agent, tp.x, tp.y, chaseSpeed);
						rt.repathTimer = glm::max(agent->GetEnemyRepathInterval(), 0.1f);
					}
				}
				else
				{
					MoveDirect(agent, tp.x, tp.y, chaseSpeed, stopPx, dt);
				}
				continue;
			}

			if (rt.state == EnemyAIState::Return)
			{
				if (useNav)
				{
					if (!agent->IsNavMoving() || NearPoint(agent, rt.homePos.x, rt.homePos.y, stopPx))
					{
						rt.state = EnemyAIState::Patrol;
						rt.waitTimer = agent->GetEnemyWaypointWait();
					}
				}
				else
				{
					MoveDirect(agent, rt.homePos.x, rt.homePos.y, patrolSpeed, stopPx, dt);
					if (NearPoint(agent, rt.homePos.x, rt.homePos.y, stopPx))
					{
						rt.state = EnemyAIState::Patrol;
						rt.waitTimer = agent->GetEnemyWaypointWait();
					}
				}
				continue;
			}

			const auto& points = agent->GetEnemyPatrolPoints();
			if (points.empty())
			{
				if (useNav)
					agent->StopNav();
				continue;
			}

			if (rt.patrolIndex < 0 || rt.patrolIndex >= (int)points.size())
				rt.patrolIndex = 0;

			if (rt.waitTimer > 0.0f)
			{
				rt.waitTimer -= dt;
				continue;
			}

			const glm::vec2& wp = points[(size_t)rt.patrolIndex];
			if (useNav)
			{
				if (!agent->IsNavMoving())
				{
					if (NearPoint(agent, wp.x, wp.y, stopPx * 1.5f))
					{
						rt.patrolIndex = (rt.patrolIndex + 1) % (int)points.size();
						rt.waitTimer = agent->GetEnemyWaypointWait();
					}
					else
					{
						GoTo(agent, wp.x, wp.y, patrolSpeed);
					}
				}
			}
			else
			{
				MoveDirect(agent, wp.x, wp.y, patrolSpeed, stopPx, dt);
				if (NearPoint(agent, wp.x, wp.y, stopPx))
				{
					rt.patrolIndex = (rt.patrolIndex + 1) % (int)points.size();
					rt.waitTimer = agent->GetEnemyWaypointWait();
				}
			}
		}
	}
}
