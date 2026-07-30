#pragma once
#include"bzpch.h"
#include"Blaze/Renderer/VertexArray.h"
#include"Blaze/Renderer/Texture.h"
#include"glm.hpp"

namespace Blaze
{
	struct CameraProp;
	class Shader;

	enum class ObjectType
	{
		Sprite = 0,
		Empty,
		Camera,
		Circle,
		Box,
		Sound
	};

	enum class CollisionType
	{
		Circle = 0,
		Block,
		PhysicsRigidbody2D
	};

	enum class ColliderShape
	{
		Circle = 0,
		Block
	};

	class BLAZE_API GameObject
	{
	public:
		GameObject(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, glm::vec4 color,std::string name);
		virtual ~GameObject();

		void Draw(const CameraProp& prop, Shader& const m_Shader);
		void SetPosition(glm::vec3& pos);
		void Translate(glm::vec3& translate);
		void SetRotation(float degrees);
		void Rotate(float degrees);
		void SetScale(glm::vec2 scale);
		void SetTexture(std::string path);
		void SetName(const std::string& name) { m_Name = name; }
		void SetType(ObjectType type);
		void SetDrivesView(bool drives) { m_DrivesView = drives; }
		void SetZoom(float zoom);
		void SetLayer(int layer);

		void AddCollision(CollisionType type = CollisionType::Block);
		void RemoveCollision();
		void SetCollisionType(CollisionType type);
		void SetColliderShape(ColliderShape shape);
		void SetCollisionRadius(float radius);
		void SetCollisionSize(glm::vec2 size);
		void SetRigidbodyMass(float mass);
		void SetRigidbodyGravityScale(float scale);
		void SetRigidbodyKinematic(bool kinematic);

		void AddAudio();
		void RemoveAudio();
		void SetAudioPath(const std::string& path);
		void SetAudioVolume(float volume);
		void SetAudioLoop(bool loop);
		void SetAudioPlayOnStart(bool playOnStart);

		void AddScript();
		void RemoveScript();
		void SetScriptPath(const std::string& path);

		inline ObjectType GetType() const { return m_Type; }
		inline bool IsCamera() const { return m_Type == ObjectType::Camera; }
		inline bool IsSound() const { return m_Type == ObjectType::Sound; }
		inline bool DrivesView() const { return m_DrivesView; }
		inline float GetZoom() const { return m_Zoom; }
		inline std::string GetName() const { return m_Name; }
		inline const std::string& GetTexturePath() const { return m_TexturePath; }
		inline unsigned int GetTextureID() const { return m_Texture.GetTextID(); }
		inline bool HasTexture() const { return m_Texture.IsValid(); }
		inline glm::vec3 GetPosition() const { return m_Pos; }
		inline float GetRotation() const { return m_Rotation; }
		inline glm::vec2 GetScale() const { return m_Scale; }
		inline int GetLayer() const { return m_Layer; }

		inline bool HasCollision() const { return m_HasCollision; }
		inline CollisionType GetCollisionType() const { return m_CollisionType; }
		inline ColliderShape GetColliderShape() const { return m_ColliderShape; }
		inline float GetCollisionRadius() const { return m_CollisionRadius; }
		inline glm::vec2 GetCollisionSize() const { return m_CollisionSize; }
		inline float GetRigidbodyMass() const { return m_RigidbodyMass; }
		inline float GetRigidbodyGravityScale() const { return m_RigidbodyGravityScale; }
		inline bool IsRigidbodyKinematic() const { return m_RigidbodyKinematic; }

		inline bool HasAudio() const { return m_HasAudio; }
		inline const std::string& GetAudioPath() const { return m_AudioPath; }
		inline float GetAudioVolume() const { return m_AudioVolume; }
		inline bool GetAudioLoop() const { return m_AudioLoop; }
		inline bool GetAudioPlayOnStart() const { return m_AudioPlayOnStart; }

		inline bool HasScript() const { return m_HasScript; }
		inline const std::string& GetScriptPath() const { return m_ScriptPath; }

		void GetWorldAABB(const CameraProp& prop, glm::vec2& outMin, glm::vec2& outMax) const;
		bool IsHovered(const CameraProp& prop, const glm::vec2& worldPoint) const;
	private:
		void RebuildMatrix();
	private:
		VertexArray m_VertexArray;
		VertexBuffer m_VertexBuffer;
		unsigned int m_EBO;
		Texture m_Texture;
		std::string m_TexturePath;
		glm::vec4 m_Color;
		glm::mat4 m_Matrix = glm::mat4(1.0f);
		glm::vec3 m_Pos = glm::vec3(0.0f);
		float m_Rotation = 0.0f;
		glm::vec2 m_Scale = glm::vec2(1.0f);
		int m_Layer = 0;
		glm::vec2 m_LocalMin = glm::vec2(0.0f);
		glm::vec2 m_LocalMax = glm::vec2(0.0f);
		std::string m_Name;
		ObjectType m_Type = ObjectType::Sprite;
		bool m_DrivesView = false;
		float m_Zoom = 1.0f;

		bool m_HasCollision = false;
		CollisionType m_CollisionType = CollisionType::Block;
		ColliderShape m_ColliderShape = ColliderShape::Block;
		float m_CollisionRadius = 50.0f;
		glm::vec2 m_CollisionSize = glm::vec2(100.0f, 100.0f);
		float m_RigidbodyMass = 1.0f;
		float m_RigidbodyGravityScale = 1.0f;
		bool m_RigidbodyKinematic = false;

		bool m_HasAudio = false;
		std::string m_AudioPath;
		float m_AudioVolume = 1.0f;
		bool m_AudioLoop = false;
		bool m_AudioPlayOnStart = true;

		bool m_HasScript = false;
		std::string m_ScriptPath;
	};
}
