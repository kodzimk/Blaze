#pragma once
#include"GameObject.h"

namespace Blaze
{

	class BLAZE_API Gizmo
	{
	public:
		Gizmo(const std::vector<float>& vertices, const std::vector<unsigned int>& indices, glm::vec4 color);
		virtual ~Gizmo();

		void MoveGizmo();
		void Focus(GameObject* object);
		void UnFocus();
		const bool GetVisibility() { return m_Visible; }

		void Draw(const CameraProp& prop, Shader& const m_Shader);
		void SetPosition(glm::vec3& pos);
		void Translate(glm::vec3& translate);
		void SetTexture(std::string path);

	public:
		inline static Gizmo& Get() { return *s_Instance; }
		static Gizmo* s_Instance;

	private:
		bool m_Visible;
		std::vector<GameObject*> m_ReferredObjects;
		
	protected:
		VertexArray m_VertexArray;
		VertexBuffer m_VertexBuffer;
		unsigned int m_EBO;
		Texture m_Texture;
		glm::vec4 m_Color;
		glm::mat4 m_Matrix = glm::mat4(1.0f);
		glm::vec3 m_Pos = glm::vec3(0.0f);
	};


}

