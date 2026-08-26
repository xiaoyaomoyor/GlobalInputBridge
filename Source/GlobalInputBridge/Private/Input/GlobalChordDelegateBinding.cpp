#include "Input/GlobalChordDelegateBinding.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Subsystem/GlobalInputSubsystem.h"

void UGlobalChordDelegateBinding::BindDynamicDelegates(
	UObject* InInstance) const
{
	if (!GEngine || !IsValid(InInstance) || InInstance->IsTemplate())
	{
		return;
	}

	UWorld* World = InInstance->GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return;
	}

	UGlobalInputSubsystem* Subsystem =
		GEngine->GetEngineSubsystem<UGlobalInputSubsystem>();
	if (!Subsystem)
	{
		return;
	}

	for (const FGlobalChordBlueprintBinding& Binding : ChordBindings)
	{
		Subsystem->RegisterGlobalChordBinding(InInstance, Binding);
	}
}

void UGlobalChordDelegateBinding::UnbindDynamicDelegates(
	UObject* InInstance) const
{
	if (!GEngine || !InInstance)
	{
		return;
	}

	if (UGlobalInputSubsystem* Subsystem =
		GEngine->GetEngineSubsystem<UGlobalInputSubsystem>())
	{
		Subsystem->UnregisterGlobalChordBindings(InInstance);
	}
}
