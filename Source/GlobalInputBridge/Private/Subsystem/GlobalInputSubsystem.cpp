//GlobalInputSubsystem.cpp

#include "Subsystem/GlobalInputSubsystem.h"

#include "Containers/Queue.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GlobalInputBridgeModule.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Input/GlobalChordBindingManager.h"
#include "Input/GlobalChordDelegateBinding.h"
#include "Input/InputStateManager.h"
#include "Input/RawInputTypes.h"
#include "Input/RawInputWorker.h"
#include "Logging/GlobalInputLog.h"
#include "Misc/CoreMisc.h"
#include "Settings/GlobalInputSettings.h"
#include "Templates/UnrealTemplate.h"
#include "Windows/WindowsKeyMapper.h"
#include "Windows/WindowsMousePoller.h"

/**
 * Private Implementation。
 *
 * Public Subsystem 头文件不会暴露：
 * - Raw Input 数据包
 * - 输入队列
 * - Worker
 * - InputStateManager
 * - WindowsMousePoller
 */
struct FGlobalInputSubsystemImpl
{
	TQueue<FRawInputMessage, EQueueMode::Spsc> RawInputQueue;

	TUniquePtr<FRawInputWorker> Worker;

	FInputStateManager StateManager;
	FGlobalChordBindingManager ChordBindingManager;
	FWindowsMousePoller MousePoller;

	/** 本帧从 Raw Input 队列累加的鼠标相对位移（设备计数）。 */
	FVector2D PendingRawMouseDelta = FVector2D::ZeroVector;
	double LastRawMouseTimestampSeconds = 0.0;
	bool bRawMouseMovedThisFrame = false;

	TSet<FKey> EventFilter;
	bool bEventFilterEnabled = false;
	bool bEventFilterExcludeMode = false;
	EGlobalMouseTrackingMode MouseTrackingMode =
		EGlobalMouseTrackingMode::RawInput;
};

namespace
{
	bool ShouldPollMouseButtons(EGlobalMouseTrackingMode Mode)
	{
		return Mode != EGlobalMouseTrackingMode::Disabled;
	}

	/** RawInput 模式仍轮询位置作为事件载荷；锁鼠游戏中即屏幕中心。 */
	bool ShouldPollMousePosition(EGlobalMouseTrackingMode Mode)
	{
		return Mode == EGlobalMouseTrackingMode::Polling ||
			Mode == EGlobalMouseTrackingMode::RawInput;
	}

	bool ShouldUseRawMouseDelta(EGlobalMouseTrackingMode Mode)
	{
		return Mode == EGlobalMouseTrackingMode::RawInput;
	}

	void InvokeChordCallbacks(
		const TArray<FGlobalChordInvocation>& Invocations)
	{
		for (const FGlobalChordInvocation& Invocation : Invocations)
		{
			UObject* Target = Invocation.Target.Get();
			if (!IsValid(Target) || Invocation.FunctionName.IsNone())
			{
				continue;
			}

			UFunction* Function =
				Target->FindFunction(Invocation.FunctionName);
			if (!Function)
			{
				continue;
			}

			FGlobalChordEventInfo Parameters = Invocation.EventInfo;
			Target->ProcessEvent(Function, &Parameters);
		}
	}
}

void UGlobalInputSubsystem::Initialize(
	FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	check(Impl == nullptr);

	Impl = new FGlobalInputSubsystemImpl();
	bListening = false;
	RefreshRuntimeSettings();

	FWorldDelegates::OnWorldInitializedActors.AddUObject(
		this,
		&UGlobalInputSubsystem::HandleWorldInitializedActors);
	FWorldDelegates::LevelAddedToWorld.AddUObject(
		this,
		&UGlobalInputSubsystem::HandleLevelAddedToWorld);
	FWorldDelegates::OnWorldCleanup.AddUObject(
		this,
		&UGlobalInputSubsystem::HandleWorldCleanup);

	// FWindowsKeyMapper::Initialize() 已在模块 StartupModule 中调用。

	if (FGlobalInputLog::ShouldLog(EGlobalInputLogLevel::Verbose))
	{
		UE_LOG(LogGlobalInput, Log,
			TEXT("GlobalInputSubsystem initialized."));
	}

	const UGlobalInputSettings* Settings =
		GetDefault<UGlobalInputSettings>();
	if (Settings && Settings->bAutoStart &&
		!IsRunningCommandlet() &&
		!IsRunningDedicatedServer())
	{
		StartListening();
	}
}

void UGlobalInputSubsystem::Deinitialize()
{
	FWorldDelegates::OnWorldInitializedActors.RemoveAll(this);
	FWorldDelegates::LevelAddedToWorld.RemoveAll(this);
	FWorldDelegates::OnWorldCleanup.RemoveAll(this);

	// Engine shutdown must not call Blueprint listeners that may already be tearing down.
	StopListeningInternal(false);

	delete Impl;
	Impl = nullptr;

	if (FGlobalInputLog::ShouldLog(EGlobalInputLogLevel::Verbose))
	{
		UE_LOG(LogGlobalInput, Log,
			TEXT("GlobalInputSubsystem deinitialized."));
	}

	Super::Deinitialize();
}

void UGlobalInputSubsystem::Tick(float DeltaTime)
{
	(void)DeltaTime;

	if (!bListening || !Impl)
	{
		return;
	}

	Impl->StateManager.BeginFrame();

	// 先处理键盘，使本帧鼠标按钮事件能够获得最新修饰键状态。
	ProcessRawInputQueue();

	// A listener is allowed to call StopListening from a key callback.
	if (!bListening)
	{
		return;
	}

	if (ShouldPollMouseButtons(Impl->MouseTrackingMode))
	{
		PollGlobalMouse(
			ShouldPollMousePosition(Impl->MouseTrackingMode));
	}

	if (bListening)
	{
		TriggerActiveGlobalChords();
	}
}

bool UGlobalInputSubsystem::IsTickable() const
{
	return bListening &&
		Impl != nullptr &&
		!IsTemplate();
}

bool UGlobalInputSubsystem::StartListening()
{
	if (!Impl)
	{
		if (FGlobalInputLog::ShouldLog(EGlobalInputLogLevel::Error))
		{
			UE_LOG(LogGlobalInput, Error,
				TEXT("Cannot start Global Input: subsystem implementation is invalid."));
		}
		return false;
	}

	if (bListening)
	{
		return true;
	}

	if (bLifecycleTransition)
	{
		if (FGlobalInputLog::ShouldLog(EGlobalInputLogLevel::Warning))
		{
			UE_LOG(LogGlobalInput, Warning,
				TEXT("Cannot start Global Input during another listening lifecycle transition."));
		}
		return false;
	}

	TGuardValue<bool> LifecycleGuard(
		bLifecycleTransition,
		true);

	RefreshRuntimeSettings();

	// 清理异常残留的 Worker。
	if (Impl->Worker)
	{
		Impl->Worker->Shutdown();
		Impl->Worker.Reset();
	}

	Impl->StateManager.Reset();
	Impl->MousePoller.Reset();
	ClearPendingPackets();

	Impl->Worker =
		MakeUnique<FRawInputWorker>(
			Impl->RawInputQueue,
			ShouldUseRawMouseDelta(Impl->MouseTrackingMode));

	if (!Impl->Worker->Start())
	{
		if (FGlobalInputLog::ShouldLog(EGlobalInputLogLevel::Error))
		{
			UE_LOG(LogGlobalInput, Error,
				TEXT("Failed to start Global Input worker."));
		}

		Impl->Worker.Reset();
		return false;
	}

	// Capture position and button baselines without manufacturing startup events.
	if (ShouldPollMouseButtons(Impl->MouseTrackingMode))
	{
		SynchronizeGlobalMouse(
			ShouldPollMousePosition(Impl->MouseTrackingMode));
	}

	// Initial mouse state establishes a baseline, never a per-frame edge.
	Impl->StateManager.BeginFrame();

	bListening = true;

	if (FGlobalInputLog::ShouldLog(EGlobalInputLogLevel::Verbose))
	{
		UE_LOG(LogGlobalInput, Log,
			TEXT("Global Input listening started."));
	}

	return true;
}

void UGlobalInputSubsystem::StopListening()
{
	StopListeningInternal(true);
}

void UGlobalInputSubsystem::StopListeningInternal(
	bool bBroadcastReleasedEvents)
{
	if (!Impl)
	{
		return;
	}

	if (bLifecycleTransition)
	{
		return;
	}

	TGuardValue<bool> LifecycleGuard(
		bLifecycleTransition,
		true);

	const bool bWasListening = bListening;

	bListening = false;

	if (Impl->Worker)
	{
		Impl->Worker->Shutdown();
		Impl->Worker.Reset();
	}

	ClearPendingPackets();

	TArray<FGlobalKeyEvent> ReleasedEvents;
	Impl->StateManager.ReleaseAllKeys(
		FPlatformTime::Seconds(),
		ReleasedEvents);

	if (bBroadcastReleasedEvents && bWasListening)
	{
		for (const FGlobalKeyEvent& ReleasedEvent : ReleasedEvents)
		{
			BroadcastKeyEvent(ReleasedEvent);
		}
	}

	Impl->StateManager.Reset();
	Impl->MousePoller.Reset();

	if (bWasListening)
	{
		if (FGlobalInputLog::ShouldLog(EGlobalInputLogLevel::Verbose))
		{
			UE_LOG(LogGlobalInput, Log,
				TEXT("Global Input listening stopped."));
		}
	}
}

bool UGlobalInputSubsystem::IsGlobalKeyDown(
	FKey Key) const
{
	if (!Impl || !Key.IsValid())
	{
		return false;
	}

	return Impl->StateManager.IsKeyDown(Key);
}

bool UGlobalInputSubsystem::WasGlobalKeyPressedThisFrame(
	FKey Key) const
{
	return Impl &&
		Impl->StateManager.WasKeyPressedThisFrame(Key);
}

bool UGlobalInputSubsystem::WasGlobalKeyReleasedThisFrame(
	FKey Key) const
{
	return Impl &&
		Impl->StateManager.WasKeyReleasedThisFrame(Key);
}

TArray<FKey> UGlobalInputSubsystem::GetPressedGlobalKeys() const
{
	return Impl
		? Impl->StateManager.GetPressedKeys()
		: TArray<FKey>();
}

FVector2D
UGlobalInputSubsystem::GetGlobalMousePosition() const
{
	return Impl
		? Impl->StateManager.GetMousePosition()
		: FVector2D::ZeroVector;
}

FVector2D UGlobalInputSubsystem::GetGlobalMouseDelta() const
{
	return Impl
		? Impl->StateManager.GetMouseDelta()
		: FVector2D::ZeroVector;
}

bool
UGlobalInputSubsystem::HasValidGlobalMousePosition() const
{
	return Impl &&
		Impl->StateManager.HasValidMousePosition();
}

FGlobalModifierState
UGlobalInputSubsystem::GetGlobalModifierState() const
{
	return Impl
		? Impl->StateManager.GetModifierState()
		: FGlobalModifierState();
}

bool UGlobalInputSubsystem::MatchesGlobalChord(
	const FGlobalKeyEvent& KeyEvent,
	const FGlobalKeyChord& Chord) const
{
	if (!Chord.Key.IsValid() ||
		KeyEvent.Key != Chord.Key)
	{
		return false;
	}

	FGlobalModifierState Actual = KeyEvent.Modifiers;
	FGlobalModifierState Required =
		Chord.GetEffectiveRequiredModifiers();
	FGlobalKeyChord::RemoveMainKeyModifier(Chord.Key, Actual);

	if (Required.bControl && !Actual.bControl)
	{
		return false;
	}

	if (Required.bAlt && !Actual.bAlt)
	{
		return false;
	}

	if (Required.bShift && !Actual.bShift)
	{
		return false;
	}

	if (Required.bCommand && !Actual.bCommand)
	{
		return false;
	}

	if (!Chord.bExactModifiers)
	{
		return true;
	}

	return Actual == Required;
}

FGlobalInputDebugInfo
UGlobalInputSubsystem::GetGlobalInputDebugInfo() const
{
	FGlobalInputDebugInfo Info;
	Info.bListening = bListening;

	if (!Impl)
	{
		return Info;
	}

	const TArray<FKey> PressedKeys =
		Impl->StateManager.GetPressedKeys();

	Info.KeyboardCount =
		Impl->StateManager.GetKeyboardDeviceCount();
	Info.PressedKeyCount = PressedKeys.Num();
	Info.MousePosition =
		Impl->StateManager.GetMousePosition();
	Info.MouseDelta =
		Impl->StateManager.GetMouseDelta();
	Info.bMousePositionValid =
		Impl->StateManager.HasValidMousePosition();
	Info.bEventFilterEnabled =
		Impl->bEventFilterEnabled;
	Info.bEventFilterExcludeMode =
		Impl->bEventFilterExcludeMode;
	Info.EventFilterKeyCount =
		Impl->EventFilter.Num();

	TArray<FString> KeyNames;
	KeyNames.Reserve(PressedKeys.Num());
	for (const FKey& Key : PressedKeys)
	{
		KeyNames.Add(Key.ToString());
	}
	Info.ActiveKeys = FString::Join(KeyNames, TEXT(", "));

	return Info;
}

void UGlobalInputSubsystem::SetGlobalInputEventFilter(
	const TArray<FKey>& Keys,
	bool bExcludeMode)
{
	if (!Impl)
	{
		return;
	}

	Impl->EventFilter.Reset();
	for (const FKey& Key : Keys)
	{
		if (Key.IsValid())
		{
			Impl->EventFilter.Add(Key);
		}
	}
	Impl->bEventFilterEnabled = true;
	Impl->bEventFilterExcludeMode = bExcludeMode;
}

void UGlobalInputSubsystem::ClearGlobalInputEventFilter()
{
	if (!Impl)
	{
		return;
	}

	Impl->EventFilter.Reset();
	Impl->bEventFilterEnabled = false;
	Impl->bEventFilterExcludeMode = false;
}

bool UGlobalInputSubsystem::IsGlobalInputEventFilterEnabled() const
{
	return Impl && Impl->bEventFilterEnabled;
}

bool UGlobalInputSubsystem::IsGlobalInputEventFilterExcludeMode() const
{
	return Impl &&
		Impl->bEventFilterEnabled &&
		Impl->bEventFilterExcludeMode;
}

TArray<FKey> UGlobalInputSubsystem::GetGlobalInputEventFilter() const
{
	TArray<FKey> Keys;
	if (!Impl)
	{
		return Keys;
	}

	Keys.Reserve(Impl->EventFilter.Num());
	for (const FKey& Key : Impl->EventFilter)
	{
		Keys.Add(Key);
	}
	Keys.Sort([](const FKey& Left, const FKey& Right)
	{
		return Left.GetFName().LexicalLess(Right.GetFName());
	});
	return Keys;
}

void UGlobalInputSubsystem::RegisterGlobalChordBinding(
	UObject* Target,
	const FGlobalChordBlueprintBinding& Binding)
{
	if (!Impl)
	{
		return;
	}

	Impl->ChordBindingManager.RegisterBinding(Target, Binding);
}

void UGlobalInputSubsystem::UnregisterGlobalChordBindings(
	UObject* Target)
{
	if (!Impl)
	{
		return;
	}

	Impl->ChordBindingManager.UnregisterBindings(Target);
}

void UGlobalInputSubsystem::ProcessRawInputQueue()
{
	if (!Impl)
	{
		return;
	}

	FRawInputMessage Message;

	while (bListening && Impl->RawInputQueue.Dequeue(Message))
	{
		if (Message.Type == ERawInputMessageType::DeviceAdded)
		{
			Impl->StateManager.RegisterKeyboardDevice(
				static_cast<int64>(Message.DeviceId));
			continue;
		}

		if (Message.Type == ERawInputMessageType::DeviceRemoved)
		{
			TArray<FGlobalKeyEvent> ReleasedEvents;
			Impl->StateManager.RemoveDevice(
				static_cast<int64>(Message.DeviceId),
				Message.TimestampSeconds,
				ReleasedEvents);

			for (const FGlobalKeyEvent& ReleasedEvent : ReleasedEvents)
			{
				BroadcastKeyEvent(ReleasedEvent);

				if (!bListening)
				{
					return;
				}
			}

			continue;
		}

		if (Message.Type == ERawInputMessageType::MouseDelta)
		{
			// 多个鼠标设备的位移在帧内聚合，与轮询模式的聚合语义一致。
			Impl->PendingRawMouseDelta.X +=
				static_cast<double>(Message.Mouse.DeltaX);
			Impl->PendingRawMouseDelta.Y +=
				static_cast<double>(Message.Mouse.DeltaY);
			Impl->LastRawMouseTimestampSeconds =
				Message.TimestampSeconds;
			Impl->bRawMouseMovedThisFrame = true;
			continue;
		}

		const FRawKeyboardPacket& Packet = Message.Keyboard;
		Impl->StateManager.RegisterKeyboardDevice(
			static_cast<int64>(Packet.DeviceId));

		const FKey Key =
			FWindowsKeyMapper::ConvertKeyboard(Packet);

		if (!Key.IsValid())
		{
			continue;
		}

		FGlobalKeyEvent KeyEvent;

		if (!Impl->StateManager.ProcessKeyEvent(
			Key,
			Packet.bPressed,
			static_cast<int64>(Packet.DeviceId),
			Packet.TimestampSeconds,
			KeyEvent))
		{
			continue;
		}

		BroadcastKeyEvent(KeyEvent);

		if (!bListening)
		{
			return;
		}

		if (FGlobalInputLog::ShouldLog(EGlobalInputLogLevel::Verbose))
		{
			UE_LOG(LogGlobalInput, Log,
				TEXT("Global key %s %s Repeat=%d"),
				*Key.ToString(),
				KeyEvent.EventType ==
					EGlobalInputEventType::Pressed
						? TEXT("Pressed")
						: TEXT("Released"),
				KeyEvent.bRepeat ? 1 : 0);
		}
	}
}

void UGlobalInputSubsystem::SynchronizeGlobalMouse(
	bool bPollPosition)
{
	if (!Impl)
	{
		return;
	}

	FWindowsMousePollResult Result;
	Impl->MousePoller.Poll(Result, bPollPosition);

	if (bPollPosition)
	{
		Impl->StateManager.UpdateMousePosition(
			Result.ScreenPosition,
			Result.bPositionValid);
		Impl->StateManager.UpdateMouseDelta(
			FVector2D::ZeroVector);
	}

	for (const FWindowsMouseButtonTransition& Transition :
		Result.ButtonTransitions)
	{
		FGlobalKeyEvent IgnoredEvent;
		Impl->StateManager.ProcessKeyEvent(
			Transition.Key,
			Transition.bPressed,
			0,
			Result.TimestampSeconds,
			IgnoredEvent);
	}

	// Worker 启动到基线同步之间可能已积累 Raw 位移，全部吞掉。
	Impl->PendingRawMouseDelta = FVector2D::ZeroVector;
	Impl->LastRawMouseTimestampSeconds = 0.0;
	Impl->bRawMouseMovedThisFrame = false;
}

void UGlobalInputSubsystem::PollGlobalMouse(
	bool bPollPosition)
{
	if (!Impl)
	{
		return;
	}

	FWindowsMousePollResult Result;
	Impl->MousePoller.Poll(Result, bPollPosition);

	const bool bUseRawDelta =
		ShouldUseRawMouseDelta(Impl->MouseTrackingMode);

	if (bPollPosition)
	{
		const FVector2D FrameDelta = bUseRawDelta
			? Impl->PendingRawMouseDelta
			: (Result.bPositionValid
				? Result.Delta
				: FVector2D::ZeroVector);

		Impl->StateManager.UpdateMouseDelta(FrameDelta);

		const bool bPreviousPositionValid =
			Impl->StateManager.HasValidMousePosition();

		const FVector2D PreviousPosition =
			Impl->StateManager.GetMousePosition();

		if (Result.bPositionValid)
		{
			Impl->StateManager.UpdateMousePosition(
				Result.ScreenPosition,
				true);
		}
		else
		{
			// 保留最后一次有效坐标，但把有效状态设为 false。
			Impl->StateManager.UpdateMousePosition(
				PreviousPosition,
				false);
		}

		const bool bPositionValidityChanged =
			bPreviousPositionValid != Result.bPositionValid;

		const bool bFrameMouseMoved =
			(bUseRawDelta && Impl->bRawMouseMovedThisFrame) ||
			(!bUseRawDelta && Result.bPositionChanged) ||
			bPositionValidityChanged;

		if (bFrameMouseMoved)
		{
			FGlobalMouseMoveEvent MouseEvent;

			MouseEvent.ScreenPosition =
				Result.bPositionValid
					? Result.ScreenPosition
					: PreviousPosition;

			MouseEvent.Delta = FrameDelta;

			MouseEvent.bPositionValid =
				Result.bPositionValid;

			// Raw 位移与轮询一样是跨设备聚合结果，无法区分物理鼠标。
			MouseEvent.DeviceId = 0;
			MouseEvent.TimestampSeconds =
				bUseRawDelta && Impl->bRawMouseMovedThisFrame
					? Impl->LastRawMouseTimestampSeconds
					: Result.TimestampSeconds;

			OnGlobalMouseMove.Broadcast(MouseEvent);

			if (!bListening)
			{
				return;
			}

			//调试用：打印鼠标位置
			if (FGlobalInputLog::ShouldLog(EGlobalInputLogLevel::Verbose))
			{
				UE_LOG(LogGlobalInput, Log,
					TEXT("Global mouse position=%s Delta=%s Valid=%d Raw=%d"),
					*MouseEvent.ScreenPosition.ToString(),
					*MouseEvent.Delta.ToString(),
					MouseEvent.bPositionValid ? 1 : 0,
					bUseRawDelta ? 1 : 0);
			}
		}

		// 消费本帧累加的 Raw 位移，避免跨帧残留。
		Impl->PendingRawMouseDelta = FVector2D::ZeroVector;
		Impl->LastRawMouseTimestampSeconds = 0.0;
		Impl->bRawMouseMovedThisFrame = false;
	}

	for (const FWindowsMouseButtonTransition& Transition :
		Result.ButtonTransitions)
	{
		if (!Transition.Key.IsValid())
		{
			continue;
		}

		FGlobalKeyEvent KeyEvent;

		if (!Impl->StateManager.ProcessKeyEvent(
			Transition.Key,
			Transition.bPressed,
			0,
			Result.TimestampSeconds,
			KeyEvent))
		{
			continue;
		}

		BroadcastKeyEvent(KeyEvent);

		if (!bListening)
		{
			return;
		}

		//调试用：打印鼠标按键
		if (FGlobalInputLog::ShouldLog(EGlobalInputLogLevel::Verbose))
		{
			UE_LOG(LogGlobalInput, Log,
				TEXT("Global mouse button %s %s"),
				*Transition.Key.ToString(),
				Transition.bPressed
					? TEXT("Pressed")
					: TEXT("Released"));
		}
	}
}

void UGlobalInputSubsystem::ClearPendingPackets()
{
	if (!Impl)
	{
		return;
	}

	FRawInputMessage Message;

	while (Impl->RawInputQueue.Dequeue(Message))
	{
	}

	Impl->PendingRawMouseDelta = FVector2D::ZeroVector;
	Impl->LastRawMouseTimestampSeconds = 0.0;
	Impl->bRawMouseMovedThisFrame = false;
}

bool UGlobalInputSubsystem::ShouldBroadcastKeyEvent(
	const FKey& Key) const
{
	if (!Impl || !Key.IsValid())
	{
		return false;
	}

	return !Impl->bEventFilterEnabled ||
		(Impl->bEventFilterExcludeMode
			? !Impl->EventFilter.Contains(Key)
			: Impl->EventFilter.Contains(Key));
}

void UGlobalInputSubsystem::BroadcastKeyEvent(
	const FGlobalKeyEvent& KeyEvent)
{
	const bool bAllowedByFilter = ShouldBroadcastKeyEvent(KeyEvent.Key);

	if (bAllowedByFilter)
	{
		OnGlobalKeyEvent.Broadcast(KeyEvent);
	}

	/*
	 * 事件过滤同样作用于 Global Input Action Event：
	 * 被过滤键的 Pressed 不进入组合键处理，无法触发新的 Started；
	 * Released 仍放行，保证过滤前已开始的动作立即收到 Completed，
	 * 否则蓝图侧 Started 无人收尾。
	 */
	if (bAllowedByFilter ||
		KeyEvent.EventType == EGlobalInputEventType::Released)
	{
		ProcessGlobalChordKeyEvent(KeyEvent);
	}
}

void UGlobalInputSubsystem::ProcessGlobalChordKeyEvent(
	const FGlobalKeyEvent& KeyEvent)
{
	if (!Impl)
	{
		return;
	}

	TArray<FGlobalChordInvocation> Invocations;
	Impl->ChordBindingManager.ProcessKeyEvent(
		KeyEvent,
		Impl->StateManager,
		Invocations);
	InvokeChordCallbacks(Invocations);
}

void UGlobalInputSubsystem::TriggerActiveGlobalChords()
{
	if (!Impl)
	{
		return;
	}

	TArray<FGlobalChordInvocation> Invocations;
	Impl->ChordBindingManager.GatherTriggered(
		Impl->StateManager,
		FPlatformTime::Seconds(),
		Invocations);

	// 被过滤键的活动动作不再触发 Triggered；
	// Completed 必须放行，负责收尾过滤前已开始的动作。
	Invocations.RemoveAllSwap(
		[this](const FGlobalChordInvocation& Invocation)
		{
			return Invocation.Phase ==
					EGlobalChordInvocationPhase::Triggered &&
				!ShouldBroadcastKeyEvent(Invocation.EventInfo.Key);
		},
		EAllowShrinking::No);

	InvokeChordCallbacks(Invocations);
}

void UGlobalInputSubsystem::HandleWorldInitializedActors(
	const FActorsInitializedParams& Parameters)
{
	RegisterGlobalChordBindingsForWorld(Parameters.World);
}

void UGlobalInputSubsystem::HandleLevelAddedToWorld(
	ULevel* Level,
	UWorld* World)
{
	if (!Impl || !Level || !World || !World->IsGameWorld())
	{
		return;
	}

	for (AActor* Actor : Level->Actors)
	{
		RegisterCompiledGlobalChordBindings(Actor);
	}
}

void UGlobalInputSubsystem::HandleWorldCleanup(
	UWorld* World,
	bool bSessionEnded,
	bool bCleanupResources)
{
	(void)bSessionEnded;
	(void)bCleanupResources;

	if (Impl)
	{
		Impl->ChordBindingManager.UnregisterBindingsForWorld(World);
	}
}

void UGlobalInputSubsystem::RegisterCompiledGlobalChordBindings(
	UObject* Target)
{
	if (!Impl || !IsValid(Target) || Target->IsTemplate())
	{
		return;
	}

	for (const UClass* Class = Target->GetClass();
		Class;
		Class = Class->GetSuperClass())
	{
		const UGlobalChordDelegateBinding* BindingObject =
			Cast<UGlobalChordDelegateBinding>(
				UBlueprintGeneratedClass::GetDynamicBindingObject(
					Class,
					UGlobalChordDelegateBinding::StaticClass()));

		if (!BindingObject)
		{
			continue;
		}

		for (const FGlobalChordBlueprintBinding& Binding :
			BindingObject->ChordBindings)
		{
			Impl->ChordBindingManager.RegisterBinding(Target, Binding);
		}
	}
}

void UGlobalInputSubsystem::RegisterGlobalChordBindingsForWorld(
	UWorld* World)
{
	if (!Impl || !World || !World->IsGameWorld())
	{
		return;
	}

	const int32 PreviousBindingCount =
		Impl->ChordBindingManager.Num();

	for (TActorIterator<AActor> ActorIterator(World);
		ActorIterator;
		++ActorIterator)
	{
		RegisterCompiledGlobalChordBindings(*ActorIterator);
	}

	if (FGlobalInputLog::ShouldLog(EGlobalInputLogLevel::Verbose))
	{
		UE_LOG(
			LogGlobalInput,
			Log,
			TEXT("Refreshed compiled chord bindings for world %s. Added=%d Total=%d"),
			*World->GetName(),
			Impl->ChordBindingManager.Num() - PreviousBindingCount,
			Impl->ChordBindingManager.Num());
	}
}

#if WITH_DEV_AUTOMATION_TESTS
int32 UGlobalInputSubsystem::GetRegisteredGlobalChordBindingCountForTarget(
	const UObject* Target) const
{
	return Impl
		? Impl->ChordBindingManager.NumForTarget(Target)
		: 0;
}
#endif

void UGlobalInputSubsystem::RefreshRuntimeSettings()
{
	if (!Impl)
	{
		return;
	}

	const UGlobalInputSettings* Settings =
		GetDefault<UGlobalInputSettings>();
	if (!Settings)
	{
		return;
	}

	FGlobalInputLog::SetLevel(Settings->LogLevel);
	Impl->MouseTrackingMode = Settings->MouseTrackingMode;
}
