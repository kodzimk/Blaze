#pragma once
#include"bzpch.h"

namespace Blaze {
	typedef struct CameraProp{
		std::vector<float> pos;
		double zoom = 1.0;
	}CameraProp;

	class Camera
	{
	public:
		Camera(float x, float y, double zoom);
		virtual ~Camera();

		CameraProp GetCameraProp() const;
	private:
		CameraProp m_prop;
	};
}

