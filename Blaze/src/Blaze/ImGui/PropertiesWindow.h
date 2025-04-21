#pragma once
#include"Blaze/Core/Core.h"

namespace Blaze
{
	class StartWindow
	{
	public:
		StartWindow();
		virtual ~StartWindow();

		void OnUpdate();
		bool OnFocus() const { return onFocus; }
	private:
		bool onFocus;
	};

	class BLAZE_API PropertiesWindow
	{
	public:
		PropertiesWindow();
		virtual ~PropertiesWindow();

		void OnUpdate();
		bool OnFocus() const { return onFocus; }
		static void SetSize(float width, float height);
	private:
		bool onFocus;
	public:
		static float width;
		static float height;
	};
}
