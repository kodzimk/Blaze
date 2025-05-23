#pragma once

#include"Blaze/Core/Input.h"

namespace Blaze
{
	class WindowsInput : public Input
	{
	protected:
		virtual bool IsKeyPressedImpl(int keycode) override;
		virtual bool IsMouseButtonPressedImpl(int button) override;
		virtual float GetMouseXImpl() override;
		virtual float GetMouseYImpl() override;
		virtual float MouseScrollXImpl() override;
	};
}

