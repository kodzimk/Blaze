
namespace Blaze
{
	class EventCallBack
	{
	public:
		EventCallBack() = default;
		virtual ~EventCallBack() = default;
	};

	class EventClose : public EventCallBack
	{
	public:
		EventClose() = default;
		virtual ~EventClose() = default;
	};

	class EventResize : public EventCallBack
	{
	public:
		EventResize() = default;
		virtual ~EventResize() = default;
	};
}