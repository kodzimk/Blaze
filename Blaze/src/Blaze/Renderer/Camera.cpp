#include "Camera.h"

namespace Blaze
{
	Camera::Camera(glm::vec3 pos, float zoom)
	{
		m_prop.pos = pos;
		m_prop.zoom = zoom;
	}

	Camera::~Camera()
	{

	}

	CameraProp Camera::GetCameraProp() const
	{
		return m_prop;
	}
}