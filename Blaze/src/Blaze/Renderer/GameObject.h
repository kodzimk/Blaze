#include"bzpch.h"

namespace Blaze
{
	class GameObject
	{
	public:
		GameObject(std::vector<float> vertices);
		virtual ~GameObject();

		void Draw();
	private:
		std::vector<float> vertices;
		unsigned int m_vaID; // vertex array ID
		unsigned int m_vboID;	// vertex buffer ID
		unsigned int m_vertexshaderID; // vertex shader ID
		unsigned int m_fragmentshaderID; // fragment shader ID
		unsigned int m_shaderProg; // shader Program
	};
}

