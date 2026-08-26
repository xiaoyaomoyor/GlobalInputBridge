//WindowsMousePoller.cpp
#include "Windows/WindowsMousePoller.h"

#include "HAL/PlatformTime.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

FWindowsMousePoller::FWindowsMousePoller()
{
#if PLATFORM_WINDOWS
	Buttons[0] = {VK_LBUTTON, EKeys::LeftMouseButton, false};
	Buttons[1] = {VK_RBUTTON, EKeys::RightMouseButton, false};
	Buttons[2] = {VK_MBUTTON, EKeys::MiddleMouseButton, false};
	Buttons[3] = {VK_XBUTTON1, EKeys::ThumbMouseButton, false};
	Buttons[4] = {VK_XBUTTON2, EKeys::ThumbMouseButton2, false};
#endif
}

void FWindowsMousePoller::Poll(
	FWindowsMousePollResult& OutResult,
	bool bPollPosition)
{
	check(IsInGameThread());

	OutResult.ResetFrame();
	OutResult.TimestampSeconds = FPlatformTime::Seconds();

#if PLATFORM_WINDOWS
	if (bPollPosition)
	{
		POINT CursorPosition{};

		if (GetCursorPos(&CursorPosition))
		{
			const FVector2D CurrentPosition(
				static_cast<double>(CursorPosition.x),
				static_cast<double>(CursorPosition.y));

			OutResult.ScreenPosition = CurrentPosition;
			OutResult.bPositionValid = true;

			if (!bPreviousPositionValid)
			{
				OutResult.bPositionChanged = true;
			}
			else
			{
				OutResult.Delta = CurrentPosition - PreviousPosition;
				OutResult.bPositionChanged =
					!CurrentPosition.Equals(PreviousPosition);
			}

			PreviousPosition = CurrentPosition;
			bPreviousPositionValid = true;
		}
		else
		{
			bPreviousPositionValid = false;
		}
	}
	else
	{
		PreviousPosition = FVector2D::ZeroVector;
		bPreviousPositionValid = false;
	}

	for (FButtonState& Button : Buttons)
	{
		const SHORT State = GetAsyncKeyState(Button.VirtualKey);
		const bool bCurrentlyDown = (State & 0x8000) != 0;

		if (bCurrentlyDown == Button.bDown) continue;

		Button.bDown = bCurrentlyDown;

		FWindowsMouseButtonTransition& Transition =
			OutResult.ButtonTransitions.AddDefaulted_GetRef();

		Transition.Key = Button.Key;
		Transition.bPressed = bCurrentlyDown;
	}
#endif
}

void FWindowsMousePoller::Reset()
{
	check(IsInGameThread());

	for (FButtonState& Button : Buttons)
	{
		Button.bDown = false;
	}

	PreviousPosition = FVector2D::ZeroVector;
	bPreviousPositionValid = false;
}
