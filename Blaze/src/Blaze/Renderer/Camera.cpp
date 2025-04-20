#include "Camera.h"

namespace Blaze
{
	Camera::Camera(glm::vec3 pos, float zoom)
	{
		m_prop.pos = pos;
		m_prop.zoom = zoom;
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
}