#include "K2Node_GlobalInputActionEvent_Internal.h"

#include "Engine/DynamicBlueprintBinding.h"
#include "Input/GlobalChordDelegateBinding.h"

UK2Node_GlobalInputActionEvent_Internal::UK2Node_GlobalInputActionEvent_Internal(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bInternalEvent = true;
}

UClass* UK2Node_GlobalInputActionEvent_Internal::GetDynamicBindingClass() const
{
	return UGlobalChordDelegateBinding::StaticClass();
}

void UK2Node_GlobalInputActionEvent_Internal::RegisterDynamicBinding(
	UDynamicBlueprintBinding* BindingObject) const
{
	UGlobalChordDelegateBinding* ChordBindingObject =
		CastChecked<UGlobalChordDelegateBinding>(BindingObject);

	FGlobalChordBlueprintBinding* Binding =
		ChordBindingObject->ChordBindings.FindByPredicate(
		[this](const FGlobalChordBlueprintBinding& Candidate)
			{
				return Candidate.BindingId == BindingId;
			});

	if (!Binding)
	{
		Binding = &ChordBindingObject->ChordBindings.AddDefaulted_GetRef();
		Binding->BindingId = BindingId;
		Binding->Chord = Chord;
	}

	switch (Phase)
	{
	case EGlobalInputActionBindingPhase::Started:
		Binding->StartedFunctionName = CustomFunctionName;
		break;

	case EGlobalInputActionBindingPhase::Triggered:
		Binding->TriggeredFunctionName = CustomFunctionName;
		break;

	case EGlobalInputActionBindingPhase::Completed:
		Binding->CompletedFunctionName = CustomFunctionName;
		break;
	}
}
