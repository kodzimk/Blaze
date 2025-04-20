#include "Renderer.h"

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

		delete m_camera;
	}

	void Renderer::BeginScene(const char* uniform_name)
	{
		m_ShaderProgam.Use();
		m_ShaderProgam.SetUniformMatrix4fv(m_camera->GetCameraProp().matrix, uniform_name);
	}

	void Renderer::EndScene()
	{

	}

	void Renderer::CreateObject(std::vector<float> vertices,glm::vec4 color)
	{
		m_objects.push_back(new GameObject(vertices,color));
	}

	void Renderer::CreateObject(GameObject *object)
	{
		m_objects.push_back(object);
	}

	void Renderer::CreateCamera(float x, float y, float zoom)
	{
		m_camera = new Camera({ x,y ,0.0f}, zoom);
	}

	void Renderer::Render()
	{
		for (size_t i = 0; i < m_objects.size(); ++i)
		{
			m_objects[i]->Draw(m_camera->GetCameraProp(),m_ShaderProgam);
		}
	}
}
