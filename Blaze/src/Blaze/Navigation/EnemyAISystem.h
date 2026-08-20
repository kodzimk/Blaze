#pragma once
#include "Blaze/Core/Core.h"
#include <string>
#include <vector>

namespace Blaze
{
	class GameObject;

	enum class EnemyAIState
	{
		Patrol = 0,
		Chase,
		Return
	};

	class BLAZE_API EnemyAISystem
	{
	public:
		static void SetObjects(const std::vector<GameObject*>* objects);
		static void Update(float dt);
		static void Clear();
		static void RemoveObject(GameObject* object);

		static EnemyAIState GetState(GameObject* object);
		static const char* GetStateName(GameObject* object);

	private:
		static const std::vector<GameObject*>* s_Objects;
	};
}
