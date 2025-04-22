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
	{
		
	}

	Texture::~Texture()
	{
		glDeleteTextures(1, &m_texture);
	}

	void Texture::Generate(std::string path)
	{
        BZ_INFO("Generating Texture from: {0}", path);
        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);
  
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
      
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
      
        int width, height, nrChannels;
      
        unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            BZ_INFO("Succefully was created!!!");
        }
        else
        {
            BZ_INFO("Failed to load!!!");
        }
        stbi_image_free(data);
	}
}