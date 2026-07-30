#pragma once
#include"bzpch.h"
#include"Blaze/Core/Core.h"
#include"glm.hpp"

namespace Blaze
{
	class GameObject;

	struct BLAZE_API CollisionEvent
	{
		enum class Type { Begin, End };
		Type type = Type::Begin;
		GameObject* a = nullptr;
		GameObject* b = nullptr;
	};

	class BLAZE_API PhysicsWorld2D
	{
	public:
		PhysicsWorld2D();
		~PhysicsWorld2D();

		void SetGravity(const glm::vec2& gravity) { m_Gravity = gravity; }
		const glm::vec2& GetGravity() const { return m_Gravity; }

		void Reset();
		void Step(float dt, const std::vector<GameObject*>& objects);
		void RemoveObject(GameObject* object);

		void ApplyPlayTransforms(const std::vector<GameObject*>& objects) const;
		glm::vec2 GetVelocity(GameObject* object) const;
		void SetVelocity(GameObject* object, const glm::vec2& velocity);

		std::vector<CollisionEvent> ConsumeCollisionEvents();

	private:
		enum class ShapeKind { Circle, Box };

		struct Body
		{
			GameObject* object = nullptr;
			glm::vec2 position = glm::vec2(0.0f);
			glm::vec2 velocity = glm::vec2(0.0f);
			float invMass = 0.0f;
			float gravityScale = 1.0f;
			bool isStatic = true;
			ShapeKind shape = ShapeKind::Box;
			float radius = 50.0f;
			glm::vec2 halfSize = glm::vec2(50.0f);
		};

		struct PlayState
		{
			glm::vec2 position = glm::vec2(0.0f);
			glm::vec2 velocity = glm::vec2(0.0f);
		};

		struct Contact
		{
			int a = 0;
			int b = 0;
			glm::vec2 normal = glm::vec2(0.0f, 1.0f);
			float penetration = 0.0f;
		};

		using PairKey = std::pair<GameObject*, GameObject*>;
		struct PairHash
		{
			size_t operator()(const PairKey& key) const noexcept
			{
				size_t h1 = std::hash<GameObject*>{}(key.first);
				size_t h2 = std::hash<GameObject*>{}(key.second);
				return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
			}
		};

		void BuildBodies(const std::vector<GameObject*>& objects);
		void Integrate(float dt);
		void DetectContacts(std::vector<Contact>& contacts) const;
		void ResolveContacts(const std::vector<Contact>& contacts);
		void StorePlayState();
		void UpdateCollisionEvents(const std::vector<Contact>& contacts);
		static PairKey MakePair(GameObject* a, GameObject* b);

		static bool CircleCircle(const Body& a, const Body& b, Contact& out);
		static bool BoxBox(const Body& a, const Body& b, Contact& out);
		static bool CircleBox(const Body& circle, const Body& box, Contact& out);
		static void GetPixelScale(float& sx, float& sy);
	private:
		glm::vec2 m_Gravity = glm::vec2(0.0f, -1800.0f);
		std::vector<Body> m_Bodies;
		std::unordered_map<GameObject*, PlayState> m_PlayState;
		std::unordered_set<PairKey, PairHash> m_ActivePairs;
		std::vector<CollisionEvent> m_Events;

		float m_Restitution = 0.05f;
		float m_Friction = 0.25f;
		float m_PositionalPercent = 0.8f;
		float m_PositionalSlop = 0.5f;
	};
}
