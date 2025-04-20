#include"Blaze/Renderer/GameObject.h"
#include"Blaze/Renderer/Camera.h"
#include"Blaze/Renderer/Shader.h"

namespace Blaze {
	class BLAZE_API Renderer
	{
	public:
		Renderer(const char* vertexSource,const char* fragmentSource);
		virtual ~Renderer();

		void BeginScene(const char* uniform_matrix_name);
		void EndScene();

		void CreateObject(std::vector<float> vertices,glm::vec4 color = glm::vec4(0.0f, 1.0f, 0.0f, 1.0f));
		void CreateObject(GameObject *object);
		void CreateCamera(float x = 0.0f, float y = 0.0f, float zoom = 1.0f);
		void Render();

	private:
		Shader m_ShaderProgam;
		std::vector<GameObject*> m_objects;
		Camera* m_camera;
	};
}

