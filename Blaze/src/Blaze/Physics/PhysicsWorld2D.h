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
		bool isTrigger = false; // true if either collider is a trigger (overlap only)
	};

	struct BLAZE_API RaycastHit
	{
		bool hit = false;
		GameObject* object = nullptr;
		glm::vec2 point = glm::vec2(0.0f);   // pixel / world units
		glm::vec2 normal = glm::vec2(0.0f, 1.0f);
		float fraction = 1.0f;               // 0..1 along start→end
		float distance = 0.0f;               // pixels
	};

	// One contact relative to a queried body (normal points into that body / away from the other).
	struct BLAZE_API BodyContactInfo
	{
		GameObject* other = nullptr;
		glm::vec2 normal = glm::vec2(0.0f, 1.0f);
		bool isTrigger = false;
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

		// Pixel-space helpers for scripts (match SetLocation / Translate units).
		glm::vec2 GetLinearVelocity(GameObject* object) const;
		void SetLinearVelocity(GameObject* object, const glm::vec2& velocityPixels);
		void AddLinearVelocity(GameObject* object, const glm::vec2& deltaPixels);
		void ApplyForce(GameObject* object, const glm::vec2& forcePixels);
		void ApplyImpulse(GameObject* object, const glm::vec2& impulsePixels);

		std::vector<CollisionEvent> ConsumeCollisionEvents();

		// Character helpers (use contacts from the last Step). maxSlopeDegrees = steepest walkable floor.
		bool IsGrounded(GameObject* object, float maxSlopeDegrees = 50.0f) const;
		bool IsTouchingCeiling(GameObject* object, float maxSlopeDegrees = 50.0f) const;
		bool IsTouchingWall(GameObject* object) const;
		bool IsTouchingWallLeft(GameObject* object) const;
		bool IsTouchingWallRight(GameObject* object) const;
		glm::vec2 GetGroundNormal(GameObject* object, float maxSlopeDegrees = 50.0f) const;
		int GetContactCount(GameObject* object, bool includeTriggers = false) const;
		const std::vector<BodyContactInfo>& GetContacts(GameObject* object) const;
		// Extra feet probe: short downward ray from collider bottom (pixels).
		bool CheckGround(GameObject* object, const std::vector<GameObject*>& objects,
			float probeDistancePx = 6.0f, float maxSlopeDegrees = 50.0f,
			uint32_t layerMask = 0xFFFFFFFFu) const;

		// Queries use pixel/world units (same as SetLocation). layerMask bits = (1u << layer).
		bool Raycast(const glm::vec2& startPx, const glm::vec2& endPx,
			const std::vector<GameObject*>& objects, RaycastHit& out,
			uint32_t layerMask = 0xFFFFFFFFu, GameObject* ignore = nullptr,
			bool includeTriggers = false);
		std::vector<RaycastHit> RaycastAll(const glm::vec2& startPx, const glm::vec2& endPx,
			const std::vector<GameObject*>& objects,
			uint32_t layerMask = 0xFFFFFFFFu, GameObject* ignore = nullptr,
			bool includeTriggers = false);
		bool OverlapPoint(const glm::vec2& pointPx, const std::vector<GameObject*>& objects,
			std::vector<GameObject*>& outObjects,
			uint32_t layerMask = 0xFFFFFFFFu, GameObject* ignore = nullptr,
			bool includeTriggers = true);
		bool OverlapCircle(const glm::vec2& centerPx, float radiusPx,
			const std::vector<GameObject*>& objects, std::vector<GameObject*>& outObjects,
			uint32_t layerMask = 0xFFFFFFFFu, GameObject* ignore = nullptr,
			bool includeTriggers = true);

	private:
		enum class ShapeKind { Circle, Box };

		struct Body
		{
			GameObject* object = nullptr;
			glm::vec2 position = glm::vec2(0.0f);
			glm::vec2 velocity = glm::vec2(0.0f);
			glm::vec2 pendingForce = glm::vec2(0.0f);
			float invMass = 0.0f;
			float gravityScale = 1.0f;
			bool isStatic = true;
			bool isTrigger = false;
			int layer = 0;
			uint32_t mask = 0xFFFFFFFFu;
			ShapeKind shape = ShapeKind::Box;
			float radius = 50.0f;
			glm::vec2 halfSize = glm::vec2(50.0f);
		};

		struct PlayState
		{
			glm::vec2 position = glm::vec2(0.0f);
			glm::vec2 velocity = glm::vec2(0.0f);
			glm::vec2 pendingForce = glm::vec2(0.0f);
		};

		struct Contact
		{
			int a = 0;
			int b = 0;
			glm::vec2 normal = glm::vec2(0.0f, 1.0f);
			float penetration = 0.0f;
			bool isTrigger = false;
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
		void CacheContacts(const std::vector<Contact>& contacts);
		static PairKey MakePair(GameObject* a, GameObject* b);
		glm::vec2 UpDirection() const;
		static bool IsFloorNormal(const glm::vec2& n, const glm::vec2& up, float maxSlopeDegrees);
		static bool IsCeilingNormal(const glm::vec2& n, const glm::vec2& up, float maxSlopeDegrees);

		static bool CircleCircle(const Body& a, const Body& b, Contact& out);
		static bool BoxBox(const Body& a, const Body& b, Contact& out);
		static bool CircleBox(const Body& circle, const Body& box, Contact& out);
		static void GetPixelScale(float& sx, float& sy);

		static bool RaycastBody(const Body& body, const glm::vec2& origin, const glm::vec2& dir,
			float length, float& outT, glm::vec2& outNormal);
		static bool PointInBody(const Body& body, const glm::vec2& point);
		static bool CircleOverlapsBody(const Body& body, const glm::vec2& center, float radius);
	private:
		glm::vec2 m_Gravity = glm::vec2(0.0f, -1800.0f);
		std::vector<Body> m_Bodies;
		std::unordered_map<GameObject*, PlayState> m_PlayState;
		std::unordered_set<PairKey, PairHash> m_ActivePairs;
		std::vector<CollisionEvent> m_Events;
		std::unordered_map<GameObject*, std::vector<BodyContactInfo>> m_ContactsByObject;
		std::vector<BodyContactInfo> m_EmptyContacts;

		float m_Restitution = 0.05f;
		float m_Friction = 0.25f;
		float m_PositionalPercent = 0.8f;
		float m_PositionalSlop = 0.5f;
	};
}
