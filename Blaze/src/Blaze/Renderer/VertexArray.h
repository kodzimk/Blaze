#pragma once

#include"Blaze/Renderer/VertexBuffer.h"

namespace Blaze
{
	class BLAZE_API VertexArray
	{
	public:
		VertexArray();
		~VertexArray();

		void Bind();
		void UnBind();

		void AttribPointer(const VertexBuffer& buffer);
	private:
		uint32_t m_ID;
		uint32_t m_VertexBufferIndex;
	};
}

