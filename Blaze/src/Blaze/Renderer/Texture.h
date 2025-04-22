#pragma once

#include"bzpch.h"
namespace Blaze
{
	class Texture
	{
	public:
		Texture();
		virtual ~Texture();

		void Generate(std::string path);
		inline unsigned int GetTextID() const { return m_texture; }
	private:
		unsigned int m_texture;
	};
}

