#include "Renderer.h"
#include"Blaze/Renderer/Camera.h"
#include"Blaze/Renderer/Lighting2D.h"
#include"Blaze/Core/Window.h"
#include"Blaze/Audio/AudioEngine.h"
#include"Blaze/Scripting/ScriptEngine.h"
#include"Blaze/Renderer/ParticleSystem2D.h"
#include"Blaze/Navigation/NavAgentSystem.h"
#include"glad/glad.h"
#include <algorithm>
#include <vector>

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
			AudioEngine::RemoveObject(m_objects[i]);
			ScriptEngine::RemoveObject(m_objects[i]);
			ParticleSystem2D::RemoveObject(m_objects[i]);
			NavAgentSystem::RemoveObject(m_objects[i]);
			delete m_objects[i];
		}
	}

	void Renderer::BeginScene(const Camera& camera,const char* uniform_name)
	{
		m_ShaderProgam.Use();
		
		m_ShaderProgam.SetUniformMatrix4fv(camera.GetCameraProp().matrix, uniform_name);
		m_ShaderProgam.SetUniformMatrix4fv(camera.GetCameraProp().projection, "ortho");
		Lighting2D::Upload(m_ShaderProgam, m_objects);
	}

	void Renderer::BeginScene(const glm::mat4& viewMatrix, const char* uniform_name)
	{
		m_ShaderProgam.Use();

		m_ShaderProgam.SetUniformMatrix4fv(viewMatrix, uniform_name);
		m_ShaderProgam.SetUniformMatrix4fv(Camera::m_prop.projection, "ortho");
		Lighting2D::Upload(m_ShaderProgam, m_objects);
	}

	void Renderer::EndScene()
	{

	}

	void Renderer::Update(float dt)
	{
		for (GameObject* object : m_objects)
		{
			if (object && object->IsActive())
				object->UpdateAnimation(dt);
		}
	}

	void Renderer::CreateObject(const std::vector<float>& vertices, const std::vector<unsigned int>& indices,glm::vec4 color)
	{
		m_objects.push_back(new GameObject(vertices,indices,color,"Object" + std::to_string(m_objects.size() + 1)));
	}

	void Renderer::CreateObject(GameObject *object)
	{
		m_objects.push_back(object);
	}

	bool Renderer::DestroyObject(GameObject* object)
	{
		if (!object)
			return false;

		for (auto it = m_objects.begin(); it != m_objects.end(); ++it)
		{
			if (*it == object)
			{
				AudioEngine::RemoveObject(object);
				ScriptEngine::RemoveObject(object);
				ParticleSystem2D::RemoveObject(object);
				NavAgentSystem::RemoveObject(object);
				delete *it;
				m_objects.erase(it);
				return true;
			}
		}
		return false;
	}

	void Renderer::Render(const Camera& camera)
	{
		std::vector<GameObject*> sorted = m_objects;
		std::stable_sort(sorted.begin(), sorted.end(),
			[](GameObject* a, GameObject* b)
			{
				const int la = a ? a->GetLayer() : 0;
				const int lb = b ? b->GetLayer() : 0;
				return la < lb;
			});

		for (GameObject* object : sorted)
		{
			if (object)
				object->Draw(camera.GetCameraProp(), m_ShaderProgam);
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
