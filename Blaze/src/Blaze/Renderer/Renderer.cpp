#include "Renderer.h"
#include"Blaze/Renderer/Camera.h"
#include"Blaze/Core/Window.h"
#include"glad/glad.h"

namespace Blaze {
	Renderer::Renderer(const char* vertexSource,const char* fragmentSource)
	{
		m_ShaderProgam.CreateProgram(vertexSource, fragmentSource);
		m_ShaderProgam.LinkProgram();
	}

	Renderer::~Renderer()
	{
		for (size_t i = 0; i < m_objects.size(); ++i)
		{
			delete m_objects[i];
		}
	}

	void Renderer::BeginScene(const Camera& camera,const char* uniform_name)
	{
		m_ShaderProgam.Use();
		
		m_ShaderProgam.SetUniformMatrix4fv(camera.GetCameraProp().matrix, uniform_name);
		m_ShaderProgam.SetUniformMatrix4fv(camera.GetCameraProp().projection, "ortho");
	}

	void Renderer::EndScene()
	{

	}

	void Renderer::CreateObject(const std::vector<float>& vertices, const std::vector<unsigned int>& indices,glm::vec4 color)
	{
		m_objects.push_back(new GameObject(vertices,indices,color,"Object" + std::to_string(m_objects.size() + 1)));
	}

	void Renderer::CreateObject(GameObject *object)
	{
		m_objects.push_back(object);
	}

	void Renderer::Render(const Camera& camera)
	{
		for (size_t i = 0; i < m_objects.size(); ++i)
		{
			m_objects[i]->Draw(camera.GetCameraProp(), m_ShaderProgam);
		}
	}
	void Renderer::OnWindowResize(uint32_t width, uint32_t height)
	{
		glViewport(0, 0, width, height);
	}
	GameObject* Renderer::GetGameObjectByName(std::string name)
	{
		for (size_t i = 0; i < m_objects.size(); i++)
		{
			if (name == m_objects[i]->GetName()) {
				return m_objects[i];
			}
		}

		return nullptr;
	}
}
