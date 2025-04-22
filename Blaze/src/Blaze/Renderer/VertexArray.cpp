#include "VertexArray.h"
#include"glad/glad.h"

namespace Blaze
{
	VertexArray::VertexArray()
		: m_VertexBufferIndex(0)
	{
		glGenVertexArrays(1, &m_ID);
	}

	VertexArray::~VertexArray()
	{
		glDeleteVertexArrays(1, &m_ID);
	}

	void VertexArray::Bind()
	{
		glBindVertexArray(m_ID);
	}

	void VertexArray::UnBind()
	{
		glBindVertexArray(0);
	}

	void VertexArray::AttribPointer(const VertexBuffer& buffer,int distance,int start)
	{
		switch (buffer.m_dataType)
		{	
			case FLOAT:
			case FLOAT2:
			case FLOAT3:
			case FLOAT4:
			{
				glEnableVertexAttribArray(m_VertexBufferIndex);
				glVertexAttribPointer(m_VertexBufferIndex, distance,
					GL_FLOAT, GL_FALSE, buffer.m_stride * sizeof(float), (void*)(sizeof(float) * start));
			
				m_VertexBufferIndex++;
				break;
		    }

			case INT:
			case INT2:
			case INT3:
			case INT4:
			{
				glEnableVertexAttribArray(m_VertexBufferIndex);
				glVertexAttribPointer(m_VertexBufferIndex, buffer.m_stride,
					GL_INT, GL_FALSE, buffer.m_stride * sizeof(GL_INT), (void*)0);

				m_VertexBufferIndex++;
				break;
			}
		}
	}
}
