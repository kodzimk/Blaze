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
			if (!object || object->IsCamera() || object->IsSound())
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
			else if (type == CollisionType::Block)
				shape = ColliderShape::Block;

			body.shape = (shape == ColliderShape::Circle) ? ShapeKind::Circle : ShapeKind::Box;
			body.radius = glm::max(object->GetCollisionRadius(), 0.01f);
			body.halfSize = object->GetCollisionSize() * 0.5f;
			body.halfSize.x = glm::max(body.halfSize.x, 0.01f);
			body.halfSize.y = glm::max(body.halfSize.y, 0.01f);
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
				bool hit = false;

				if (a.shape == ShapeKind::Circle && b.shape == ShapeKind::Circle)
					hit = CircleCircle(a, b, contact);
				else if (a.shape == ShapeKind::Box && b.shape == ShapeKind::Box)
					hit = BoxBox(a, b, contact);
				else if (a.shape == ShapeKind::Circle && b.shape == ShapeKind::Box)
				{
					hit = CircleBox(a, b, contact);
					contact.normal = -contact.normal;
				}
				else
					hit = CircleBox(b, a, contact);

				if (hit && contact.penetration > 0.0f)
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
			float enterNormal = (axis == 0) ? -1.0f : -1.0f;
			float exitSign = 1.0f;
			if (t1 > t2)
			{
				std::swap(t1, t2);
				enterNormal = 1.0f;
			}
			else
			{
				enterNormal = -1.0f;
			}
			(void)exitSign;
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
		if (body.shape == ShapeKind::Circle)
			return CircleCircle(probe, body, c);
		return CircleBox(probe, body, c);
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
