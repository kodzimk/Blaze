#pragma once
#include"bzpch.h"
#include<glm.hpp>

namespace Blaze {
	typedef struct CameraProp{
		glm::vec3 pos;
		float zoom = 1.0;
	}CameraProp;

	class Camera
	{
	public:
		Camera(glm::vec3 pos, float zoom);
		virtual ~Camera();

		CameraProp GetCameraProp() const;
	private:
		CameraProp m_prop;
	};
}

