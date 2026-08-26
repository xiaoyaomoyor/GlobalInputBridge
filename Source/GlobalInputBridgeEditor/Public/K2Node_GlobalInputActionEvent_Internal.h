#pragma once

#include "CoreMinimal.h"
#include "Input/GlobalInputTypes.h"
#include "K2Node_Event.h"
#include "K2Node_GlobalInputActionEvent_Internal.generated.h"

UENUM()
enum class EGlobalInputActionBindingPhase : uint8
{
	Started,
	Triggered,
	Completed
};

/** Compiler-only event node that emits one phase function and binding record. */
UCLASS()
class GLOBALINPUTBRIDGEEDITOR_API UK2Node_GlobalInputActionEvent_Internal
	: public UK2Node_Event
{
	GENERATED_BODY()

public:
	UK2Node_GlobalInputActionEvent_Internal(
		const FObjectInitializer& ObjectInitializer);

	virtual UClass* GetDynamicBindingClass() const override;
	virtual void RegisterDynamicBinding(
		UDynamicBlueprintBinding* BindingObject) const override;

	UPROPERTY()
	FGlobalKeyChord Chord;

	UPROPERTY()
	FGuid BindingId;

	UPROPERTY()
	EGlobalInputActionBindingPhase Phase =
		EGlobalInputActionBindingPhase::Started;
};
