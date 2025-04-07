#include "Renderer.h"

namespace Blaze {
	Renderer::Renderer()
	{

	}

	Renderer::~Renderer()
	{
		for (size_t i = 0; i < m_objects.size(); ++i)
		{
			delete m_objects[i];
		}

		delete m_camera;
	}

	void Renderer::CreateObject(std::vector<float> vertices)
	{
		m_objects.push_back(new GameObject(vertices, m_camera->GetCameraProp()));
	}

	void Renderer::CreateObject(GameObject *object)
	{
		m_objects.push_back(object);
	}

	void Renderer::CreateCamera(float x, float y,double zoom)
	{
		m_camera = new Camera(x,y,zoom);
	}

	void Renderer::Render()
	{
		for (size_t i = 0; i < m_objects.size(); ++i)
		{
			m_objects[i]->Draw(m_camera->GetCameraProp());
		}
	}
}
