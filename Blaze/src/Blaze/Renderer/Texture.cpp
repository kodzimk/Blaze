#include "Texture.h"

#include"bzpch.h"
#include"glad/glad.h"

#define STB_IMAGE_IMPLEMENTATION
#include "Blaze/Dep/stb_image.h"

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include"Blaze/Logging/Log.h"

namespace Blaze
{
	Texture::Texture()
		: m_texture(0)
	{
	}

	Texture::~Texture()
	{
		if (m_texture != 0)
			glDeleteTextures(1, &m_texture);
	}

	void Texture::Generate(std::string path)
	{
        BZ_INFO("Generating Texture from: {0}", path);
		if (m_texture != 0)
		{
			glDeleteTextures(1, &m_texture);
			m_texture = 0;
		}

        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);
  
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);	// set texture wrapping to GL_REPEAT (default wrapping method)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      
        int width, height, nrChannels;
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);

        if (data && nrChannels == 2)
        {
            stbi_image_free(data);
            data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);
            nrChannels = 4;
        }

        if (data)
        {
            GLenum format = GL_RGB;
            if (nrChannels == 1)      format = GL_RED;
            else if (nrChannels == 4) format = GL_RGBA;

            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            glGenerateMipmap(GL_TEXTURE_2D);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        }
        else
        {
            BZ_INFO("Failed to load texture");
        }
        stbi_image_free(data);
	}
    void Texture::Bind()
    {
        glBindTexture(GL_TEXTURE_2D, m_texture);
    }

    void Texture::UnBind()
    {
        glBindTexture(GL_TEXTURE_2D, 0);
    }
}
