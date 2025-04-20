#include "Camera.h"
#include"Blaze/Core/Input.h"
#include"Blaze/Event/KeyCodes.h"
#include"Blaze/Event/MouseButtonCodes.h"

namespace Blaze
{
	float Camera::zoom = 1.0f;
	Camera::Camera(glm::vec3 pos, float zoom)
	{
		m_prop.pos = pos;
		m_prop.matrix = glm::mat4(1.0f);
		m_prop.matrix = glm::translate(m_prop.matrix, m_prop.pos);
	}

	Camera::~Camera()
	{

	}

	CameraProp Camera::GetCameraProp() const
	{
		return m_prop;
	}
	Camera* Camera::CreateCamera(glm::vec3 pos,float zoom)
	{
		return new Camera(pos,zoom);
	}
	void Camera::CameraMove()
	{
		static float x = 0.f, y = 0.f;
		if (Blaze::Input::IsKeyPressed(BZ_KEY_LEFT))
		{
			TranslateCamera(glm::vec3(0.005f, 0.0f, 0.0f));
		}
		if (Blaze::Input::IsKeyPressed(BZ_KEY_RIGHT))
		{
			TranslateCamera(glm::vec3(-0.005f, 0.0f, 0.0f));
		}
		if (Blaze::Input::IsKeyPressed(BZ_KEY_DOWN))
		{
			TranslateCamera(glm::vec3(0.0f, 0.005f, 0.0f));
		}
		if (Blaze::Input::IsKeyPressed(BZ_KEY_UP))
		{
			TranslateCamera(glm::vec3(0.0f, -0.005f, 0.0f));
		}

		if (Blaze::Input::IsMouseButtonPressed(BZ_MOUSE_BUTTON_RIGHT))
		{
			if (x == Input::GetMouseX())
			{

				TranslateCamera(glm::vec3(0.005f, 0.0f, 0.0f));
			}

			if (y == Input::GetMouseY())
			{
				TranslateCamera(glm::vec3(0.0f, 0.005f, 0.0f));
			}
		}
		else
		{
			x = Input::GetMouseX();
			y = Input::GetMouseY();
		}
	}

	void Camera::SetCameraPos(glm::vec3 pos)
	{
		this->m_prop.pos = pos;
		m_prop.matrix = glm::translate(m_prop.matrix, pos - m_prop.pos);
	}
	void Camera::TranslateCamera(glm::vec3 translate)
	{
		m_prop.pos += translate;
		m_prop.matrix = glm::translate(m_prop.matrix, translate);
	}
}