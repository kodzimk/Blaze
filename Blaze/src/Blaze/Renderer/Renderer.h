#include"Blaze/Renderer/GameObject.h"
#include"Blaze/Renderer/Shader.h"
#include"Blaze/Core/Core.h"

namespace Blaze {
	class Camera;
	class BLAZE_API Renderer
	{
	public:
		Renderer(const char* vertexSource,const char* fragmentSource);
		virtual ~Renderer();

		void BeginScene(const Camera& camera,const char* uniform_matrix_name);
		void BeginScene(const glm::mat4& viewMatrix, const char* uniform_matrix_name);
		void EndScene();
		void Update(float dt);
		 
		void CreateObject(const std::vector<float>& vertices, const std::vector<unsigned int>& indices,glm::vec4 color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
		void CreateObject(GameObject *object);
		bool DestroyObject(GameObject* object);
		void Render(const Camera& camera);

		static void OnWindowResize(uint32_t width, uint32_t height);
		GameObject& GetGameObject(std::string name) { return *GetGameObjectByName(name); }
		const std::vector<GameObject*>& GetObjects() const { return m_objects; }

	private:
		GameObject* GetGameObjectByName(std::string name);
	private:
		Shader m_ShaderProgam;
		std::vector<GameObject*> m_objects;
	};
}

