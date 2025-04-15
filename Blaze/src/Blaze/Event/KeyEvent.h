#include"Blaze/Event/Event.h"
#include<glfw/glfw3.h>

namespace Blaze
{
	class BLAZE_API KeyEvent : public Event
	{
	public:
		inline int GetKeyCode() const { return m_KeyCode; }
		int GetCategoryFlags() const 
		{
			return EventCategoryKeyboard | EventCategoryInput;
		}
	protected:
		KeyEvent(int keyCode) :
			m_KeyCode(keyCode) {}

		int m_KeyCode;
	};

	class BLAZE_API KeyPressedEvent : public KeyEvent
	{
	public:
		KeyPressedEvent(int key,int repeatCount)
			:KeyEvent(key),m_repeatCount(repeatCount)
		{
			
		};

		EventType GetEventType() const
		{
			return EventType::KeyPressed;
		}
		const char* GetName() const
		{
			return "KeyPressed";
		}
		inline int GetRepeatCount() const { return m_repeatCount; }

		std::string ToString() const
		{
			std::stringstream ss;
			ss << "KeyPressedEvent: " << m_KeyCode << " repeatedCount: " << m_repeatCount;
			return ss.str();
		}

	protected:
		int m_repeatCount;
	};

	class BLAZE_API KeyReleasedEvent : public KeyEvent
	{
	public:
		KeyReleasedEvent(int key)
			:KeyEvent(key)
		{

		};
		
		EventType GetEventType() const 
		{
			return EventType::KeyReleased;
		}
		const char* GetName() const 
		{
			return "KeyRelease";
		}
		std::string ToString() const 
		{
			std::stringstream ss;
			ss << "KeyReleasedEvent: " << m_KeyCode;
			return ss.str();
		}
	};
}