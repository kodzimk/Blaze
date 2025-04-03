#include"bzpch.h"

namespace Blaze
{
	class MouseEventMoved 
	{
	public:
		   MouseEventMoved(double Xpos,double Ypos)
			   : Xpos(Xpos),Ypos(Ypos)
		   {

		   };
		   virtual ~MouseEventMoved() = default;

	private:
			double Xpos;
			double Ypos;
	};

	class MouseEventPressed
	{
	public:
		MouseEventPressed(int button,int mods)
			:button(button),mods(mods)
		{
			
		};
		virtual ~MouseEventPressed() = default;

		void ToString()
		{
			BZ_INFO("Mouse {0} was pressed", button);
		}
	private:
		int button;
		int mods;
	};

	class MouseScrollEvent
	{
	public:
		MouseScrollEvent(double Xoffset, double Yoffset)
			:Xoffset(Xoffset), Yoffset(Yoffset)
		{

		};
		virtual ~MouseScrollEvent() = default;

		void ToString()
		{
			BZ_INFO("Mouse Scrolled X Offset: {0}, Y Offset: {1} ", Xoffset,Yoffset);
		}
	private:
		double Xoffset;
		double Yoffset;
	};
}