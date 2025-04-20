#pragma once
#include"bzpch.h"

namespace Blaze
{
	enum DataType
	{
		NONE = 0,
		INT,
		INT2,
        INT3,
		INT4,
		FLOAT,
		FLOAT2,
		FLOAT3,
		FLOAT4,
	};

	class VertexBuffer
	{
	public:
		VertexBuffer();
		~VertexBuffer();

		void Bind();
		void UnBind();
		void Gen();

		void SetData(const void* data, uint32_t size);
		void SetFloat(float* data, uint32_t size,uint32_t stride);
	private:
		uint32_t m_ID;

	public:
		uint32_t m_stride;
		DataType m_dataType;
	};
}

