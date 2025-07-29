#include"Camera.h"
#include"Blaze/Core/Input.h"
#include"Blaze/Event/KeyCodes.h"
#include"Blaze/Event/MouseButtonCodes.h"

namespace Blaze
{
	float Camera::zoom = 1.0f;
	CameraProp Camera::m_prop;

	Camera::Camera(glm::vec3 pos, float zoom)
	{
		m_prop.pos = pos;
		m_prop.matrix = glm::mat4(1.0f);
		m_prop.matrix = glm::translate(m_prop.matrix, m_prop.pos);

		m_prop.projection = glm::ortho(-800.f, 800.f, -450.f, 450.f, -100.f, 100.0f);
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

	void Camera::ScaleCamera()
	{
		m_prop.matrix = glm::mat4(1.0f);
		m_prop.matrix = glm::scale(m_prop.matrix, glm::vec3(zoom, zoom, 0.0f));
		m_prop.matrix = glm::translate(m_prop.matrix, m_prop.pos);
	}

	void Camera::SetProjection(float x, float y, float bottom, float top, float zNear, float zFar)
	{
		m_prop.projection = glm::ortho(x, y, bottom, top, zNear, zFar);
	}

	void Camera::CameraMove()
	{
		static float x = 0.f, y = 0.f;
		static float tempX, tempY = 0.0f;
		static float camera_speed = 0.005f;
		static float div_by = 100000.f;

		if (Blaze::Input::IsKeyPressed(BZ_KEY_LEFT))
		{
			TranslateCamera(glm::vec3(camera_speed, 0.0f, 0.0f));
		}
		if (Blaze::Input::IsKeyPressed(BZ_KEY_RIGHT))
		{
			TranslateCamera(glm::vec3(-camera_speed, 0.0f, 0.0f));
		}
		if (Blaze::Input::IsKeyPressed(BZ_KEY_DOWN))
		{
			TranslateCamera(glm::vec3(0.0f, camera_speed, 0.0f));
		}
		if (Blaze::Input::IsKeyPressed(BZ_KEY_UP))
		{
			TranslateCamera(glm::vec3(0.0f, -camera_speed, 0.0f));
		}

		if (Blaze::Input::IsMouseButtonPressed(BZ_MOUSE_BUTTON_RIGHT))
		{
		
			if (tempX != Input::GetMouseX())
			{
				if (x < Input::GetMouseX())
				{

					TranslateCamera(glm::vec3((Input::GetMouseX() - x) / div_by, 0.0f, 0.0f));
				}
				else if (x > Input::GetMouseX())
				{
					TranslateCamera(glm::vec3((Input::GetMouseX() - x) / div_by, 0.0f, 0.0f));
				}
			}
			else
			{
				x = Input::GetMouseX();
			}

			if (tempY != Input::GetMouseY())
			{

				if (y < Input::GetMouseY())
				{
					TranslateCamera(glm::vec3(0.0f, -0.004f, 0.0f));
				}
				else if (y > Input::GetMouseY())
				{
					TranslateCamera(glm::vec3(0.0f, 0.004f, 0.0f));
				}
			}
			else
			{
				y = Input::GetMouseY();
			}
			tempX = Input::GetMouseX();
			tempY = Input::GetMouseY();
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