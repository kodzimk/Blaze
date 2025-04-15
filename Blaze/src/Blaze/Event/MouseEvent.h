#include"Blaze/Event/KeyEvent.h"
#include"Blaze/Core/Core.h"

namespace Blaze
{
	class BLAZE_API MouseMovedEvent : public Event 
	{
	public:
		MouseMovedEvent(float x, float y)
			: m_MouseX(x), m_MouseY(y) {}

		inline float GetX() const { return m_MouseX; }
		inline float GetY() const { return m_MouseY; }
		EventType GetEventType() const 
		{
			return EventType::MouseMoved;
		}
		const char* GetName() const 
		{
			return "MouseMoved";
		}
		int GetCategoryFlags() const 
		{
			return  EventCategoryMouse;
		}

		std::string ToString() const 
		{
			std::stringstream ss;
			ss << "MouseMoved X: " << m_MouseX << " Y: " << m_MouseY;
			return ss.str();
		}

	private:
		float m_MouseX, m_MouseY;
	};

	class BLAZE_API MouseScrollEvent : public Event
	{
	public:
		MouseScrollEvent(float offset)
			: m_offset(offset) {}

		inline float GetMouseOffset() const { return m_offset; }
		EventType GetEventType() const 
		{
			return EventType::MouseButtonRelease;
		}
		const char* GetName() const 
		{
			return "MouseButtonReleased";
		}
		int GetCategoryFlags() const 
		{
			return EventCategoryInput | EventCategoryMouse;
		}
		std::string ToString() const
		{
			std::stringstream ss;
			ss << "MouseScrolled:" << m_offset;
			return ss.str();
		}

	private:
		float m_offset;
	};

	class BLAZE_API MousePressedEvent : public Event
	{
	public:
		MousePressedEvent(int button)
			: m_MouseCode(button) {}

		inline int GetMouseCode() const { return m_MouseCode; }
		EventType GetEventType() const 
		{
			return EventType::MouseButtonPressed;
		}
		const char* GetName() const 
		{
			return "MouseButtonPressed";
		}
		int GetCategoryFlags() const
		{
			return EventCategoryMouseButton | EventCategoryInput;
		}
		std::string ToString() const 
		{
			std::stringstream ss;
			ss << "MousePressed: " << m_MouseCode;
			return ss.str();
		}

	private:
		int m_MouseCode;
	};

	class BLAZE_API MouseReleasedEvent : public Event
	{
	public:
		MouseReleasedEvent(int button)
			: m_MouseCode(button) {}

		inline int GetMouseCode() const { return m_MouseCode; }
		EventType GetEventType() const 
		{
			return EventType::MouseButtonRelease;
		}
		const char* GetName() const 
		{
			return "MouseButtonReleased";
		}
		int GetCategoryFlags() const 
		{
			return EventCategoryMouseButton | EventCategoryInput;
		}
		std::string ToString() const 
		{
			std::stringstream ss;
			ss << "MouseReleased: " << m_MouseCode;
			return ss.str();
		}

	private:
		int m_MouseCode;
	};
}