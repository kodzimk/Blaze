#include "Camera.h"
#include"Blaze/Core/Input.h"
#include"Blaze/Event/KeyCodes.h"
#include"Blaze/Event/MouseButtonCodes.h"

#include<gtc/type_ptr.hpp>
#include"Blaze/Core/Input.h"

namespace Blaze
{
	float Camera::zoom = 1.0f;
	CameraProp Camera::m_prop;

	Camera::Camera(glm::vec3 pos, float zoom)
	{
		Camera::zoom = zoom;
		m_prop.pos = pos;
		m_prop.projection = glm::ortho(-800.f, 800.f, -450.f, 450.f, -100.f, 100.0f);
		ScaleCamera();
	}

	Camera::~Camera()
	{
	}

	CameraProp Camera::GetCameraProp() const
	{
		return m_prop;
	}

	Camera* Camera::CreateCamera(glm::vec3 pos, float zoom)
	{
		return new Camera(pos, zoom);
	}

	void Camera::ScaleCamera()
	{
		m_prop.matrix = glm::mat4(1.0f);
		m_prop.matrix = glm::scale(m_prop.matrix, glm::vec3(zoom, zoom, 1.0f));
		m_prop.matrix = glm::translate(m_prop.matrix, -m_prop.pos);
	}

	glm::vec2 Camera::ScreenToWorld(float mouseX, float mouseY, float windowWidth, float windowHeight)
	{
		glm::vec4 ndc((2.0f * mouseX) / windowWidth - 1.0f,
			1.0f - (2.0f * mouseY) / windowHeight,
			0.0f, 1.0f);

		glm::vec4 world = glm::inverse(m_prop.matrix) * ndc;
		return glm::vec2(world.x, world.y);
	}

	void Camera::SetProjection(float x, float y, float bottom, float top, float zNear, float zFar)
	{
		m_prop.projection = glm::ortho(x, y, bottom, top, zNear, zFar);
	}

	void Camera::CameraMove()
	{
		static float prevMouseX = 0.f, prevMouseY = 0.f;
		const float keySpeed = 0.01f;

		if (Blaze::Input::IsKeyPressed(BZ_KEY_LEFT))
			TranslateCamera(glm::vec3(-keySpeed, 0.0f, 0.0f));
		if (Blaze::Input::IsKeyPressed(BZ_KEY_RIGHT))
			TranslateCamera(glm::vec3(keySpeed, 0.0f, 0.0f));
		if (Blaze::Input::IsKeyPressed(BZ_KEY_DOWN))
			TranslateCamera(glm::vec3(0.0f, -keySpeed, 0.0f));
		if (Blaze::Input::IsKeyPressed(BZ_KEY_UP))
			TranslateCamera(glm::vec3(0.0f, keySpeed, 0.0f));

		if (Blaze::Input::IsMouseButtonPressed(BZ_MOUSE_BUTTON_RIGHT))
		{
			float mx = (float)Input::GetMouseX();
			float my = (float)Input::GetMouseY();
			float dx = (mx - prevMouseX) / 500.0f;
			float dy = (prevMouseY - my) / 500.0f;
			if (dx != 0.0f || dy != 0.0f)
				TranslateCamera(glm::vec3(-dx / zoom, -dy / zoom, 0.0f));
			prevMouseX = mx;
			prevMouseY = my;
		}
		else
		{
			prevMouseX = (float)Input::GetMouseX();
			prevMouseY = (float)Input::GetMouseY();
		}
	}

	void Camera::SetCameraPos(glm::vec3 pos)
	{
		m_prop.pos = pos;
		ScaleCamera();
	}

	void Camera::TranslateCamera(glm::vec3 translate)
	{
		m_prop.pos += translate;
		ScaleCamera();
	}
}
