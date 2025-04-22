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

	void VertexBuffer::SetFloat(float* vertices, uint32_t size, uint32_t stride, DataType type)
	{
		m_stride = stride;
		m_dataType = type;
		glBufferData(GL_ARRAY_BUFFER, size, vertices, GL_STATIC_DRAW);
	}
}
