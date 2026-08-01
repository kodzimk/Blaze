#include "Lighting2D.h"
#include "Blaze/Renderer/GameObject.h"
#include "Blaze/Renderer/Shader.h"

namespace Blaze
{
	glm::vec3 Lighting2D::s_Ambient = glm::vec3(1.0f);
	bool Lighting2D::s_Enabled = true;

	void Lighting2D::SetAmbient(const glm::vec3& ambient)
	{
		s_Ambient = glm::max(ambient, glm::vec3(0.0f));
	}

	void Lighting2D::SetAmbient(float r, float g, float b)
	{
		SetAmbient(glm::vec3(r, g, b));
	}

	glm::vec3 Lighting2D::GetAmbient()
	{
		return s_Ambient;
	}

	void Lighting2D::SetEnabled(bool enabled)
	{
		s_Enabled = enabled;
	}

	bool Lighting2D::IsEnabled()
	{
		return s_Enabled;
	}

	void Lighting2D::Upload(Shader& shader, const std::vector<GameObject*>& objects)
	{
		shader.SetUniform1i(s_Enabled ? 1 : 0, "u_LightingEnabled");

		glm::vec2 positions[MaxLights] = {};
		glm::vec3 colors[MaxLights] = {};
		glm::vec2 directions[MaxLights] = {};
		float radii[MaxLights] = {};
		float intensities[MaxLights] = {};
		float types[MaxLights] = {};
		float innerCos[MaxLights] = {};
		float outerCos[MaxLights] = {};
		int count = 0;

		if (s_Enabled)
		{
			for (GameObject* object : objects)
			{
				if (!object || !object->IsLight() || !object->IsActive() || !object->IsVisible() || !object->IsLightEnabled())
					continue;
				if (count >= MaxLights)
					break;

				const glm::vec3 pos = object->GetWorldPosition();
				const glm::vec4 color = object->GetColor();
				positions[count] = glm::vec2(pos.x, pos.y);
				colors[count] = glm::vec3(color.r, color.g, color.b);
				radii[count] = glm::max(object->GetLightRadius(), 1.0f);
				intensities[count] = glm::max(object->GetLightIntensity(), 0.0f);

				const bool spot = object->GetLightKind() == LightKind::Spot;
				types[count] = spot ? 1.0f : 0.0f;
				directions[count] = object->GetLightDirection();

				float outer = glm::clamp(object->GetLightOuterAngle(), 0.5f, 179.0f);
				float inner = glm::clamp(object->GetLightInnerAngle(), 0.5f, outer);
				innerCos[count] = glm::cos(glm::radians(inner));
				outerCos[count] = glm::cos(glm::radians(outer));
				++count;
			}
		}

		shader.SetUniform3fv(s_Ambient, "u_Ambient");
		shader.SetUniform1i(count, "u_LightCount");
		shader.SetUniform2fvArray(positions, MaxLights, "u_LightPos");
		shader.SetUniform3fvArray(colors, MaxLights, "u_LightColor");
		shader.SetUniform1fvArray(radii, MaxLights, "u_LightRadius");
		shader.SetUniform1fvArray(intensities, MaxLights, "u_LightIntensity");
		shader.SetUniform1fvArray(types, MaxLights, "u_LightType");
		shader.SetUniform2fvArray(directions, MaxLights, "u_LightDir");
		shader.SetUniform1fvArray(innerCos, MaxLights, "u_LightInnerCos");
		shader.SetUniform1fvArray(outerCos, MaxLights, "u_LightOuterCos");
	}
}
