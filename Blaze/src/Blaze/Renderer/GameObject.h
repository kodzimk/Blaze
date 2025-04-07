#include"bzpch.h"

namespace Blaze
{
	struct CameraProp;
	class GameObject
	{
	public:
		GameObject(std::vector<float> vertices, CameraProp prop);
		virtual ~GameObject();

		void Draw(CameraProp prop);
	private:
		std::vector<float> vertices;
		unsigned int m_vaID; // vertex array ID
		unsigned int m_vboID;	// vertex buffer ID
		unsigned int m_vertexshaderID; // vertex shader ID
		unsigned int m_fragmentshaderID; // fragment shader ID
		unsigned int m_shaderProg; // shader Program
		unsigned int m_cvboId;
	};
}

