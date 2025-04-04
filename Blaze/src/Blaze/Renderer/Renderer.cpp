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
	}

	void Renderer::CreateObject(std::vector<float> vertices)
	{
		m_objects.push_back(new GameObject(vertices));
	}

	void Renderer::CreateObject(GameObject *object)
	{
		m_objects.push_back(object);
	}

	void Renderer::Render()
	{
		for (size_t i = 0; i < m_objects.size(); ++i)
		{
			m_objects[i]->Draw();
		}
	}
}
