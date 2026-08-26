//InputStateManager.cpp
#include "Input/InputStateManager.h"

bool FInputStateManager::ProcessKeyEvent(
	const FKey& Key,
	bool bPressed,
	int64 DeviceId,
	double TimestampSeconds,
	FGlobalKeyEvent& OutEvent)
{
	check(IsInGameThread());

	OutEvent = FGlobalKeyEvent();

	if (!Key.IsValid())
	{
		return false;
	}

	bool bRepeat = false;
	bool bAggregateStateChanged = false;

	if (bPressed)
	{
		TSet<FKey>& PressedKeys = DevicePressedKeys.FindOrAdd(DeviceId);
		const bool bAlreadyPressedByDevice = PressedKeys.Contains(Key);

		if (bAlreadyPressedByDevice)
		{
			// Windows 长按自动重复。
			// 不增加聚合计数，但仍然向上层报告事件。
			bRepeat = true;
		}
		else
		{
			PressedKeys.Add(Key);

			int32& AggregateCount = AggregateKeyCounts.FindOrAdd(Key);
			bAggregateStateChanged = AggregateCount == 0;
			++AggregateCount;
		}
	}
	else
	{
		TSet<FKey>* PressedKeys = DevicePressedKeys.Find(DeviceId);

		// 当前设备从未按下该键，忽略无效或重复的 Released。
		if (!PressedKeys || !PressedKeys->Contains(Key))
		{
			return false;
		}

		PressedKeys->Remove(Key);

		if (PressedKeys->Num() == 0)
		{
			DevicePressedKeys.Remove(DeviceId);
		}

		if (int32* AggregateCount = AggregateKeyCounts.Find(Key))
		{
			if (*AggregateCount <= 1)
			{
				AggregateKeyCounts.Remove(Key);
				bAggregateStateChanged = true;
			}
			else
			{
				--(*AggregateCount);
			}
		}
		else
		{
			ensureMsgf(false,
				TEXT("Global input aggregate state is missing key %s."),
				*Key.ToString());
			return false;
		}
	}

	// Preserve real repeated Down messages. A second physical device changing
	// the same key only updates per-device state and does not create a new edge.
	if (!bRepeat && !bAggregateStateChanged)
	{
		return false;
	}

	if (bAggregateStateChanged)
	{
		if (bPressed)
		{
			PressedThisFrame.Add(Key);
		}
		else
		{
			ReleasedThisFrame.Add(Key);
		}
	}

	OutEvent.Key = Key;
	OutEvent.EventType = bPressed
		? EGlobalInputEventType::Pressed
		: EGlobalInputEventType::Released;

	OutEvent.bRepeat = bRepeat;
	OutEvent.DeviceId = DeviceId;
	OutEvent.TimestampSeconds = TimestampSeconds;

	/*
	 * 在状态更新后获取修饰键状态：
	 *
	 * LeftControl Pressed 事件中 bControl=true。
	 * LeftControl Released 事件中 bControl=false，
	 * 除非另一个设备或 RightControl 仍然按住。
	 */
	OutEvent.Modifiers = GetModifierState();

	return true;
}

void FInputStateManager::BeginFrame()
{
	check(IsInGameThread());

	PressedThisFrame.Reset();
	ReleasedThisFrame.Reset();
	MouseDelta = FVector2D::ZeroVector;
}

void FInputStateManager::RegisterKeyboardDevice(
	int64 DeviceId)
{
	check(IsInGameThread());

	if (DeviceId != 0)
	{
		KnownKeyboardDevices.Add(DeviceId);
	}
}

void FInputStateManager::RemoveDevice(
	int64 DeviceId,
	double TimestampSeconds,
	TArray<FGlobalKeyEvent>& OutReleasedEvents)
{
	check(IsInGameThread());

	OutReleasedEvents.Reset();
	KnownKeyboardDevices.Remove(DeviceId);

	TSet<FKey>* PressedKeys = DevicePressedKeys.Find(DeviceId);
	if (!PressedKeys)
	{
		return;
	}

	TArray<FKey> RemovedKeys;
	RemovedKeys.Reserve(PressedKeys->Num());
	for (const FKey& Key : *PressedKeys)
	{
		RemovedKeys.Add(Key);
	}

	DevicePressedKeys.Remove(DeviceId);

	TArray<FKey> AggregateReleasedKeys;
	AggregateReleasedKeys.Reserve(RemovedKeys.Num());

	for (const FKey& Key : RemovedKeys)
	{
		int32* AggregateCount = AggregateKeyCounts.Find(Key);
		if (!ensureMsgf(AggregateCount && *AggregateCount > 0,
			TEXT("Global input aggregate state is invalid for removed device key %s."),
			*Key.ToString()))
		{
			continue;
		}

		if (*AggregateCount == 1)
		{
			AggregateKeyCounts.Remove(Key);
			AggregateReleasedKeys.Add(Key);
			ReleasedThisFrame.Add(Key);
		}
		else
		{
			--(*AggregateCount);
		}
	}

	const FGlobalModifierState ModifiersAfterRemoval =
		GetModifierState();

	OutReleasedEvents.Reserve(AggregateReleasedKeys.Num());

	for (const FKey& Key : AggregateReleasedKeys)
	{
		FGlobalKeyEvent& Event =
			OutReleasedEvents.AddDefaulted_GetRef();
		Event.Key = Key;
		Event.EventType = EGlobalInputEventType::Released;
		Event.Modifiers = ModifiersAfterRemoval;
		Event.bRepeat = false;
		Event.DeviceId = DeviceId;
		Event.TimestampSeconds = TimestampSeconds;
	}
}

void FInputStateManager::ReleaseAllKeys(
	double TimestampSeconds,
	TArray<FGlobalKeyEvent>& OutReleasedEvents)
{
	check(IsInGameThread());

	OutReleasedEvents.Reset();
	OutReleasedEvents.Reserve(AggregateKeyCounts.Num());

	TArray<FKey> ReleasedKeys;
	AggregateKeyCounts.GenerateKeyArray(ReleasedKeys);

	DevicePressedKeys.Reset();
	AggregateKeyCounts.Reset();

	const FGlobalModifierState ClearedModifiers;

	for (const FKey& Key : ReleasedKeys)
	{
		ReleasedThisFrame.Add(Key);

		FGlobalKeyEvent& Event =
			OutReleasedEvents.AddDefaulted_GetRef();
		Event.Key = Key;
		Event.EventType = EGlobalInputEventType::Released;
		Event.Modifiers = ClearedModifiers;
		Event.bRepeat = false;
		Event.DeviceId = 0;
		Event.TimestampSeconds = TimestampSeconds;
	}
}

bool FInputStateManager::IsKeyDown(const FKey& Key) const
{
	check(IsInGameThread());

	if (!Key.IsValid())
	{
		return false;
	}

	const int32* AggregateCount = AggregateKeyCounts.Find(Key);
	return AggregateCount && *AggregateCount > 0;
}

bool FInputStateManager::WasKeyPressedThisFrame(
	const FKey& Key) const
{
	check(IsInGameThread());
	return Key.IsValid() && PressedThisFrame.Contains(Key);
}

bool FInputStateManager::WasKeyReleasedThisFrame(
	const FKey& Key) const
{
	check(IsInGameThread());
	return Key.IsValid() && ReleasedThisFrame.Contains(Key);
}

TArray<FKey> FInputStateManager::GetPressedKeys() const
{
	check(IsInGameThread());

	TArray<FKey> Keys;
	AggregateKeyCounts.GenerateKeyArray(Keys);
	Keys.Sort([](const FKey& Left, const FKey& Right)
	{
		return Left.GetFName().LexicalLess(Right.GetFName());
	});
	return Keys;
}

int32 FInputStateManager::GetKeyboardDeviceCount() const
{
	check(IsInGameThread());
	return KnownKeyboardDevices.Num();
}

void FInputStateManager::UpdateMousePosition(
	const FVector2D& Position,
	bool bPositionValid)
{
	check(IsInGameThread());

	if (bPositionValid)
	{
		MousePosition = Position;
	}

	/*
	 * 当 GetCursorPos 失败时保留最后一次坐标，
	 * 但把有效状态设为 false。
	 */
	bMousePositionValid = bPositionValid;
}

void FInputStateManager::UpdateMouseDelta(
	const FVector2D& Delta)
{
	check(IsInGameThread());
	MouseDelta = Delta;
}

FVector2D FInputStateManager::GetMousePosition() const
{
	check(IsInGameThread());
	return MousePosition;
}

FVector2D FInputStateManager::GetMouseDelta() const
{
	check(IsInGameThread());
	return MouseDelta;
}

bool FInputStateManager::HasValidMousePosition() const
{
	check(IsInGameThread());
	return bMousePositionValid;
}

FGlobalModifierState FInputStateManager::GetModifierState() const
{
	check(IsInGameThread());

	FGlobalModifierState ModifierState;

	ModifierState.bControl =
		IsKeyDown(EKeys::LeftControl) ||
		IsKeyDown(EKeys::RightControl);

	ModifierState.bAlt =
		IsKeyDown(EKeys::LeftAlt) ||
		IsKeyDown(EKeys::RightAlt);

	ModifierState.bShift =
		IsKeyDown(EKeys::LeftShift) ||
		IsKeyDown(EKeys::RightShift);

	ModifierState.bCommand =
		IsKeyDown(EKeys::LeftCommand) ||
		IsKeyDown(EKeys::RightCommand);

	return ModifierState;
}

void FInputStateManager::Reset()
{
	check(IsInGameThread());

	DevicePressedKeys.Reset();
	AggregateKeyCounts.Reset();
	PressedThisFrame.Reset();
	ReleasedThisFrame.Reset();
	KnownKeyboardDevices.Reset();

	MousePosition = FVector2D::ZeroVector;
	MouseDelta = FVector2D::ZeroVector;
	bMousePositionValid = false;
}
