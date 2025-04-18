#pragma once

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

	class PropertiesWindow
	{
	public:
		PropertiesWindow();
		virtual ~PropertiesWindow();

		void OnUpdate();
		bool OnFocus() const { return onFocus; }
	private:
		bool onFocus;
	};
}
