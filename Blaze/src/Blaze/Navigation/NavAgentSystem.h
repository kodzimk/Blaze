#pragma once
#include "Blaze/Core/Core.h"
#include <vector>

namespace Blaze
{
	class GameObject;

	// Runtime path-following for GameObjects with a Nav Agent component.
	class BLAZE_API NavAgentSystem
	{
	public:
		static void SetObjects(const std::vector<GameObject*>* objects);
		static void Update(float dt);
		static void Clear();
		static void RemoveObject(GameObject* object);

		static bool SetDestination(GameObject* agent, float worldX, float worldY);
		static void Stop(GameObject* agent);
		static bool IsMoving(GameObject* agent);
		static int GetWaypointCount(GameObject* agent);

	private:
		static const std::vector<GameObject*>* s_Objects;
	};
}
