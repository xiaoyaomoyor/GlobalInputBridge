//WindowsMousePoller.h
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

/** 一次鼠标按钮状态变化，仅供 Private 层使用。 */
struct FWindowsMouseButtonTransition
{
	FKey Key;
	bool bPressed = false;
};

/** 一帧鼠标轮询结果。 */
struct FWindowsMousePollResult
{
	FVector2D ScreenPosition = FVector2D::ZeroVector;
	FVector2D Delta = FVector2D::ZeroVector;

	bool bPositionValid = false;
	bool bPositionChanged = false;

	double TimestampSeconds = 0.0;

	TArray<FWindowsMouseButtonTransition, TInlineAllocator<5>> ButtonTransitions;

	void ResetFrame()
	{
		ScreenPosition = FVector2D::ZeroVector;
		Delta = FVector2D::ZeroVector;
		bPositionValid = false;
		bPositionChanged = false;
		TimestampSeconds = 0.0;
		ButtonTransitions.Reset();
	}
};

/**
 * Windows 全局鼠标轮询器。
 *
 * 只读取桌面光标位置和鼠标按钮状态。
 * 不注册 Raw Input，不安装钩子，不广播事件。
 * 只允许在 GameThread 使用。
 */
class FWindowsMousePoller final
{
public:
	FWindowsMousePoller();

	void Poll(
		FWindowsMousePollResult& OutResult,
		bool bPollPosition = true);
	void Reset();

private:
	struct FButtonState
	{
		int32 VirtualKey = 0;
		FKey Key;
		bool bDown = false;
	};

	FButtonState Buttons[5];

	FVector2D PreviousPosition = FVector2D::ZeroVector;
	bool bPreviousPositionValid = false;
};
