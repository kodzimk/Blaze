#include"Blaze/Core/Core.h"
#include"bzpch.h"

namespace Blaze
{
	enum class EventType
	{
		None,
		WindowClose, WindowResize, WindowFocus, WindowLostFocus, WindowMoved,
		KeyPressed, KeyReleased,
		MouseButtonPressed, MouseButtonRelease, MouseMoved, MouseScrolled,
	};

	enum EventCategory
	{
		None = 0,
		EventCategoryApplication = BIT(0),
		EventCategoryInput = BIT(1),
		EventCategoryKeyboard = BIT(2),
		EventCategoryMouse = BIT(3),
		EventCategoryMouseButton = BIT(4)
	};

	class BLAZE_API Event
	{
		friend class EventDispatcher;
	 public:
		 virtual EventType GetEventType() const = 0;
		 virtual const char* GetName() const = 0;
		 virtual int GetCategoryFlags() const = 0;
		 virtual std::string ToString() const { return GetName(); }
		 inline bool IsInCategory(EventCategory category)
		 {
			 return GetCategoryFlags() & category;
		 }

	protected:
		bool m_Handled = false;
	};

	class EventDispatcher
	{
		template<typename T>
		using EventFn = std::function<bool(T&)>;
	   public:
		   EventDispatcher(Event& event)
			   : m_Event(event)
		   {
		   }

		   // F will be deduced by the compiler
		   template<typename T, typename F>
		   bool Dispatch(const F& func)
		   {
			   if (m_Event.GetEventType() == T::GetStaticType())
			   {
				   m_Event.Handled |= func(static_cast<T&>(m_Event));
				   return true;
			   }
			   return false;
		   }

	private:
		Event& m_Event;
	};

}
