#include"Blaze/Renderer/GameObject.h"

namespace Blaze {
	class Renderer
	{
	public:
		Renderer();
		virtual ~Renderer();

		void CreateObject(std::vector<float> vertices);
		void CreateObject(GameObject *object);
		void Render();

	private:
		std::vector<GameObject*> m_objects;
	};
}

