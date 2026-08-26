// GlobalChordDelegateBinding.h
//对于蓝图来说体验像Event Global Chord，本质上是Register Callback
#pragma once

#include "CoreMinimal.h"
#include "Engine/DynamicBlueprintBinding.h"
#include "Input/GlobalInputTypes.h"
#include "GlobalChordDelegateBinding.generated.h"

/** Signature used by the compiler-generated internal chord event functions. */
DECLARE_DYNAMIC_DELEGATE_OneParam(
	FGlobalChordEventHandlerDynamicSignature,
	const FGlobalChordEventInfo&,
	KeyInfo);

/** Compile-time data emitted by a Global Input Action Event blueprint node. */
USTRUCT()
struct GLOBALINPUTBRIDGE_API FGlobalChordBlueprintBinding
{
	GENERATED_BODY()

	UPROPERTY()
	FGuid BindingId;

	UPROPERTY()
	FGlobalKeyChord Chord;

	UPROPERTY()
	FName StartedFunctionName;

	UPROPERTY()
	FName TriggeredFunctionName;

	UPROPERTY()
	FName CompletedFunctionName;
};

/** Automatically registers compiled input action nodes for each blueprint instance. */
UCLASS()
class GLOBALINPUTBRIDGE_API UGlobalChordDelegateBinding
	: public UDynamicBlueprintBinding
{
	GENERATED_BODY()

public:
	virtual void BindDynamicDelegates(UObject* InInstance) const override;
	virtual void UnbindDynamicDelegates(UObject* InInstance) const override;

	UPROPERTY()
	TArray<FGlobalChordBlueprintBinding> ChordBindings;
};
