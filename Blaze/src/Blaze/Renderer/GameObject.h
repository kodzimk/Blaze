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
		Camera
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

		inline ObjectType GetType() const { return m_Type; }
		inline bool IsCamera() const { return m_Type == ObjectType::Camera; }
		inline bool DrivesView() const { return m_DrivesView; }
		inline float GetZoom() const { return m_Zoom; }
		inline std::string GetName() const { return m_Name; }
		inline const std::string& GetTexturePath() const { return m_TexturePath; }
		inline unsigned int GetTextureID() const { return m_Texture.GetTextID(); }
		inline bool HasTexture() const { return m_Texture.IsValid(); }
		inline glm::vec3 GetPosition() const { return m_Pos; }
		inline float GetRotation() const { return m_Rotation; }
		inline glm::vec2 GetScale() const { return m_Scale; }

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
		glm::vec2 m_LocalMin = glm::vec2(0.0f);
		glm::vec2 m_LocalMax = glm::vec2(0.0f);
		std::string m_Name;
		ObjectType m_Type = ObjectType::Sprite;
		bool m_DrivesView = false;
		float m_Zoom = 1.0f;
	};
}
