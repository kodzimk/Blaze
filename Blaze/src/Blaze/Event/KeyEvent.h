#include"bzpch.h"
#include<glfw/glfw3.h>

namespace Blaze
{
	class KeyPressedEvent
	{
	public:
		KeyPressedEvent(int key,bool repeated)
			:key(key) , repeated(repeated)
		{

		};

		virtual ~KeyPressedEvent() = default;

		bool isRepeated() const
		{
			return repeated;
		}

		void ToString()
		{
			BZ_INFO("Key was pressed: {0}", (char)key);
		}

	private:
		bool repeated;
		int key;
	};

	class KeyReleasedEvent
	{
	public:
		KeyReleasedEvent(int key)
			:key(key)
		{
			
		};
		virtual ~KeyReleasedEvent() = default;

		void ToString()
		{
			BZ_INFO("Key was released: {0}", (char)key);
		}

	private:
		int key;
	};

#define Is_Key_Pressed(symbol) if(GetKeyState(symbol)) BZ_INFO("Pressed",symbol);
}