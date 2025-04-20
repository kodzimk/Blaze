#pragma once
#include"bzpch.h"
#include<glm.hpp>
#include"../vendor/glm/glm/gtc/matrix_transform.hpp"

namespace Blaze {
	typedef struct CameraProp{
		glm::vec3 pos;
		float zoom = 1.0;
		glm::mat4 matrix;
	}CameraProp;

	class  Camera
	{
	public:
		Camera(glm::vec3 pos, float zoom);
		virtual ~Camera();

		CameraProp GetCameraProp() const;
	private:
		CameraProp m_prop;
	};
}

