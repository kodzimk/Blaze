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
	}

	void VertexArray::Bind()
	{
		glBindVertexArray(m_ID);
	}

	void VertexArray::UnBind()
	{
		glBindVertexArray(0);
	}

	void VertexArray::AttribPointer(const VertexBuffer& buffer)
	{
		switch (buffer.m_dataType)
		{	
			case FLOAT:
			case FLOAT2:
			case FLOAT3:
			case FLOAT4:
			{
				glEnableVertexAttribArray(m_VertexBufferIndex);
				glVertexAttribPointer(m_VertexBufferIndex, buffer.m_stride,
					GL_FLOAT, GL_FALSE, buffer.m_stride * sizeof(GL_FLOAT), (void*)0);
			
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
