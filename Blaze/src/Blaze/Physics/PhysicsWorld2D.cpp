#include "PhysicsWorld2D.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Renderer/Camera.h"
#include <algorithm>
#include <unordered_set>

namespace Blaze
{
	PhysicsWorld2D::PhysicsWorld2D()
	{
	}

	PhysicsWorld2D::~PhysicsWorld2D()
	{
	}

	void PhysicsWorld2D::GetPixelScale(float& sx, float& sy)
	{
		sx = Camera::m_prop.projection[0][0];
		sy = Camera::m_prop.projection[1][1];
		if (glm::abs(sx) < 1e-8f) sx = 1.0f;
		if (glm::abs(sy) < 1e-8f) sy = 1.0f;
	}

	void PhysicsWorld2D::Reset()
	{
		m_PlayState.clear();
		m_Bodies.clear();
		m_ActivePairs.clear();
		m_Events.clear();
		m_ContactsByObject.clear();
	}

	std::vector<CollisionEvent> PhysicsWorld2D::ConsumeCollisionEvents()
	{
		std::vector<CollisionEvent> out;
		out.swap(m_Events);
		return out;
	}

	void PhysicsWorld2D::RemoveObject(GameObject* object)
	{
		if (!object)
			return;

		m_PlayState.erase(object);
		m_ContactsByObject.erase(object);

		for (auto it = m_ActivePairs.begin(); it != m_ActivePairs.end(); )
		{
			if (it->first == object || it->second == object)
				it = m_ActivePairs.erase(it);
			else
				++it;
		}

		for (auto it = m_Events.begin(); it != m_Events.end(); )
		{
			if (it->a == object || it->b == object)
				it = m_Events.erase(it);
			else
				++it;
		}
	}

	PhysicsWorld2D::PairKey PhysicsWorld2D::MakePair(GameObject* a, GameObject* b)
	{
		if (a < b)
			return PairKey(a, b);
		return PairKey(b, a);
	}

	void PhysicsWorld2D::UpdateCollisionEvents(const std::vector<Contact>& contacts)
	{
		std::unordered_set<PairKey, PairHash> current;
		current.reserve(contacts.size() * 2);

		for (const Contact& contact : contacts)
		{
			if (contact.a < 0 || contact.b < 0 ||
				contact.a >= (int)m_Bodies.size() || contact.b >= (int)m_Bodies.size())
				continue;

			GameObject* oa = m_Bodies[contact.a].object;
			GameObject* ob = m_Bodies[contact.b].object;
			if (!oa || !ob || oa == ob)
				continue;

			PairKey key = MakePair(oa, ob);
			current.insert(key);
			if (m_ActivePairs.find(key) == m_ActivePairs.end())
			{
				CollisionEvent ev;
				ev.type = CollisionEvent::Type::Begin;
				ev.a = key.first;
				ev.b = key.second;
				ev.isTrigger = contact.isTrigger;
				m_Events.push_back(ev);
			}
		}

		for (const PairKey& key : m_ActivePairs)
		{
			if (current.find(key) == current.end())
			{
				CollisionEvent ev;
				ev.type = CollisionEvent::Type::End;
				ev.a = key.first;
				ev.b = key.second;
				const bool triggerA = key.first && key.first->IsTrigger();
				const bool triggerB = key.second && key.second->IsTrigger();
				ev.isTrigger = triggerA || triggerB;
				m_Events.push_back(ev);
			}
		}

		m_ActivePairs.swap(current);
	}

	void PhysicsWorld2D::CacheContacts(const std::vector<Contact>& contacts)
	{
		m_ContactsByObject.clear();
		for (const Contact& contact : contacts)
		{
			if (contact.a < 0 || contact.b < 0 ||
				contact.a >= (int)m_Bodies.size() || contact.b >= (int)m_Bodies.size())
				continue;

			GameObject* oa = m_Bodies[contact.a].object;
			GameObject* ob = m_Bodies[contact.b].object;
			if (!oa || !ob || oa == ob)
				continue;

			// contact.normal points from A toward B. For each body, store the normal into that body.
			BodyContactInfo infoA;
			infoA.other = ob;
			infoA.normal = -contact.normal;
			infoA.isTrigger = contact.isTrigger;
			m_ContactsByObject[oa].push_back(infoA);

			BodyContactInfo infoB;
			infoB.other = oa;
			infoB.normal = contact.normal;
			infoB.isTrigger = contact.isTrigger;
			m_ContactsByObject[ob].push_back(infoB);
		}
	}

	glm::vec2 PhysicsWorld2D::UpDirection() const
	{
		const float len = glm::length(m_Gravity);
		if (len > 1e-6f)
			return -m_Gravity / len;
		return glm::vec2(0.0f, 1.0f);
	}

	bool PhysicsWorld2D::IsFloorNormal(const glm::vec2& n, const glm::vec2& up, float maxSlopeDegrees)
	{
		const float maxRad = glm::radians(glm::clamp(maxSlopeDegrees, 0.0f, 89.0f));
		return glm::dot(n, up) >= glm::cos(maxRad);
	}

	bool PhysicsWorld2D::IsCeilingNormal(const glm::vec2& n, const glm::vec2& up, float maxSlopeDegrees)
	{
		const float maxRad = glm::radians(glm::clamp(maxSlopeDegrees, 0.0f, 89.0f));
		return glm::dot(n, -up) >= glm::cos(maxRad);
	}

	const std::vector<BodyContactInfo>& PhysicsWorld2D::GetContacts(GameObject* object) const
	{
		auto it = m_ContactsByObject.find(object);
		if (it == m_ContactsByObject.end())
			return m_EmptyContacts;
		return it->second;
	}

	bool PhysicsWorld2D::IsGrounded(GameObject* object, float maxSlopeDegrees) const
	{
		if (!object)
			return false;
		const glm::vec2 up = UpDirection();
		for (const BodyContactInfo& c : GetContacts(object))
		{
			if (c.isTrigger)
				continue;
			if (IsFloorNormal(c.normal, up, maxSlopeDegrees))
				return true;
		}
		return false;
	}

	bool PhysicsWorld2D::IsTouchingCeiling(GameObject* object, float maxSlopeDegrees) const
	{
		if (!object)
			return false;
		const glm::vec2 up = UpDirection();
		for (const BodyContactInfo& c : GetContacts(object))
		{
			if (c.isTrigger)
				continue;
			if (IsCeilingNormal(c.normal, up, maxSlopeDegrees))
				return true;
		}
		return false;
	}

	bool PhysicsWorld2D::IsTouchingWall(GameObject* object) const
	{
		return IsTouchingWallLeft(object) || IsTouchingWallRight(object);
	}

	bool PhysicsWorld2D::IsTouchingWallLeft(GameObject* object) const
	{
		if (!object)
			return false;
		for (const BodyContactInfo& c : GetContacts(object))
		{
			if (c.isTrigger)
				continue;
			if (glm::abs(c.normal.x) >= glm::abs(c.normal.y) && c.normal.x > 0.5f)
				return true;
		}
		return false;
	}

	bool PhysicsWorld2D::IsTouchingWallRight(GameObject* object) const
	{
		if (!object)
			return false;
		for (const BodyContactInfo& c : GetContacts(object))
		{
			if (c.isTrigger)
				continue;
			if (glm::abs(c.normal.x) >= glm::abs(c.normal.y) && c.normal.x < -0.5f)
				return true;
		}
		return false;
	}

	glm::vec2 PhysicsWorld2D::GetGroundNormal(GameObject* object, float maxSlopeDegrees) const
	{
		const glm::vec2 up = UpDirection();
		glm::vec2 best = up;
		float bestDot = -2.0f;
		for (const BodyContactInfo& c : GetContacts(object))
		{
			if (c.isTrigger)
				continue;
			if (!IsFloorNormal(c.normal, up, maxSlopeDegrees))
				continue;
			const float d = glm::dot(c.normal, up);
			if (d > bestDot)
			{
				bestDot = d;
				best = c.normal;
			}
		}
		return best;
	}

	int PhysicsWorld2D::GetContactCount(GameObject* object, bool includeTriggers) const
	{
		int count = 0;
		for (const BodyContactInfo& c : GetContacts(object))
		{
			if (!includeTriggers && c.isTrigger)
				continue;
			++count;
		}
		return count;
	}

	bool PhysicsWorld2D::CheckGround(GameObject* object, const std::vector<GameObject*>& objects,
		float probeDistancePx, float maxSlopeDegrees, uint32_t layerMask) const
	{
		if (!object)
			return false;
		if (IsGrounded(object, maxSlopeDegrees))
			return true;

		float halfY = 50.0f;
		ColliderShape shape = object->GetColliderShape();
		if (object->GetCollisionType() == CollisionType::Circle)
			shape = ColliderShape::Circle;
		if (shape == ColliderShape::Circle)
			halfY = object->GetCollisionRadius();
		else
			halfY = object->GetCollisionSize().y * 0.5f;

		const glm::vec3 pos = object->GetPosition();
		const glm::vec2 up = UpDirection();
		const glm::vec2 start(pos.x - up.x * halfY * 0.92f, pos.y - up.y * halfY * 0.92f);
		const float dist = glm::max(probeDistancePx, 1.0f);
		const glm::vec2 end = start - up * dist;

		RaycastHit hit;
		PhysicsWorld2D* self = const_cast<PhysicsWorld2D*>(this);
		if (!self->Raycast(start, end, objects, hit, layerMask, object, false))
			return false;
		return IsFloorNormal(hit.normal, up, maxSlopeDegrees);
	}

	glm::vec2 PhysicsWorld2D::GetVelocity(GameObject* object) const
	{
		auto it = m_PlayState.find(object);
		if (it == m_PlayState.end())
			return glm::vec2(0.0f);
		return it->second.velocity;
	}

	void PhysicsWorld2D::SetVelocity(GameObject* object, const glm::vec2& velocity)
	{
		if (!object)
			return;

		float sx, sy;
		GetPixelScale(sx, sy);

		auto it = m_PlayState.find(object);
		if (it != m_PlayState.end())
		{
			it->second.velocity = velocity;
			return;
		}

		PlayState state;
		state.position = glm::vec2(object->GetPosition().x / sx, object->GetPosition().y / sy);
		state.velocity = velocity;
		m_PlayState[object] = state;
	}

	glm::vec2 PhysicsWorld2D::GetLinearVelocity(GameObject* object) const
	{
		float sx, sy;
		GetPixelScale(sx, sy);
		glm::vec2 v = GetVelocity(object);
		return glm::vec2(v.x * sx, v.y * sy);
	}

	void PhysicsWorld2D::SetLinearVelocity(GameObject* object, const glm::vec2& velocityPixels)
	{
		if (!object)
			return;
		float sx, sy;
		GetPixelScale(sx, sy);
		SetVelocity(object, glm::vec2(velocityPixels.x / sx, velocityPixels.y / sy));
	}

	void PhysicsWorld2D::AddLinearVelocity(GameObject* object, const glm::vec2& deltaPixels)
	{
		if (!object)
			return;
		SetLinearVelocity(object, GetLinearVelocity(object) + deltaPixels);
	}

	void PhysicsWorld2D::ApplyForce(GameObject* object, const glm::vec2& forcePixels)
	{
		if (!object)
			return;
		if (object->GetCollisionType() != CollisionType::PhysicsRigidbody2D || object->IsRigidbodyKinematic())
			return;

		float sx, sy;
		GetPixelScale(sx, sy);
		glm::vec2 forcePlay(forcePixels.x / sx, forcePixels.y / sy);

		auto it = m_PlayState.find(object);
		if (it != m_PlayState.end())
		{
			it->second.pendingForce += forcePlay;
			return;
		}

		PlayState state;
		state.position = glm::vec2(object->GetPosition().x / sx, object->GetPosition().y / sy);
		state.pendingForce = forcePlay;
		m_PlayState[object] = state;
	}

	void PhysicsWorld2D::ApplyImpulse(GameObject* object, const glm::vec2& impulsePixels)
	{
		if (!object)
			return;
		if (object->GetCollisionType() != CollisionType::PhysicsRigidbody2D || object->IsRigidbodyKinematic())
			return;

		float sx, sy;
		GetPixelScale(sx, sy);
		const float invMass = 1.0f / glm::max(object->GetRigidbodyMass(), 0.01f);
		glm::vec2 deltaPlay((impulsePixels.x / sx) * invMass, (impulsePixels.y / sy) * invMass);

		auto it = m_PlayState.find(object);
		if (it != m_PlayState.end())
		{
			it->second.velocity += deltaPlay;
			return;
		}

		PlayState state;
		state.position = glm::vec2(object->GetPosition().x / sx, object->GetPosition().y / sy);
		state.velocity = deltaPlay;
		m_PlayState[object] = state;
	}

	void PhysicsWorld2D::Step(float dt, const std::vector<GameObject*>& objects)
	{
		if (dt <= 0.0f)
			return;
		if (dt > 0.05f)
			dt = 0.05f;

		BuildBodies(objects);
		if (m_Bodies.empty())
		{
			UpdateCollisionEvents({});
			CacheContacts({});
			return;
		}

		Integrate(dt);

		std::vector<Contact> contacts;
		contacts.reserve(m_Bodies.size() * 2);
		DetectContacts(contacts);
		ResolveContacts(contacts);
		UpdateCollisionEvents(contacts);
		CacheContacts(contacts);

		StorePlayState();
	}

	void PhysicsWorld2D::BuildBodies(const std::vector<GameObject*>& objects)
	{
		m_Bodies.clear();

		float sx, sy;
		GetPixelScale(sx, sy);

		std::unordered_map<GameObject*, PlayState> live;
		live.reserve(m_PlayState.size());

		auto appendTileBodies = [&](GameObject* object)
		{
			if (!object || !object->IsTilemap() || !object->IsTileCollisionEnabled())
				return;

			const int cols = object->GetTilemapColumns();
			const int rows = object->GetTilemapRows();
			if (cols <= 0 || rows <= 0)
				return;

			const float cell = glm::max(object->GetTileCellSize(), 1.0f);
			const glm::vec2 worldScale = object->GetWorldScale();
			const float worldRot = object->GetWorldRotation();
			const glm::vec3 worldPos = object->GetWorldPosition();
			const float c = glm::cos(glm::radians(worldRot));
			const float s = glm::sin(glm::radians(worldRot));
			const glm::vec2 half(
				glm::max(cell * glm::abs(worldScale.x) * 0.5f, 0.01f),
				glm::max(cell * glm::abs(worldScale.y) * 0.5f, 0.01f));

			const int layer = object->HasCollision()
				? object->GetCollisionLayer()
				: CollisionLayer_Ground;
			const uint32_t mask = object->HasCollision()
				? object->GetCollisionMask()
				: 0xFFFFFFFFu;

			for (int y = 0; y < rows; ++y)
			{
				for (int x = 0; x < cols; ++x)
				{
					if (!object->IsSolidAt(x, y))
						continue;

					const glm::vec2 local = object->TileCenterLocal(x, y);
					glm::vec2 v(local.x * worldScale.x, local.y * worldScale.y);
					glm::vec2 r(c * v.x - s * v.y, s * v.x + c * v.y);

					Body body;
					body.object = object;
					body.shape = ShapeKind::Box;
					body.halfSize = half;
					body.radius = glm::max(half.x, half.y);
					body.isStatic = true;
					body.isTrigger = false;
					body.invMass = 0.0f;
					body.velocity = glm::vec2(0.0f);
					body.pendingForce = glm::vec2(0.0f);
					body.layer = layer;
					body.mask = mask;
					// Match other bodies: physics space = world / projection scale.
					body.position = glm::vec2(worldPos.x / sx, worldPos.y / sy) + r;
					m_Bodies.push_back(body);
				}
			}
		};

		for (GameObject* object : objects)
		{
			if (!object || !object->IsActive() || object->IsCamera() || object->IsSound() || object->IsLight())
				continue;

			if (object->IsTilemap() && object->IsTileCollisionEnabled())
			{
				appendTileBodies(object);
				continue;
			}

			if (!object->HasCollision())
				continue;

			Body body;
			body.object = object;

			CollisionType type = object->GetCollisionType();
			ColliderShape shape = object->GetColliderShape();
			if (type == CollisionType::Circle)
				shape = ColliderShape::Circle;

			body.radius = glm::max(object->GetCollisionRadius(), 0.01f);
			body.halfSize = object->GetCollisionSize() * 0.5f;
			body.halfSize.x = glm::max(body.halfSize.x, 0.01f);
			body.halfSize.y = glm::max(body.halfSize.y, 0.01f);
			body.capsuleHalfLength = 0.0f;
			body.verts.clear();
			body.isTrigger = object->IsTrigger();
			body.layer = object->GetCollisionLayer();
			body.mask = object->GetCollisionMask();

			const bool dynamic = (type == CollisionType::PhysicsRigidbody2D && !object->IsRigidbodyKinematic());
			body.isStatic = !dynamic;

			glm::vec2 editorPixel(object->GetPosition().x / sx, object->GetPosition().y / sy);
			auto previous = m_PlayState.find(object);
			glm::vec2 pendingForce = (previous != m_PlayState.end()) ? previous->second.pendingForce : glm::vec2(0.0f);
			if (dynamic)
			{
				body.gravityScale = object->GetRigidbodyGravityScale();
				body.invMass = 1.0f / glm::max(object->GetRigidbodyMass(), 0.01f);
				body.pendingForce = pendingForce;
				if (previous != m_PlayState.end())
				{
					body.position = previous->second.position;
					body.velocity = previous->second.velocity;
				}
				else
				{
					body.position = editorPixel;
					body.velocity = glm::vec2(0.0f);
				}
			}
			else
			{
				body.invMass = 0.0f;
				body.velocity = glm::vec2(0.0f);
				body.pendingForce = glm::vec2(0.0f);
				body.position = editorPixel;
			}

			if (shape == ColliderShape::Circle)
			{
				body.shape = ShapeKind::Circle;
			}
			else if (shape == ColliderShape::Capsule)
			{
				body.shape = ShapeKind::Capsule;
				const float height = object->GetCapsuleHeight();
				body.capsuleHalfLength = glm::max(height * 0.5f - body.radius, 0.0f);
				body.halfSize = glm::vec2(body.radius, body.capsuleHalfLength + body.radius);
			}
			else if (shape == ColliderShape::Polygon)
			{
				body.shape = ShapeKind::Polygon;
				const auto& local = object->GetColliderVertices();
				if (local.size() >= 3)
				{
					body.verts.reserve(local.size());
					for (const glm::vec2& v : local)
						body.verts.push_back(body.position + v);
				}
				else
				{
					BoxToVerts(body, body.verts);
					body.shape = ShapeKind::Box;
				}
			}
			else
			{
				body.shape = ShapeKind::Box;
			}

			live[object] = PlayState{ body.position, body.velocity, glm::vec2(0.0f) };
			m_Bodies.push_back(body);
		}

		m_PlayState.swap(live);
	}

	void PhysicsWorld2D::Integrate(float dt)
	{
		for (Body& body : m_Bodies)
		{
			if (body.isStatic)
				continue;

			body.velocity += m_Gravity * body.gravityScale * dt;
			body.velocity += body.pendingForce * body.invMass * dt;
			body.pendingForce = glm::vec2(0.0f);
			body.position += body.velocity * dt;
		}
	}

	bool PhysicsWorld2D::CircleCircle(const Body& a, const Body& b, Contact& out)
	{
		glm::vec2 delta = b.position - a.position;
		float distSq = glm::dot(delta, delta);
		float radius = a.radius + b.radius;
		if (distSq >= radius * radius)
			return false;

		float dist = glm::sqrt(distSq);
		if (dist > 1e-6f)
			out.normal = delta / dist;
		else
			out.normal = glm::vec2(0.0f, 1.0f);
		out.penetration = radius - dist;
		return true;
	}

	bool PhysicsWorld2D::BoxBox(const Body& a, const Body& b, Contact& out)
	{
		glm::vec2 delta = b.position - a.position;
		float overlapX = a.halfSize.x + b.halfSize.x - glm::abs(delta.x);
		float overlapY = a.halfSize.y + b.halfSize.y - glm::abs(delta.y);
		if (overlapX <= 0.0f || overlapY <= 0.0f)
			return false;

		if (overlapX < overlapY)
		{
			out.normal = glm::vec2(delta.x < 0.0f ? -1.0f : 1.0f, 0.0f);
			out.penetration = overlapX;
		}
		else
		{
			out.normal = glm::vec2(0.0f, delta.y < 0.0f ? -1.0f : 1.0f);
			out.penetration = overlapY;
		}
		return true;
	}

	bool PhysicsWorld2D::CircleBox(const Body& circle, const Body& box, Contact& out)
	{
		glm::vec2 delta = circle.position - box.position;
		glm::vec2 closest(
			glm::clamp(delta.x, -box.halfSize.x, box.halfSize.x),
			glm::clamp(delta.y, -box.halfSize.y, box.halfSize.y));

		glm::vec2 diff = delta - closest;
		float distSq = glm::dot(diff, diff);
		if (distSq >= circle.radius * circle.radius)
			return false;

		if (distSq > 1e-8f)
		{
			float dist = glm::sqrt(distSq);
			out.normal = diff / dist;
			out.penetration = circle.radius - dist;
		}
		else
		{
			float overlapX = box.halfSize.x - glm::abs(delta.x);
			float overlapY = box.halfSize.y - glm::abs(delta.y);
			if (overlapX < overlapY)
			{
				out.normal = glm::vec2(delta.x < 0.0f ? -1.0f : 1.0f, 0.0f);
				out.penetration = circle.radius + overlapX;
			}
			else
			{
				out.normal = glm::vec2(delta.y < 0.0f ? -1.0f : 1.0f, 0.0f);
				out.penetration = circle.radius + overlapY;
			}
		}
		return true;
	}

	glm::vec2 PhysicsWorld2D::ClosestPointOnSegment(const glm::vec2& p, const glm::vec2& a, const glm::vec2& b)
	{
		glm::vec2 ab = b - a;
		float lenSq = glm::dot(ab, ab);
		if (lenSq < 1e-12f)
			return a;
		float t = glm::clamp(glm::dot(p - a, ab) / lenSq, 0.0f, 1.0f);
		return a + ab * t;
	}

	void PhysicsWorld2D::CapsuleEndpoints(const Body& capsule, glm::vec2& outA, glm::vec2& outB)
	{
		outA = capsule.position + glm::vec2(0.0f, -capsule.capsuleHalfLength);
		outB = capsule.position + glm::vec2(0.0f, capsule.capsuleHalfLength);
	}

	void PhysicsWorld2D::BoxToVerts(const Body& box, std::vector<glm::vec2>& out)
	{
		out.clear();
		out.push_back(box.position + glm::vec2(-box.halfSize.x, -box.halfSize.y));
		out.push_back(box.position + glm::vec2( box.halfSize.x, -box.halfSize.y));
		out.push_back(box.position + glm::vec2( box.halfSize.x,  box.halfSize.y));
		out.push_back(box.position + glm::vec2(-box.halfSize.x,  box.halfSize.y));
	}

	static void ProjectPolygon(const std::vector<glm::vec2>& verts, const glm::vec2& axis, float& minOut, float& maxOut)
	{
		minOut = maxOut = glm::dot(verts[0], axis);
		for (size_t i = 1; i < verts.size(); ++i)
		{
			float p = glm::dot(verts[i], axis);
			minOut = glm::min(minOut, p);
			maxOut = glm::max(maxOut, p);
		}
	}

	bool PhysicsWorld2D::SatPolygons(const std::vector<glm::vec2>& a, const std::vector<glm::vec2>& b, Contact& out)
	{
		if (a.size() < 3 || b.size() < 3)
			return false;

		float bestOverlap = 1e30f;
		glm::vec2 bestAxis(0.0f, 1.0f);

		auto testEdges = [&](const std::vector<glm::vec2>& poly) -> bool
		{
			const size_t n = poly.size();
			for (size_t i = 0; i < n; ++i)
			{
				glm::vec2 edge = poly[(i + 1) % n] - poly[i];
				glm::vec2 axis(-edge.y, edge.x);
				float len = glm::length(axis);
				if (len < 1e-8f)
					continue;
				axis /= len;

				float minA, maxA, minB, maxB;
				ProjectPolygon(a, axis, minA, maxA);
				ProjectPolygon(b, axis, minB, maxB);
				float overlap = glm::min(maxA, maxB) - glm::max(minA, minB);
				if (overlap <= 0.0f)
					return false;
				if (overlap < bestOverlap)
				{
					bestOverlap = overlap;
					bestAxis = axis;
				}
			}
			return true;
		};

		if (!testEdges(a) || !testEdges(b))
			return false;

		glm::vec2 centerA(0.0f);
		glm::vec2 centerB(0.0f);
		for (const glm::vec2& v : a) centerA += v;
		for (const glm::vec2& v : b) centerB += v;
		centerA /= (float)a.size();
		centerB /= (float)b.size();
		if (glm::dot(centerB - centerA, bestAxis) < 0.0f)
			bestAxis = -bestAxis;

		out.normal = bestAxis;
		out.penetration = bestOverlap;
		return true;
	}

	bool PhysicsWorld2D::CircleCapsule(const Body& circle, const Body& capsule, Contact& out)
	{
		glm::vec2 a, b;
		CapsuleEndpoints(capsule, a, b);
		glm::vec2 closest = ClosestPointOnSegment(circle.position, a, b);
		Body probe;
		probe.position = closest;
		probe.radius = capsule.radius;
		return CircleCircle(circle, probe, out);
	}

	bool PhysicsWorld2D::CapsuleBox(const Body& capsule, const Body& box, Contact& out)
	{
		glm::vec2 a, b;
		CapsuleEndpoints(capsule, a, b);

		auto closestOnBox = [&](const glm::vec2& p)
		{
			glm::vec2 d = p - box.position;
			return box.position + glm::vec2(
				glm::clamp(d.x, -box.halfSize.x, box.halfSize.x),
				glm::clamp(d.y, -box.halfSize.y, box.halfSize.y));
		};

		glm::vec2 bestSeg = a;
		glm::vec2 bestBox = closestOnBox(a);
		float bestDistSq = glm::dot(bestSeg - bestBox, bestSeg - bestBox);

		glm::vec2 candSeg = b;
		glm::vec2 candBox = closestOnBox(b);
		float dSq = glm::dot(candSeg - candBox, candSeg - candBox);
		if (dSq < bestDistSq)
		{
			bestDistSq = dSq;
			bestSeg = candSeg;
			bestBox = candBox;
		}

		const glm::vec2 corners[4] = {
			box.position + glm::vec2(-box.halfSize.x, -box.halfSize.y),
			box.position + glm::vec2( box.halfSize.x, -box.halfSize.y),
			box.position + glm::vec2( box.halfSize.x,  box.halfSize.y),
			box.position + glm::vec2(-box.halfSize.x,  box.halfSize.y),
		};
		for (const glm::vec2& c : corners)
		{
			glm::vec2 onSeg = ClosestPointOnSegment(c, a, b);
			dSq = glm::dot(onSeg - c, onSeg - c);
			if (dSq < bestDistSq)
			{
				bestDistSq = dSq;
				bestSeg = onSeg;
				bestBox = c;
			}
		}

		Body probeCircle;
		probeCircle.position = bestSeg;
		probeCircle.radius = capsule.radius;
		Body probeBox = box;
		if (!CircleBox(probeCircle, probeBox, out))
			return false;
		// CircleBox normal points from box toward circle (= from box toward capsule axis).
		// We want normal from capsule(a) toward box(b): flip.
		out.normal = -out.normal;
		return true;
	}

	bool PhysicsWorld2D::CapsuleCapsule(const Body& a, const Body& b, Contact& out)
	{
		glm::vec2 a0, a1, b0, b1;
		CapsuleEndpoints(a, a0, a1);
		CapsuleEndpoints(b, b0, b1);

		// Sample closest between endpoints and clamped projections.
		glm::vec2 bestA = a0;
		glm::vec2 bestB = ClosestPointOnSegment(a0, b0, b1);
		float bestDistSq = glm::dot(bestA - bestB, bestA - bestB);

		auto consider = [&](const glm::vec2& pa, const glm::vec2& pb)
		{
			float d = glm::dot(pa - pb, pa - pb);
			if (d < bestDistSq)
			{
				bestDistSq = d;
				bestA = pa;
				bestB = pb;
			}
		};
		consider(a1, ClosestPointOnSegment(a1, b0, b1));
		consider(ClosestPointOnSegment(b0, a0, a1), b0);
		consider(ClosestPointOnSegment(b1, a0, a1), b1);

		Body ca, cb;
		ca.position = bestA;
		ca.radius = a.radius;
		cb.position = bestB;
		cb.radius = b.radius;
		return CircleCircle(ca, cb, out);
	}

	bool PhysicsWorld2D::PolygonPolygon(const Body& a, const Body& b, Contact& out)
	{
		return SatPolygons(a.verts, b.verts, out);
	}

	bool PhysicsWorld2D::CirclePolygon(const Body& circle, const Body& poly, Contact& out)
	{
		if (poly.verts.size() < 3)
			return false;

		const size_t n = poly.verts.size();
		float bestEdgeDist = 1e30f;
		glm::vec2 bestNormal(0.0f, 1.0f);

		glm::vec2 centroid(0.0f);
		for (const glm::vec2& v : poly.verts)
			centroid += v;
		centroid /= (float)n;

		for (size_t i = 0; i < n; ++i)
		{
			const glm::vec2& v0 = poly.verts[i];
			const glm::vec2& v1 = poly.verts[(i + 1) % n];
			glm::vec2 closest = ClosestPointOnSegment(circle.position, v0, v1);
			glm::vec2 diff = circle.position - closest;
			float distSq = glm::dot(diff, diff);
			if (distSq < bestEdgeDist * bestEdgeDist)
			{
				float dist = glm::sqrt(glm::max(distSq, 0.0f));
				bestEdgeDist = dist;
				if (dist > 1e-6f)
					bestNormal = diff / dist;
				else
				{
					glm::vec2 edge = v1 - v0;
					glm::vec2 nrm(-edge.y, edge.x);
					float len = glm::length(nrm);
					bestNormal = (len > 1e-8f) ? (nrm / len) : glm::vec2(0.0f, 1.0f);
				}
			}
		}

		if (PointInBody(poly, circle.position))
		{
			// Push circle out along nearest edge normal (outward).
			float minPen = 1e30f;
			glm::vec2 pushN(0.0f, 1.0f);
			for (size_t i = 0; i < n; ++i)
			{
				const glm::vec2& v0 = poly.verts[i];
				const glm::vec2& v1 = poly.verts[(i + 1) % n];
				glm::vec2 edge = v1 - v0;
				glm::vec2 nrm(-edge.y, edge.x);
				float len = glm::length(nrm);
				if (len < 1e-8f)
					continue;
				nrm /= len;
				if (glm::dot(v0 - centroid, nrm) < 0.0f)
					nrm = -nrm;
				float dist = glm::dot(circle.position - v0, nrm);
				float pen = circle.radius - dist;
				if (pen < minPen)
				{
					minPen = pen;
					pushN = nrm;
				}
			}
			if (minPen <= 0.0f)
				return false;
			out.normal = pushN;
			out.penetration = minPen;
			return true;
		}

		if (bestEdgeDist >= circle.radius)
			return false;

		if (glm::dot(bestNormal, circle.position - centroid) < 0.0f)
			bestNormal = -bestNormal;
		out.normal = bestNormal;
		out.penetration = circle.radius - bestEdgeDist;
		return true;
	}

	bool PhysicsWorld2D::CapsulePolygon(const Body& capsule, const Body& poly, Contact& out)
	{
		glm::vec2 a, b;
		CapsuleEndpoints(capsule, a, b);

		Body probe;
		probe.shape = ShapeKind::Circle;
		probe.radius = capsule.radius;

		Contact best;
		bool any = false;
		auto consider = [&](const glm::vec2& p)
		{
			probe.position = p;
			Contact c;
			if (CirclePolygon(probe, poly, c))
			{
				if (!any || c.penetration > best.penetration)
				{
					best = c;
					any = true;
				}
			}
		};
		consider(a);
		consider(b);
		consider(capsule.position);

		// Also sample closest point on segment to each poly vertex / edge midpoint.
		for (const glm::vec2& v : poly.verts)
			consider(ClosestPointOnSegment(v, a, b));

		if (!any)
			return false;
		out = best;
		// CirclePolygon normal points roughly poly→circle; we want capsule→poly.
		out.normal = -out.normal;
		return true;
	}

	bool PhysicsWorld2D::BoxPolygon(const Body& box, const Body& poly, Contact& out)
	{
		std::vector<glm::vec2> boxVerts;
		BoxToVerts(box, boxVerts);
		Body a;
		a.verts = std::move(boxVerts);
		return SatPolygons(a.verts, poly.verts, out);
	}

	bool PhysicsWorld2D::CollideBodies(const Body& a, const Body& b, Contact& out)
	{
		const ShapeKind sa = a.shape;
		const ShapeKind sb = b.shape;

		if (sa == ShapeKind::Circle && sb == ShapeKind::Circle)
			return CircleCircle(a, b, out);
		if (sa == ShapeKind::Box && sb == ShapeKind::Box)
			return BoxBox(a, b, out);
		if (sa == ShapeKind::Capsule && sb == ShapeKind::Capsule)
			return CapsuleCapsule(a, b, out);
		if (sa == ShapeKind::Polygon && sb == ShapeKind::Polygon)
			return PolygonPolygon(a, b, out);

		if (sa == ShapeKind::Circle && sb == ShapeKind::Box)
		{
			if (!CircleBox(a, b, out))
				return false;
			out.normal = -out.normal;
			return true;
		}
		if (sa == ShapeKind::Box && sb == ShapeKind::Circle)
			return CircleBox(b, a, out);

		if (sa == ShapeKind::Circle && sb == ShapeKind::Capsule)
		{
			if (!CircleCapsule(a, b, out))
				return false;
			out.normal = -out.normal;
			return true;
		}
		if (sa == ShapeKind::Capsule && sb == ShapeKind::Circle)
			return CircleCapsule(b, a, out);

		if (sa == ShapeKind::Capsule && sb == ShapeKind::Box)
			return CapsuleBox(a, b, out);
		if (sa == ShapeKind::Box && sb == ShapeKind::Capsule)
		{
			if (!CapsuleBox(b, a, out))
				return false;
			out.normal = -out.normal;
			return true;
		}

		if (sa == ShapeKind::Circle && sb == ShapeKind::Polygon)
		{
			if (!CirclePolygon(a, b, out))
				return false;
			out.normal = -out.normal;
			return true;
		}
		if (sa == ShapeKind::Polygon && sb == ShapeKind::Circle)
			return CirclePolygon(b, a, out);

		if (sa == ShapeKind::Box && sb == ShapeKind::Polygon)
			return BoxPolygon(a, b, out);
		if (sa == ShapeKind::Polygon && sb == ShapeKind::Box)
		{
			if (!BoxPolygon(b, a, out))
				return false;
			out.normal = -out.normal;
			return true;
		}

		if (sa == ShapeKind::Capsule && sb == ShapeKind::Polygon)
			return CapsulePolygon(a, b, out);
		if (sa == ShapeKind::Polygon && sb == ShapeKind::Capsule)
		{
			if (!CapsulePolygon(b, a, out))
				return false;
			out.normal = -out.normal;
			return true;
		}

		return false;
	}

	void PhysicsWorld2D::DetectContacts(std::vector<Contact>& contacts) const
	{
		for (size_t i = 0; i < m_Bodies.size(); ++i)
		{
			for (size_t j = i + 1; j < m_Bodies.size(); ++j)
			{
				const Body& a = m_Bodies[i];
				const Body& b = m_Bodies[j];

				const bool aHitsB = (a.mask & (1u << (b.layer & 31))) != 0;
				const bool bHitsA = (b.mask & (1u << (a.layer & 31))) != 0;
				if (!aHitsB || !bHitsA)
					continue;

				Contact contact;
				contact.a = (int)i;
				contact.b = (int)j;
				contact.isTrigger = a.isTrigger || b.isTrigger;

				if (CollideBodies(a, b, contact) && contact.penetration > 0.0f)
					contacts.push_back(contact);
			}
		}
	}

	void PhysicsWorld2D::ResolveContacts(const std::vector<Contact>& contacts)
	{
		for (const Contact& contact : contacts)
		{
			if (contact.isTrigger)
				continue;

			Body& a = m_Bodies[contact.a];
			Body& b = m_Bodies[contact.b];

			float invMassSum = a.invMass + b.invMass;
			if (invMassSum <= 0.0f)
				continue;

			glm::vec2 relative = b.velocity - a.velocity;
			float velAlongNormal = glm::dot(relative, contact.normal);
			if (velAlongNormal < 0.0f)
			{
				float j = -(1.0f + m_Restitution) * velAlongNormal / invMassSum;
				glm::vec2 impulse = j * contact.normal;
				a.velocity -= impulse * a.invMass;
				b.velocity += impulse * b.invMass;

				relative = b.velocity - a.velocity;
				glm::vec2 tangent = relative - glm::dot(relative, contact.normal) * contact.normal;
				float tangentLen = glm::length(tangent);
				if (tangentLen > 1e-6f)
				{
					tangent /= tangentLen;
					float jt = -glm::dot(relative, tangent) / invMassSum;
					float maxFriction = j * m_Friction;
					jt = glm::clamp(jt, -maxFriction, maxFriction);
					glm::vec2 frictionImpulse = jt * tangent;
					a.velocity -= frictionImpulse * a.invMass;
					b.velocity += frictionImpulse * b.invMass;
				}
			}

			float correctionMag = glm::max(contact.penetration - m_PositionalSlop, 0.0f) / invMassSum * m_PositionalPercent;
			glm::vec2 correction = correctionMag * contact.normal;
			a.position -= correction * a.invMass;
			b.position += correction * b.invMass;
		}
	}

	void PhysicsWorld2D::StorePlayState()
	{
		for (Body& body : m_Bodies)
		{
			if (!body.object)
				continue;
			m_PlayState[body.object] = PlayState{
				body.position,
				body.isStatic ? glm::vec2(0.0f) : body.velocity,
				body.isStatic ? glm::vec2(0.0f) : body.pendingForce
			};
		}
	}

	void PhysicsWorld2D::ApplyPlayTransforms(const std::vector<GameObject*>& objects) const
	{
		float sx, sy;
		GetPixelScale(sx, sy);

		for (GameObject* object : objects)
		{
			if (!object)
				continue;
			auto it = m_PlayState.find(object);
			if (it == m_PlayState.end())
				continue;
			if (object->GetCollisionType() != CollisionType::PhysicsRigidbody2D || object->IsRigidbodyKinematic())
				continue;

			glm::vec3 pos(it->second.position.x * sx, it->second.position.y * sy, object->GetPosition().z);
			object->SetPosition(pos);
		}
	}

	bool PhysicsWorld2D::RaycastBody(const Body& body, const glm::vec2& origin, const glm::vec2& dir,
		float length, float& outT, glm::vec2& outNormal)
	{
		outT = length;
		if (body.shape == ShapeKind::Circle)
		{
			const glm::vec2 f = origin - body.position;
			const float a = glm::dot(dir, dir);
			const float b = 2.0f * glm::dot(f, dir);
			const float c = glm::dot(f, f) - body.radius * body.radius;
			const float disc = b * b - 4.0f * a * c;
			if (disc < 0.0f || a < 1e-12f)
				return false;
			const float sqrtD = glm::sqrt(disc);
			float t0 = (-b - sqrtD) / (2.0f * a);
			float t1 = (-b + sqrtD) / (2.0f * a);
			float t = length + 1.0f;
			if (t0 >= 0.0f && t0 <= length)
				t = t0;
			else if (t1 >= 0.0f && t1 <= length)
				t = t1;
			else
				return false;
			outT = t;
			glm::vec2 hit = origin + dir * t;
			glm::vec2 n = hit - body.position;
			float nLen = glm::length(n);
			outNormal = (nLen > 1e-6f) ? (n / nLen) : glm::vec2(0.0f, 1.0f);
			return true;
		}

		if (body.shape == ShapeKind::Capsule)
		{
			bool hit = false;
			float bestT = length + 1.0f;
			glm::vec2 bestN(0.0f, 1.0f);

			auto rayCircle = [&](const glm::vec2& center)
			{
				Body c;
				c.shape = ShapeKind::Circle;
				c.position = center;
				c.radius = body.radius;
				float t;
				glm::vec2 n;
				if (RaycastBody(c, origin, dir, length, t, n) && t < bestT)
				{
					bestT = t;
					bestN = n;
					hit = true;
				}
			};

			glm::vec2 a, b;
			CapsuleEndpoints(body, a, b);
			rayCircle(a);
			rayCircle(b);

			// Middle AABB (excluding caps' half-circles coverage is fine as over-test).
			Body mid;
			mid.shape = ShapeKind::Box;
			mid.position = body.position;
			mid.halfSize = glm::vec2(body.radius, body.capsuleHalfLength);
			float t;
			glm::vec2 n;
			if (RaycastBody(mid, origin, dir, length, t, n) && t < bestT)
			{
				bestT = t;
				bestN = n;
				hit = true;
			}

			if (!hit)
				return false;
			outT = bestT;
			outNormal = bestN;
			return true;
		}

		if (body.shape == ShapeKind::Polygon)
		{
			if (body.verts.size() < 3)
				return false;

			bool hit = false;
			float bestT = length + 1.0f;
			glm::vec2 bestN(0.0f, 1.0f);
			const size_t n = body.verts.size();
			glm::vec2 centroid(0.0f);
			for (const glm::vec2& v : body.verts)
				centroid += v;
			centroid /= (float)n;

			for (size_t i = 0; i < n; ++i)
			{
				const glm::vec2& v0 = body.verts[i];
				const glm::vec2& v1 = body.verts[(i + 1) % n];
				glm::vec2 edge = v1 - v0;
				glm::vec2 edgeN(-edge.y, edge.x);
				float eLen = glm::length(edgeN);
				if (eLen < 1e-8f)
					continue;
				edgeN /= eLen;
				if (glm::dot(v0 - centroid, edgeN) < 0.0f)
					edgeN = -edgeN;

				// Ray-segment intersection
				glm::vec2 r = dir;
				glm::vec2 s = edge;
				float rxs = r.x * s.y - r.y * s.x;
				if (glm::abs(rxs) < 1e-8f)
					continue;
				glm::vec2 qp = v0 - origin;
				float t = (qp.x * s.y - qp.y * s.x) / rxs;
				float u = (qp.x * r.y - qp.y * r.x) / rxs;
				if (t < 0.0f || t > length || u < 0.0f || u > 1.0f)
					continue;
				if (t < bestT)
				{
					bestT = t;
					bestN = edgeN;
					hit = true;
				}
			}

			if (!hit)
				return false;
			outT = bestT;
			outNormal = bestN;
			return true;
		}

		// AABB slab test
		const glm::vec2 minB = body.position - body.halfSize;
		const glm::vec2 maxB = body.position + body.halfSize;
		float tmin = 0.0f;
		float tmax = length;
		glm::vec2 normal(0.0f, 1.0f);

		for (int axis = 0; axis < 2; ++axis)
		{
			float o = (axis == 0) ? origin.x : origin.y;
			float d = (axis == 0) ? dir.x : dir.y;
			float mn = (axis == 0) ? minB.x : minB.y;
			float mx = (axis == 0) ? maxB.x : maxB.y;
			if (glm::abs(d) < 1e-8f)
			{
				if (o < mn || o > mx)
					return false;
				continue;
			}
			float inv = 1.0f / d;
			float t1 = (mn - o) * inv;
			float t2 = (mx - o) * inv;
			float enterNormal = -1.0f;
			if (t1 > t2)
			{
				std::swap(t1, t2);
				enterNormal = 1.0f;
			}
			else
			{
				enterNormal = -1.0f;
			}
			if (t1 > tmin)
			{
				tmin = t1;
				normal = (axis == 0) ? glm::vec2(enterNormal, 0.0f) : glm::vec2(0.0f, enterNormal);
			}
			tmax = glm::min(tmax, t2);
			if (tmin > tmax)
				return false;
		}

		if (tmin < 0.0f || tmin > length)
			return false;
		outT = tmin;
		outNormal = normal;
		return true;
	}

	bool PhysicsWorld2D::PointInBody(const Body& body, const glm::vec2& point)
	{
		if (body.shape == ShapeKind::Circle)
		{
			glm::vec2 d = point - body.position;
			return glm::dot(d, d) <= body.radius * body.radius;
		}
		if (body.shape == ShapeKind::Capsule)
		{
			glm::vec2 a, b;
			CapsuleEndpoints(body, a, b);
			glm::vec2 closest = ClosestPointOnSegment(point, a, b);
			glm::vec2 d = point - closest;
			return glm::dot(d, d) <= body.radius * body.radius;
		}
		if (body.shape == ShapeKind::Polygon)
		{
			if (body.verts.size() < 3)
				return false;
			// Ray cast to +X
			bool inside = false;
			const size_t n = body.verts.size();
			for (size_t i = 0, j = n - 1; i < n; j = i++)
			{
				const glm::vec2& vi = body.verts[i];
				const glm::vec2& vj = body.verts[j];
				const bool intersect = ((vi.y > point.y) != (vj.y > point.y)) &&
					(point.x < (vj.x - vi.x) * (point.y - vi.y) / (vj.y - vi.y + 1e-12f) + vi.x);
				if (intersect)
					inside = !inside;
			}
			return inside;
		}
		return glm::abs(point.x - body.position.x) <= body.halfSize.x &&
			glm::abs(point.y - body.position.y) <= body.halfSize.y;
	}

	bool PhysicsWorld2D::CircleOverlapsBody(const Body& body, const glm::vec2& center, float radius)
	{
		Body probe;
		probe.shape = ShapeKind::Circle;
		probe.position = center;
		probe.radius = radius;
		Contact c;
		return CollideBodies(probe, body, c);
	}

	bool PhysicsWorld2D::Raycast(const glm::vec2& startPx, const glm::vec2& endPx,
		const std::vector<GameObject*>& objects, RaycastHit& out,
		uint32_t layerMask, GameObject* ignore, bool includeTriggers)
	{
		out = RaycastHit{};
		BuildBodies(objects);

		float sx, sy;
		GetPixelScale(sx, sy);
		const glm::vec2 start(startPx.x / sx, startPx.y / sy);
		const glm::vec2 end(endPx.x / sx, endPx.y / sy);
		glm::vec2 delta = end - start;
		const float length = glm::length(delta);
		if (length < 1e-6f)
			return false;
		const glm::vec2 dir = delta / length;

		float bestT = length + 1.0f;
		const Body* best = nullptr;
		glm::vec2 bestN(0.0f, 1.0f);

		for (const Body& body : m_Bodies)
		{
			if (!body.object || body.object == ignore)
				continue;
			if (!includeTriggers && body.isTrigger)
				continue;
			if ((layerMask & (1u << (body.layer & 31))) == 0)
				continue;

			float t = 0.0f;
			glm::vec2 n(0.0f, 1.0f);
			if (!RaycastBody(body, start, dir, length, t, n))
				continue;
			if (t < bestT)
			{
				bestT = t;
				best = &body;
				bestN = n;
			}
		}

		if (!best)
			return false;

		out.hit = true;
		out.object = best->object;
		out.fraction = bestT / length;
		out.distance = bestT * glm::length(glm::vec2(sx, sy)); // approximate; use axis scale per component
		{
			glm::vec2 hitPhys = start + dir * bestT;
			out.point = glm::vec2(hitPhys.x * sx, hitPhys.y * sy);
			out.distance = glm::length(out.point - startPx);
		}
		out.normal = bestN;
		return true;
	}

	std::vector<RaycastHit> PhysicsWorld2D::RaycastAll(const glm::vec2& startPx, const glm::vec2& endPx,
		const std::vector<GameObject*>& objects, uint32_t layerMask, GameObject* ignore, bool includeTriggers)
	{
		std::vector<RaycastHit> hits;
		BuildBodies(objects);

		float sx, sy;
		GetPixelScale(sx, sy);
		const glm::vec2 start(startPx.x / sx, startPx.y / sy);
		const glm::vec2 end(endPx.x / sx, endPx.y / sy);
		glm::vec2 delta = end - start;
		const float length = glm::length(delta);
		if (length < 1e-6f)
			return hits;
		const glm::vec2 dir = delta / length;

		for (const Body& body : m_Bodies)
		{
			if (!body.object || body.object == ignore)
				continue;
			if (!includeTriggers && body.isTrigger)
				continue;
			if ((layerMask & (1u << (body.layer & 31))) == 0)
				continue;

			float t = 0.0f;
			glm::vec2 n(0.0f, 1.0f);
			if (!RaycastBody(body, start, dir, length, t, n))
				continue;

			RaycastHit hit;
			hit.hit = true;
			hit.object = body.object;
			hit.fraction = t / length;
			glm::vec2 hitPhys = start + dir * t;
			hit.point = glm::vec2(hitPhys.x * sx, hitPhys.y * sy);
			hit.distance = glm::length(hit.point - startPx);
			hit.normal = n;
			hits.push_back(hit);
		}

		std::sort(hits.begin(), hits.end(),
			[](const RaycastHit& a, const RaycastHit& b) { return a.fraction < b.fraction; });
		return hits;
	}

	bool PhysicsWorld2D::OverlapPoint(const glm::vec2& pointPx, const std::vector<GameObject*>& objects,
		std::vector<GameObject*>& outObjects, uint32_t layerMask, GameObject* ignore, bool includeTriggers)
	{
		outObjects.clear();
		BuildBodies(objects);
		float sx, sy;
		GetPixelScale(sx, sy);
		const glm::vec2 point(pointPx.x / sx, pointPx.y / sy);

		std::unordered_set<GameObject*> unique;
		for (const Body& body : m_Bodies)
		{
			if (!body.object || body.object == ignore)
				continue;
			if (!includeTriggers && body.isTrigger)
				continue;
			if ((layerMask & (1u << (body.layer & 31))) == 0)
				continue;
			if (!PointInBody(body, point))
				continue;
			if (unique.insert(body.object).second)
				outObjects.push_back(body.object);
		}
		return !outObjects.empty();
	}

	bool PhysicsWorld2D::OverlapCircle(const glm::vec2& centerPx, float radiusPx,
		const std::vector<GameObject*>& objects, std::vector<GameObject*>& outObjects,
		uint32_t layerMask, GameObject* ignore, bool includeTriggers)
	{
		outObjects.clear();
		BuildBodies(objects);
		float sx, sy;
		GetPixelScale(sx, sy);
		const glm::vec2 center(centerPx.x / sx, centerPx.y / sy);
		const float radius = radiusPx / glm::max(glm::min(sx, sy), 1e-6f);

		std::unordered_set<GameObject*> unique;
		for (const Body& body : m_Bodies)
		{
			if (!body.object || body.object == ignore)
				continue;
			if (!includeTriggers && body.isTrigger)
				continue;
			if ((layerMask & (1u << (body.layer & 31))) == 0)
				continue;
			if (!CircleOverlapsBody(body, center, radius))
				continue;
			if (unique.insert(body.object).second)
				outObjects.push_back(body.object);
		}
		return !outObjects.empty();
	}
}
