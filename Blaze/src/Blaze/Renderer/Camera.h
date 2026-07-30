#pragma once
#include"bzpch.h"
#include<glm.hpp>
#include"../vendor/glm/glm/gtc/matrix_transform.hpp"

namespace Blaze {
	typedef struct CameraProp{
		glm::vec3 pos;
		glm::mat4 matrix;
		glm::mat4 projection;
	}CameraProp;

	class BLAZE_API Camera
	{
	public:
		Camera(glm::vec3 pos, float zoom);
		virtual ~Camera();

		CameraProp GetCameraProp() const;
		static Camera* CreateCamera(glm::vec3 pos = glm::vec3(0.0f, 0.0f, 1.0f),float zoom = 1.0f);
		static void ScaleCamera();

		static glm::vec2 ScreenToWorld(float mouseX, float mouseY, float windowWidth, float windowHeight);

		void SetProjection(float x, float y, float bottom, float top, float zNear, float zFar);
		void CameraMove();
		void SetCameraPos(glm::vec3 pos);
		void TranslateCamera(glm::vec3 translate);
		void FocusOn(const glm::vec3& worldPos);
		void FocusOnBounds(const glm::vec2& worldMin, const glm::vec2& worldMax);
	public:
		static CameraProp m_prop;
		static float zoom;
	};
}

