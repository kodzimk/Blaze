#pragma once

#include"Blaze/Renderer/VertexBuffer.h"

namespace Blaze
{
	class VertexArray
	{
	public:
		VertexArray();
		~VertexArray();

		void Bind();
		void UnBind();
		void Gen();

		void AttribPointer(VertexBuffer& buffer);
	private:
		uint32_t m_ID;
		uint32_t m_VertexBufferIndex;
	};
}

