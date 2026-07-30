#include "PhysicsWorld2D.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Renderer/Camera.h"
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
				m_Events.push_back(ev);
			}
		}

		m_ActivePairs.swap(current);
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
			return;
		}

		Integrate(dt);

		std::vector<Contact> contacts;
		contacts.reserve(m_Bodies.size() * 2);
		DetectContacts(contacts);
		ResolveContacts(contacts);
		UpdateCollisionEvents(contacts);

		StorePlayState();
	}

	void PhysicsWorld2D::BuildBodies(const std::vector<GameObject*>& objects)
	{
		m_Bodies.clear();

		float sx, sy;
		GetPixelScale(sx, sy);

		std::unordered_map<GameObject*, PlayState> live;
		live.reserve(m_PlayState.size());

		for (GameObject* object : objects)
		{
			if (!object || !object->HasCollision() || object->IsCamera() || object->IsSound())
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

			const bool dynamic = (type == CollisionType::PhysicsRigidbody2D && !object->IsRigidbodyKinematic());
			body.isStatic = !dynamic;

			glm::vec2 editorPixel(object->GetPosition().x / sx, object->GetPosition().y / sy);
			auto previous = m_PlayState.find(object);
			if (dynamic)
			{
				body.gravityScale = object->GetRigidbodyGravityScale();
				body.invMass = 1.0f / glm::max(object->GetRigidbodyMass(), 0.01f);
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
				body.position = editorPixel;
			}

			live[object] = PlayState{ body.position, body.velocity };
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
				// Detect static-static too so trigger/callback overlaps still fire.
				// ResolveContacts no-ops pairs with zero inverse mass.

				Contact contact;
				contact.a = (int)i;
				contact.b = (int)j;
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
				body.isStatic ? glm::vec2(0.0f) : body.velocity
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
}
