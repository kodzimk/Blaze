#pragma once

namespace Blaze
{
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
