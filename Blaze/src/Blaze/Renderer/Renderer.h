#include"Blaze/Renderer/GameObject.h"
#include"Blaze/Renderer/Camera.h"

namespace Blaze {
	class BLAZE_API Renderer
	{
	public:
		Renderer();
		virtual ~Renderer();

		void CreateObject(std::vector<float> vertices);
		void CreateObject(GameObject *object);
		void CreateCamera(float x = 0.0f, float y = 0.0f, float zoom = 1.0f);
		void Render();

	private:
		std::vector<GameObject*> m_objects;
		Camera* m_camera;
	};
}

