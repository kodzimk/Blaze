#include "Camera.h"

namespace Blaze
{
	Camera::Camera(float x, float y, double zoom)
	{
		m_prop.pos.push_back(x);
		m_prop.pos.push_back(y);
		m_prop.zoom =zoom;
	}

	Camera::~Camera()
	{

	}

	CameraProp Camera::GetCameraProp() const
	{
		return m_prop;
	}
}