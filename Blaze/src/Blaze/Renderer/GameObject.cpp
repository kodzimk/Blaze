#include "GameObject.h"
#include<glad/glad.h>
#include"Blaze/Renderer/Camera.h"
#include"Blaze/Renderer/Shader.h"
#include<gtc/type_ptr.hpp>
#include"Blaze/Logging/Log.h"
#include <cctype>
#include <algorithm>

namespace Blaze {
	Blaze::GameObject::GameObject(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, glm::vec4 color, std::string name)
		: m_Color(color),m_Name(name)
	{		
		if (vertices.size() >= 5)
		{
			m_LocalMin = glm::vec2(vertices[0], vertices[1]);
			m_LocalMax = m_LocalMin;
			for (size_t i = 0; i + 1 < vertices.size(); i += 5)
			{
				m_LocalMin.x = std::min(m_LocalMin.x, vertices[i]);
				m_LocalMin.y = std::min(m_LocalMin.y, vertices[i + 1]);
				m_LocalMax.x = std::max(m_LocalMax.x, vertices[i]);
				m_LocalMax.y = std::max(m_LocalMax.y, vertices[i + 1]);
			}
		}

		glGenBuffers(1, &m_EBO);
		m_VertexArray.Bind();

		m_VertexBuffer.Bind();
		m_VertexBuffer.SetFloat(vertices.data(), vertices.size() * sizeof(float), 5);

		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices.data(), GL_STATIC_DRAW);

		m_VertexArray.AttribPointer(m_VertexBuffer, 3, 0);	
		m_VertexArray.AttribPointer(m_VertexBuffer, 2, 3);
		m_VertexBuffer.UnBind();

		m_VertexArray.UnBind();
	}

	GameObject::~GameObject()
	{
		DetachFromHierarchy();
	}

	void GameObject::Draw(const CameraProp& prop,Shader& const m_Shader)
	{
		if (m_Type == ObjectType::Camera || m_Type == ObjectType::Sound)
			return;
		if ((m_Type == ObjectType::Circle || m_Type == ObjectType::Box) && !m_Texture.IsValid())
			return;

		m_Shader.SetUniform4fv(m_Color, "color");
		m_Shader.SetUniformMatrix4fv(m_Matrix, "object_matrix");
		m_Shader.SetUniform2fv(GetFlipbookUVScale(), "uv_scale");
		m_Shader.SetUniform2fv(GetFlipbookUVOffset(), "uv_offset");

		m_Texture.Bind();
		m_VertexArray.Bind();
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
		m_Texture.UnBind();
	}

	int GameObject::GetFlipbookTotalFrames() const
	{
		const int grid = glm::max(m_FlipColumns, 1) * glm::max(m_FlipRows, 1);
		if (m_FlipFrameCount > 0)
			return glm::clamp(m_FlipFrameCount, 1, grid);
		return grid;
	}

	void GameObject::ClampFlipbookFrame()
	{
		const int total = GetFlipbookTotalFrames();
		if (m_FlipFrame < 0)
			m_FlipFrame = 0;
		if (m_FlipFrame >= total)
			m_FlipFrame = total - 1;
	}

	glm::vec2 GameObject::GetFlipbookUVScale() const
	{
		const float cols = (float)glm::max(m_FlipColumns, 1);
		const float rows = (float)glm::max(m_FlipRows, 1);
		return glm::vec2(1.0f / cols, 1.0f / rows);
	}

	glm::vec2 GameObject::GetFlipbookUVOffset() const
	{
		const int cols = glm::max(m_FlipColumns, 1);
		const int rows = glm::max(m_FlipRows, 1);
		const int frame = glm::clamp(m_FlipFrame, 0, GetFlipbookTotalFrames() - 1);
		const int col = frame % cols;
		const int rowFromTop = frame / cols;
		return glm::vec2(
			(float)col / (float)cols,
			1.0f - ((float)(rowFromTop + 1) / (float)rows));
	}

	void GameObject::UpdateAnimation(float dt)
	{
		if (!m_FlipPlaying || dt <= 0.0f)
			return;
		if (m_Type == ObjectType::Camera || m_Type == ObjectType::Sound)
			return;

		const int total = GetFlipbookTotalFrames();
		if (total <= 1)
			return;

		const float fps = glm::max(m_FlipFps, 0.01f);
		m_FlipTime += dt;
		const float frameDuration = 1.0f / fps;
		while (m_FlipTime >= frameDuration)
		{
			m_FlipTime -= frameDuration;
			++m_FlipFrame;
			if (m_FlipFrame >= total)
			{
				if (m_FlipLoop)
					m_FlipFrame = 0;
				else
				{
					m_FlipFrame = total - 1;
					m_FlipPlaying = false;
					m_FlipTime = 0.0f;
					break;
				}
			}
		}
	}

	void GameObject::SetFlipbookColumns(int columns)
	{
		m_FlipColumns = glm::max(columns, 1);
		ClampFlipbookFrame();
	}

	void GameObject::SetFlipbookRows(int rows)
	{
		m_FlipRows = glm::max(rows, 1);
		ClampFlipbookFrame();
	}

	void GameObject::SetFlipbookFrameCount(int count)
	{
		m_FlipFrameCount = glm::max(count, 0);
		ClampFlipbookFrame();
	}

	void GameObject::SetFlipbookFps(float fps)
	{
		m_FlipFps = glm::max(fps, 0.01f);
	}

	void GameObject::SetFlipbookPlaying(bool playing)
	{
		m_FlipPlaying = playing;
		if (playing)
			m_FlipTime = 0.0f;
	}

	void GameObject::SetFlipbookLoop(bool loop)
	{
		m_FlipLoop = loop;
	}

	void GameObject::SetFlipbookFrame(int frame)
	{
		m_FlipFrame = frame;
		ClampFlipbookFrame();
		m_FlipTime = 0.0f;
	}

	void GameObject::PlayFlipbook()
	{
		m_FlipPlaying = true;
		m_FlipTime = 0.0f;
	}

	void GameObject::StopFlipbook()
	{
		m_FlipPlaying = false;
		m_FlipFrame = 0;
		m_FlipTime = 0.0f;
	}

	void GameObject::PauseFlipbook()
	{
		m_FlipPlaying = false;
	}

	glm::vec3 GameObject::GetWorldPosition() const
	{
		if (!m_Parent)
			return m_Pos;

		const glm::vec3 parentPos = m_Parent->GetWorldPosition();
		const float parentRot = m_Parent->GetWorldRotation();
		const glm::vec2 parentScale = m_Parent->GetWorldScale();

		glm::vec2 local(m_Pos.x * parentScale.x, m_Pos.y * parentScale.y);
		const float rad = glm::radians(parentRot);
		const float c = glm::cos(rad);
		const float s = glm::sin(rad);
		const glm::vec2 rotated(local.x * c - local.y * s, local.x * s + local.y * c);
		return glm::vec3(parentPos.x + rotated.x, parentPos.y + rotated.y, parentPos.z + m_Pos.z);
	}

	float GameObject::GetWorldRotation() const
	{
		return m_Parent ? (m_Parent->GetWorldRotation() + m_Rotation) : m_Rotation;
	}

	glm::vec2 GameObject::GetWorldScale() const
	{
		return m_Parent ? (m_Parent->GetWorldScale() * m_Scale) : m_Scale;
	}

	void GameObject::SetLocalPosition(const glm::vec3& pos)
	{
		m_Pos = pos;
		RebuildMatrix();
	}

	void GameObject::SetLocalRotation(float degrees)
	{
		m_Rotation = degrees;
		RebuildMatrix();
	}

	void GameObject::SetLocalScale(glm::vec2 scale)
	{
		m_Scale = scale;
		RebuildMatrix();
	}

	void GameObject::SetPosition(glm::vec3& pos)
	{
		if (!m_Parent)
		{
			m_Pos = pos;
			RebuildMatrix();
			return;
		}

		const glm::vec3 parentPos = m_Parent->GetWorldPosition();
		const float parentRot = m_Parent->GetWorldRotation();
		const glm::vec2 parentScale = m_Parent->GetWorldScale();

		glm::vec2 delta(pos.x - parentPos.x, pos.y - parentPos.y);
		const float rad = -glm::radians(parentRot);
		const float c = glm::cos(rad);
		const float s = glm::sin(rad);
		glm::vec2 unrot(delta.x * c - delta.y * s, delta.x * s + delta.y * c);
		if (glm::abs(parentScale.x) > 1e-6f) unrot.x /= parentScale.x;
		if (glm::abs(parentScale.y) > 1e-6f) unrot.y /= parentScale.y;
		m_Pos = glm::vec3(unrot.x, unrot.y, pos.z - parentPos.z);
		RebuildMatrix();
	}

	void GameObject::Translate(glm::vec3& translate)
	{
		glm::vec3 world = GetWorldPosition() + translate;
		SetPosition(world);
	}

	void GameObject::SetRotation(float degrees)
	{
		if (!m_Parent)
			m_Rotation = degrees;
		else
			m_Rotation = degrees - m_Parent->GetWorldRotation();
		RebuildMatrix();
	}

	void GameObject::Rotate(float degrees)
	{
		m_Rotation += degrees;
		RebuildMatrix();
	}

	void GameObject::SetScale(glm::vec2 scale)
	{
		if (!m_Parent)
		{
			m_Scale = scale;
		}
		else
		{
			glm::vec2 parentScale = m_Parent->GetWorldScale();
			m_Scale.x = (glm::abs(parentScale.x) > 1e-6f) ? (scale.x / parentScale.x) : scale.x;
			m_Scale.y = (glm::abs(parentScale.y) > 1e-6f) ? (scale.y / parentScale.y) : scale.y;
		}
		RebuildMatrix();
	}

	bool GameObject::WouldCreateParentCycle(GameObject* parent) const
	{
		for (GameObject* p = parent; p; p = p->m_Parent)
		{
			if (p == this)
				return true;
		}
		return false;
	}

	void GameObject::RemoveFromParentChildren()
	{
		if (!m_Parent)
			return;
		auto& siblings = m_Parent->m_Children;
		siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
		m_Parent = nullptr;
	}

	void GameObject::DetachFromHierarchy()
	{
		std::vector<GameObject*> children = m_Children;
		for (GameObject* child : children)
		{
			if (child)
				child->SetParent(nullptr, true);
		}
		RemoveFromParentChildren();
	}

	void GameObject::ClearParent(bool keepWorld)
	{
		SetParent(nullptr, keepWorld);
	}

	bool GameObject::SetParent(GameObject* parent, bool keepWorld)
	{
		if (parent == this)
			return false;
		if (parent == m_Parent)
			return true;
		if (WouldCreateParentCycle(parent))
			return false;

		glm::vec3 worldPos = GetWorldPosition();
		float worldRot = GetWorldRotation();
		glm::vec2 worldScale = GetWorldScale();

		RemoveFromParentChildren();
		m_Parent = parent;
		if (m_Parent)
			m_Parent->m_Children.push_back(this);

		if (keepWorld)
		{
			SetRotation(worldRot);
			SetScale(worldScale);
			SetPosition(worldPos);
		}
		else
		{
			RebuildMatrix();
		}
		return true;
	}

	void GameObject::SetColor(const glm::vec4& color)
	{
		m_Color = color;
	}

	void GameObject::SetColor(float r, float g, float b, float a)
	{
		m_Color = glm::vec4(r, g, b, a);
	}

	void GameObject::SetType(ObjectType type)
	{
		m_Type = type;
		if (type == ObjectType::Sound)
		{
			m_HasCollision = false;
			m_DrivesView = false;
		}
	}

	void GameObject::SetZoom(float zoom)
	{
		m_Zoom = glm::max(zoom, 0.05f);
	}

	glm::mat4 GameObject::GetCameraViewMatrix() const
	{
		const float zoom = glm::max(m_Zoom, 1e-4f);
		const glm::vec3 worldPos = GetWorldPosition();
		const float worldRot = GetWorldRotation();
		glm::mat4 view(1.0f);
		view = glm::scale(view, glm::vec3(zoom, zoom, 1.0f));
		view = glm::rotate(view, -glm::radians(worldRot), glm::vec3(0.0f, 0.0f, 1.0f));
		view = glm::translate(view, -worldPos);
		return view;
	}

	void GameObject::SetLayer(int layer)
	{
		m_Layer = layer;
	}

	static std::string TrimTag(const std::string& value)
	{
		size_t start = 0;
		while (start < value.size() && std::isspace((unsigned char)value[start]))
			++start;
		size_t end = value.size();
		while (end > start && std::isspace((unsigned char)value[end - 1]))
			--end;
		return value.substr(start, end - start);
	}

	void GameObject::SetTag(const std::string& tag)
	{
		m_Tags.clear();
		std::string trimmed = TrimTag(tag);
		if (!trimmed.empty())
			m_Tags.push_back(trimmed);
	}

	void GameObject::AddTag(const std::string& tag)
	{
		std::string trimmed = TrimTag(tag);
		if (trimmed.empty() || HasTag(trimmed))
			return;
		m_Tags.push_back(trimmed);
	}

	void GameObject::RemoveTag(const std::string& tag)
	{
		std::string trimmed = TrimTag(tag);
		if (trimmed.empty())
			return;
		m_Tags.erase(std::remove(m_Tags.begin(), m_Tags.end(), trimmed), m_Tags.end());
	}

	void GameObject::RemoveTagAt(size_t index)
	{
		if (index >= m_Tags.size())
			return;
		m_Tags.erase(m_Tags.begin() + (std::ptrdiff_t)index);
	}

	void GameObject::SetTagAt(size_t index, const std::string& tag)
	{
		if (index >= m_Tags.size())
			return;

		std::string trimmed = TrimTag(tag);
		if (trimmed.empty())
		{
			RemoveTagAt(index);
			return;
		}

		for (size_t i = 0; i < m_Tags.size(); ++i)
		{
			if (i != index && m_Tags[i] == trimmed)
				return;
		}

		m_Tags[index] = trimmed;
	}

	void GameObject::ClearTags()
	{
		m_Tags.clear();
	}

	void GameObject::SetTagsFromString(const std::string& tagsCsv)
	{
		m_Tags.clear();
		std::string token;
		for (size_t i = 0; i <= tagsCsv.size(); ++i)
		{
			char c = (i < tagsCsv.size()) ? tagsCsv[i] : ',';
			if (c == ',' || c == ';' || i == tagsCsv.size())
			{
				std::string trimmed = TrimTag(token);
				if (!trimmed.empty() && !HasTag(trimmed))
					m_Tags.push_back(trimmed);
				token.clear();
			}
			else
			{
				token.push_back(c);
			}
		}
	}

	bool GameObject::HasTag(const std::string& tag) const
	{
		std::string trimmed = TrimTag(tag);
		if (trimmed.empty())
			return false;
		for (const std::string& existing : m_Tags)
		{
			if (existing == trimmed)
				return true;
		}
		return false;
	}

	std::string GameObject::GetTag() const
	{
		return m_Tags.empty() ? std::string() : m_Tags.front();
	}

	std::string GameObject::GetTagsString() const
	{
		std::string out;
		for (size_t i = 0; i < m_Tags.size(); ++i)
		{
			if (i > 0)
				out += ", ";
			out += m_Tags[i];
		}
		return out;
	}

	void GameObject::AddCollision(CollisionType type)
	{
		if (m_Type == ObjectType::Sound || m_Type == ObjectType::Camera)
			return;
		m_HasCollision = true;
		SetCollisionType(type);
	}

	void GameObject::RemoveCollision()
	{
		m_HasCollision = false;
	}

	void GameObject::SetCollisionType(CollisionType type)
	{
		if (m_Type == ObjectType::Sound || m_Type == ObjectType::Camera)
			return;
		m_CollisionType = type;
		m_HasCollision = true;
		if (type == CollisionType::Circle)
			m_ColliderShape = ColliderShape::Circle;
		else if (type == CollisionType::Block)
			m_ColliderShape = ColliderShape::Block;
	}

	void GameObject::SetColliderShape(ColliderShape shape)
	{
		m_ColliderShape = shape;
	}

	void GameObject::SetCollisionRadius(float radius)
	{
		m_CollisionRadius = glm::max(radius, 0.01f);
	}

	void GameObject::SetCollisionSize(glm::vec2 size)
	{
		m_CollisionSize.x = glm::max(size.x, 0.01f);
		m_CollisionSize.y = glm::max(size.y, 0.01f);
	}

	void GameObject::SetRigidbodyMass(float mass)
	{
		m_RigidbodyMass = glm::max(mass, 0.01f);
	}

	void GameObject::SetRigidbodyGravityScale(float scale)
	{
		m_RigidbodyGravityScale = scale;
	}

	void GameObject::SetRigidbodyKinematic(bool kinematic)
	{
		m_RigidbodyKinematic = kinematic;
	}

	void GameObject::AddAudio()
	{
		m_HasAudio = true;
	}

	void GameObject::RemoveAudio()
	{
		m_HasAudio = false;
		m_AudioPath.clear();
	}

	void GameObject::SetAudioPath(const std::string& path)
	{
		m_AudioPath = path;
		m_HasAudio = true;
	}

	void GameObject::SetAudioVolume(float volume)
	{
		m_AudioVolume = glm::clamp(volume, 0.0f, 1.0f);
	}

	void GameObject::SetAudioLoop(bool loop)
	{
		m_AudioLoop = loop;
	}

	void GameObject::SetAudioPlayOnStart(bool playOnStart)
	{
		m_AudioPlayOnStart = playOnStart;
	}

	void GameObject::AddScript()
	{
		m_HasScript = true;
	}

	void GameObject::RemoveScript()
	{
		m_HasScript = false;
		m_ScriptPath.clear();
	}

	void GameObject::SetScriptPath(const std::string& path)
	{
		m_ScriptPath = path;
		m_HasScript = true;
	}

	void GameObject::RebuildMatrixFromWorld(const glm::vec3& worldPos, float worldRot, const glm::vec2& worldScale)
	{
		float sx = Camera::m_prop.projection[0][0];
		float sy = Camera::m_prop.projection[1][1];

		if (glm::abs(sx) < 1e-8f || glm::abs(sy) < 1e-8f)
		{
			m_Matrix = glm::translate(glm::mat4(1.0f), worldPos);
			return;
		}

		glm::mat4 m = glm::translate(glm::mat4(1.0f), worldPos);
		m = glm::scale(m, glm::vec3(sx, sy, 1.0f));
		m = glm::rotate(m, glm::radians(worldRot), glm::vec3(0.0f, 0.0f, 1.0f));
		m = glm::scale(m, glm::vec3(worldScale, 1.0f));
		m = glm::scale(m, glm::vec3(1.0f / sx, 1.0f / sy, 1.0f));
		m_Matrix = m;
	}

	void GameObject::RebuildChildrenMatrices()
	{
		for (GameObject* child : m_Children)
		{
			if (child)
				child->RebuildMatrix();
		}
	}

	void GameObject::RebuildMatrix()
	{
		RebuildMatrixFromWorld(GetWorldPosition(), GetWorldRotation(), GetWorldScale());
		RebuildChildrenMatrices();
	}

	void GameObject::SetTexture(std::string path)
	{
		m_TexturePath = path;
		m_Texture.Generate(path);
	}

	void GameObject::GetWorldAABB(const CameraProp& prop, glm::vec2& outMin, glm::vec2& outMax) const
	{
		float scaleX = prop.projection[0][0];
		float scaleY = prop.projection[1][1];
		const glm::vec3 worldPos = GetWorldPosition();
		const float worldRot = GetWorldRotation();
		const glm::vec2 worldScale = GetWorldScale();

		float c = glm::cos(glm::radians(worldRot));
		float s = glm::sin(glm::radians(worldRot));
		glm::vec2 corners[4] =
		{
			{ m_LocalMin.x, m_LocalMin.y },
			{ m_LocalMax.x, m_LocalMin.y },
			{ m_LocalMax.x, m_LocalMax.y },
			{ m_LocalMin.x, m_LocalMax.y },
		};

		glm::vec2 mn, mx;
		for (int i = 0; i < 4; ++i)
		{
			glm::vec2 v(corners[i].x * worldScale.x, corners[i].y * worldScale.y);
			glm::vec2 r(c * v.x - s * v.y, s * v.x + c * v.y);
			if (i == 0) { mn = r; mx = r; }
			else
			{
				mn.x = std::min(mn.x, r.x); mn.y = std::min(mn.y, r.y);
				mx.x = std::max(mx.x, r.x); mx.y = std::max(mx.y, r.y);
			}
		}

		outMin = glm::vec2(worldPos.x + mn.x * scaleX, worldPos.y + mn.y * scaleY);
		outMax = glm::vec2(worldPos.x + mx.x * scaleX, worldPos.y + mx.y * scaleY);
	}

	bool GameObject::IsHovered(const CameraProp& prop, const glm::vec2& worldPoint) const
	{
		if (m_Type == ObjectType::Sound || m_Type == ObjectType::Camera)
		{
			float sx = prop.projection[0][0];
			float sy = prop.projection[1][1];
			glm::vec2 pos(GetWorldPosition());
			float dx = (worldPoint.x - pos.x) / sx;
			float dy = (worldPoint.y - pos.y) / sy;
			return (dx * dx + dy * dy) <= (32.0f * 32.0f);
		}

		glm::vec2 min, max;
		GetWorldAABB(prop, min, max);

		return worldPoint.x >= min.x && worldPoint.x <= max.x &&
			   worldPoint.y >= min.y && worldPoint.y <= max.y;
	}
}
