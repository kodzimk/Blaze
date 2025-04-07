#include"Blaze/Renderer/GameObject.h"
#include"Blaze/Renderer/Camera.h"

namespace Blaze {
	class Renderer
	{
	public:
		Renderer();
		virtual ~Renderer();

		void CreateObject(std::vector<float> vertices);
		void CreateObject(GameObject *object);
		void CreateCamera(float x = 0.0, float y = 0.0,double zoom = 1.0);
		void Render();

	private:
		std::vector<GameObject*> m_objects;
		Camera* m_camera;
	};
}

