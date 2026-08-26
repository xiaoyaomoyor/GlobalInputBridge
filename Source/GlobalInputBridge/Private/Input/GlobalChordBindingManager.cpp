#include "Input/GlobalChordBindingManager.h"

#include "Input/InputStateManager.h"

void FGlobalChordBindingManager::RegisterBinding(
	UObject* Target,
	const FGlobalChordBlueprintBinding& Binding)
{
	check(IsInGameThread());

	if (!IsValid(Target) || Target->IsTemplate() ||
		!Binding.Chord.Key.IsValid())
	{
		return;
	}

	FRuntimeBinding* Existing = nullptr;
	if (Binding.BindingId.IsValid())
	{
		Existing = Bindings.FindByPredicate(
			[Target, &Binding](const FRuntimeBinding& Candidate)
			{
				return Candidate.Target.Get() == Target &&
					Candidate.Definition.BindingId == Binding.BindingId;
			});
	}

	FRuntimeBinding& RuntimeBinding = Existing
		? *Existing
		: Bindings.AddDefaulted_GetRef();

	RuntimeBinding.Target = Target;
	RuntimeBinding.Definition = Binding;
	RuntimeBinding.bActive = false;
	RuntimeBinding.ActivationDeviceId = 0;
}

void FGlobalChordBindingManager::UnregisterBindings(UObject* Target)
{
	check(IsInGameThread());

	Bindings.RemoveAllSwap(
		[Target](const FRuntimeBinding& Binding)
		{
			return !Binding.Target.IsValid() ||
				Binding.Target.Get() == Target;
		},
		EAllowShrinking::No);
}

void FGlobalChordBindingManager::UnregisterBindingsForWorld(
	const UWorld* World)
{
	check(IsInGameThread());

	if (!World)
	{
		return;
	}

	Bindings.RemoveAllSwap(
		[World](const FRuntimeBinding& Binding)
		{
			UObject* Target = Binding.Target.Get();
			return !IsValid(Target) || Target->GetWorld() == World;
		},
		EAllowShrinking::No);
}

void FGlobalChordBindingManager::ProcessKeyEvent(
	const FGlobalKeyEvent& KeyEvent,
	const FInputStateManager& StateManager,
	TArray<FGlobalChordInvocation>& OutInvocations)
{
	check(IsInGameThread());

	RemoveInvalidTargets();
	OutInvocations.Reset();

	const FGlobalModifierState CurrentModifiers =
		StateManager.GetModifierState();

	for (FRuntimeBinding& Binding : Bindings)
	{
		const bool bSatisfied =
			IsChordSatisfied(Binding.Definition.Chord, StateManager);

		if (Binding.bActive && !bSatisfied)
		{
			Binding.bActive = false;

			const FGlobalChordEventInfo EventInfo = MakeEventInfo(
				Binding,
				CurrentModifiers,
				KeyEvent.DeviceId,
				KeyEvent.TimestampSeconds);

			AddInvocation(
				Binding,
				Binding.Definition.CompletedFunctionName,
				EventInfo,
				OutInvocations);
			continue;
		}

		const bool bMainKeyStarted =
			!Binding.bActive &&
			bSatisfied &&
			KeyEvent.Key == Binding.Definition.Chord.Key &&
			KeyEvent.EventType == EGlobalInputEventType::Pressed &&
			!KeyEvent.bRepeat;

		if (!bMainKeyStarted)
		{
			continue;
		}

		Binding.bActive = true;
		Binding.ActivationDeviceId = KeyEvent.DeviceId;

		const FGlobalChordEventInfo EventInfo = MakeEventInfo(
			Binding,
			CurrentModifiers,
			KeyEvent.DeviceId,
			KeyEvent.TimestampSeconds);

		AddInvocation(
			Binding,
			Binding.Definition.StartedFunctionName,
			EventInfo,
			OutInvocations);
	}
}

void FGlobalChordBindingManager::GatherTriggered(
	const FInputStateManager& StateManager,
	double TimestampSeconds,
	TArray<FGlobalChordInvocation>& OutInvocations)
{
	check(IsInGameThread());

	RemoveInvalidTargets();
	OutInvocations.Reset();

	const FGlobalModifierState CurrentModifiers =
		StateManager.GetModifierState();

	for (FRuntimeBinding& Binding : Bindings)
	{
		if (!Binding.bActive)
		{
			continue;
		}

		if (!IsChordSatisfied(Binding.Definition.Chord, StateManager))
		{
			Binding.bActive = false;

			const FGlobalChordEventInfo EventInfo = MakeEventInfo(
				Binding,
				CurrentModifiers,
				0,
				TimestampSeconds);

			AddInvocation(
				Binding,
				Binding.Definition.CompletedFunctionName,
				EventInfo,
				OutInvocations);
			continue;
		}

		const FGlobalChordEventInfo EventInfo = MakeEventInfo(
			Binding,
			CurrentModifiers,
			Binding.ActivationDeviceId,
			TimestampSeconds);

		AddInvocation(
			Binding,
			Binding.Definition.TriggeredFunctionName,
			EventInfo,
			OutInvocations);
	}
}

void FGlobalChordBindingManager::Reset()
{
	check(IsInGameThread());
	Bindings.Reset();
}

int32 FGlobalChordBindingManager::Num() const
{
	return Bindings.Num();
}

int32 FGlobalChordBindingManager::NumForTarget(
	const UObject* Target) const
{
	int32 Count = 0;
	for (const FRuntimeBinding& Binding : Bindings)
	{
		if (Binding.Target.IsValid() &&
			Binding.Target.Get() == Target)
		{
			++Count;
		}
	}
	return Count;
}

bool FGlobalChordBindingManager::IsChordSatisfied(
	const FGlobalKeyChord& Chord,
	const FInputStateManager& StateManager)
{
	if (!Chord.Key.IsValid() || !StateManager.IsKeyDown(Chord.Key))
	{
		return false;
	}

	FGlobalModifierState Actual = StateManager.GetModifierState();
	FGlobalModifierState Required =
		Chord.GetEffectiveRequiredModifiers();

	// A modifier used as the main key is already enforced by IsKeyDown(Key).
	// Remove that modifier family from the aggregate modifier comparison so
	// an exact Left/Right Ctrl action does not reject its own Ctrl state.
	FGlobalKeyChord::RemoveMainKeyModifier(Chord.Key, Actual);

	if ((Required.bControl && !Actual.bControl) ||
		(Required.bAlt && !Actual.bAlt) ||
		(Required.bShift && !Actual.bShift) ||
		(Required.bCommand && !Actual.bCommand))
	{
		return false;
	}

	return !Chord.bExactModifiers || Actual == Required;
}

FGlobalChordEventInfo FGlobalChordBindingManager::MakeEventInfo(
	const FRuntimeBinding& Binding,
	const FGlobalModifierState& Modifiers,
	int64 DeviceId,
	double TimestampSeconds)
{
	FGlobalChordEventInfo EventInfo;
	EventInfo.Key = Binding.Definition.Chord.Key;
	EventInfo.Modifiers = Modifiers;
	EventInfo.DeviceId = DeviceId;
	EventInfo.TimestampSeconds = TimestampSeconds;
	return EventInfo;
}

void FGlobalChordBindingManager::AddInvocation(
	const FRuntimeBinding& Binding,
	FName FunctionName,
	const FGlobalChordEventInfo& EventInfo,
	TArray<FGlobalChordInvocation>& OutInvocations)
{
	if (FunctionName.IsNone())
	{
		return;
	}

	FGlobalChordInvocation& Invocation =
		OutInvocations.AddDefaulted_GetRef();
	Invocation.Target = Binding.Target;
	Invocation.BindingId = Binding.Definition.BindingId;
	Invocation.FunctionName = FunctionName;
	Invocation.EventInfo = EventInfo;
}

void FGlobalChordBindingManager::RemoveInvalidTargets()
{
	Bindings.RemoveAllSwap(
		[](const FRuntimeBinding& Binding)
		{
			return !Binding.Target.IsValid();
		},
		EAllowShrinking::No);
}
