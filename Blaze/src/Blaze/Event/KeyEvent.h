#include"bzpch.h"

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
}