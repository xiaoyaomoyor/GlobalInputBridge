#pragma once

#include "BlueprintNodeSignature.h"
#include "CoreMinimal.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "Input/GlobalInputTypes.h"
#include "K2Node.h"
#include "K2Node_GlobalInputActionEvent.generated.h"

class FBlueprintActionDatabaseRegistrar;
class FKismetCompilerContext;
class UEdGraph;
class UEdGraphPin;

/**
 * Focus-independent input action with an Enhanced Input-like lifecycle.
 * Activation is main-key-driven; hardware repeat never creates another Started.
 * Modifier keys are valid main keys and are not counted twice in Exact matching.
 */
UCLASS()
class GLOBALINPUTBRIDGEEDITOR_API UK2Node_GlobalInputActionEvent
	: public UK2Node
{
	GENERATED_BODY()

public:
	UK2Node_GlobalInputActionEvent(
		const FObjectInitializer& ObjectInitializer);

	/** The keyboard key, mouse button, or left/right modifier that starts this action. */
	UPROPERTY(EditAnywhere, Category="Global Input Action")
	FKey Key;

	/** Modifier families that must already be held when the main Key is pressed. */
	UPROPERTY(EditAnywhere, Category="Global Input Action")
	FGlobalModifierState Modifiers;

	/** Optional exclusive mode. Disabled matches traditional Action Event behavior. */
	UPROPERTY(EditAnywhere, Category="Global Input Action",
		meta=(DisplayName="Exact Modifiers (Exclusive)",
			ToolTip="When enabled, pressing an unselected modifier completes this action and prevents it from starting while that modifier is held. Disabled by default so independent input actions can overlap."))
	bool bExactModifiers = false;

	virtual void PostEditChangeProperty(
		FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual void AllocateDefaultPins() override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual FText GetTooltipText() const override;
	virtual bool IsCompatibleWithGraph(const UEdGraph* TargetGraph) const override;

	virtual bool ShouldShowNodeProperties() const override
	{
		return true;
	}

	virtual void ValidateNodeDuringCompilation(
		FCompilerResultsLog& MessageLog) const override;
	virtual void ExpandNode(
		FKismetCompilerContext& CompilerContext,
		UEdGraph* SourceGraph) override;
	virtual void GetMenuActions(
		FBlueprintActionDatabaseRegistrar& ActionRegistrar) const override;
	virtual FText GetMenuCategory() const override;

	UEdGraphPin* GetStartedPin() const;
	UEdGraphPin* GetTriggeredPin() const;
	UEdGraphPin* GetCompletedPin() const;
	UEdGraphPin* GetKeyInfoPin() const;

private:
	FGlobalKeyChord MakeChord() const;
	FText GetActionDisplayText() const;

private:
	mutable FNodeTextCache CachedNodeTitle;
	mutable FNodeTextCache CachedTooltip;
};
