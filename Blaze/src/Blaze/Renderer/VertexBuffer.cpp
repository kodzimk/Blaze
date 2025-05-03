#include "VertexBuffer.h"
#include"glad/glad.h"

namespace Blaze
{
	VertexBuffer::VertexBuffer()
	{
		glGenBuffers(1, &m_ID);
	}

	VertexBuffer::~VertexBuffer()
	{
		glDeleteBuffers(1, &m_ID);
	}

	void VertexBuffer::Bind()
	{		
		glBindBuffer(GL_ARRAY_BUFFER, m_ID);
	}

	void VertexBuffer::UnBind()
	{
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}

	void VertexBuffer::SetData(const void* data, uint32_t size)
	{
		glBindBuffer(GL_ARRAY_BUFFER, m_ID);
		glBufferSubData(GL_ARRAY_BUFFER, 0, size, data);
	}

	void VertexBuffer::SetFloat(const float* vertices, uint32_t size, uint32_t stride)
	{
		m_stride = stride;
		m_dataType = FLOAT;
		glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
	
	}

	void VertexBuffer::SetUInt(const unsigned int* vertices, uint32_t size, uint32_t stride)
	{
		m_stride = stride;
		m_dataType = INT;
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
	}


}
