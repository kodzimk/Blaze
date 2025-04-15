#include"Blaze/Event/MouseEvent.h"

namespace Blaze
{
	class WindowCloseEvent : public Event
	{
		WindowCloseEvent() {}

		EventType GetEventType() const override
		{
			return EventType::WindowClose;
		}
		const char* GetName() const override
		{
			return "WindowClose";
		}
		int GetCategoryFlags() const override
		{
			return  EventCategoryApplication;
		}
	};

	class WindowResizeEvent : public Event
	{
		WindowResizeEvent() {}

		EventType GetEventType() const override
		{
			return EventType::WindowResize;
		}
		const char* GetName() const override
		{
			return "WindowResize";
		}
		int GetCategoryFlags() const override
		{
			return  EventCategoryApplication;
		}
	};
}