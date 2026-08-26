#include "K2Node_GlobalInputActionEvent.h"

#include "BlueprintActionDatabaseRegistrar.h"
#include "BlueprintNodeSpawner.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Input/GlobalChordDelegateBinding.h"
#include "K2Node_AssignmentStatement.h"
#include "K2Node_GlobalInputActionEvent_Internal.h"
#include "K2Node_TemporaryVariable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "KismetCompiler.h"

#define LOCTEXT_NAMESPACE "K2Node_GlobalInputActionEvent"

namespace
{
	const FName StartedPinName(TEXT("Started"));
	const FName TriggeredPinName(TEXT("Triggered"));
	const FName CompletedPinName(TEXT("Completed"));
	const FName KeyInfoPinName(TEXT("KeyInfo"));
	const FName ChordSignatureName(
		TEXT("GlobalChordEventHandlerDynamicSignature__DelegateSignature"));

	const TCHAR* GetPhaseName(EGlobalInputActionBindingPhase Phase)
	{
		switch (Phase)
		{
		case EGlobalInputActionBindingPhase::Started:
			return TEXT("Started");
		case EGlobalInputActionBindingPhase::Triggered:
			return TEXT("Triggered");
		case EGlobalInputActionBindingPhase::Completed:
			return TEXT("Completed");
		default:
			return TEXT("Unknown");
		}
	}
}

UK2Node_GlobalInputActionEvent::UK2Node_GlobalInputActionEvent(
	const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UK2Node_GlobalInputActionEvent::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	CachedNodeTitle.Clear();
	CachedTooltip.Clear();

	if (UEdGraph* Graph = GetGraph())
	{
		Graph->NotifyGraphChanged();
	}
}

void UK2Node_GlobalInputActionEvent::AllocateDefaultPins()
{
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, StartedPinName);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, TriggeredPinName);
	CreatePin(EGPD_Output, UEdGraphSchema_K2::PC_Exec, CompletedPinName);
	CreatePin(
		EGPD_Output,
		UEdGraphSchema_K2::PC_Struct,
		FGlobalChordEventInfo::StaticStruct(),
		KeyInfoPinName);

	Super::AllocateDefaultPins();
}

FLinearColor UK2Node_GlobalInputActionEvent::GetNodeTitleColor() const
{
	return FLinearColor(0.55f, 0.12f, 0.80f);
}

FText UK2Node_GlobalInputActionEvent::GetNodeTitle(
	ENodeTitleType::Type TitleType) const
{
	if (TitleType == ENodeTitleType::MenuTitle)
	{
		return LOCTEXT("MenuTitle", "Global Input Action Event");
	}

	if (CachedNodeTitle.IsOutOfDate(this))
	{
		CachedNodeTitle.SetCachedText(
			FText::Format(
				LOCTEXT("NodeTitle", "Global Input Action: {0}"),
				GetActionDisplayText()),
			this);
	}

	return CachedNodeTitle;
}

FText UK2Node_GlobalInputActionEvent::GetTooltipText() const
{
	if (CachedTooltip.IsOutOfDate(this))
	{
		CachedTooltip.SetCachedText(
			LOCTEXT(
				"Tooltip",
				"A focus-independent, main-key-driven input action. The main Key may be a standalone modifier such as Left Ctrl. Started fires once, Triggered fires every active subsystem tick, and Completed fires when the main key or modifier conditions become invalid."),
			this);
	}

	return CachedTooltip;
}

bool UK2Node_GlobalInputActionEvent::IsCompatibleWithGraph(
	const UEdGraph* TargetGraph) const
{
	if (!TargetGraph ||
		TargetGraph->GetSchema()->GetGraphType(TargetGraph) !=
			EGraphType::GT_Ubergraph ||
		UEdGraphSchema_K2::IsConstructionScript(TargetGraph))
	{
		return false;
	}

	const UBlueprint* Blueprint =
		FBlueprintEditorUtils::FindBlueprintForGraph(TargetGraph);
	return Blueprint &&
		Blueprint->SupportsInputEvents() &&
		Super::IsCompatibleWithGraph(TargetGraph);
}

void UK2Node_GlobalInputActionEvent::ValidateNodeDuringCompilation(
	FCompilerResultsLog& MessageLog) const
{
	Super::ValidateNodeDuringCompilation(MessageLog);

	if (!Key.IsValid())
	{
		MessageLog.Error(
			*LOCTEXT(
				"InvalidKey",
				"Global Input Action Event has no valid main Key: @@")
				.ToString(),
			this);
	}
	else if (Key.IsAnalog())
	{
		MessageLog.Error(
			*LOCTEXT(
				"AnalogKey",
				"Global Input Action Event does not support analog keys: @@")
				.ToString(),
			this);
	}

	FGlobalModifierState EffectiveModifiers = Modifiers;
	FGlobalKeyChord::RemoveMainKeyModifier(Key, EffectiveModifiers);
	if (EffectiveModifiers != Modifiers)
	{
		MessageLog.Warning(
			*LOCTEXT(
				"RedundantMainKeyModifier",
				"Global Input Action Event ignores the modifier option represented by its main Key because that Key is already matched directly: @@")
				.ToString(),
			this);
	}
}

void UK2Node_GlobalInputActionEvent::ExpandNode(
	FKismetCompilerContext& CompilerContext,
	UEdGraph* SourceGraph)
{
	Super::ExpandNode(CompilerContext, SourceGraph);

	struct FActivePhase
	{
		UEdGraphPin* Pin = nullptr;
		EGlobalInputActionBindingPhase Phase =
			EGlobalInputActionBindingPhase::Started;
	};

	TArray<FActivePhase> ActivePhases;
	if (GetStartedPin()->LinkedTo.Num() > 0)
	{
		ActivePhases.Add(
			{GetStartedPin(), EGlobalInputActionBindingPhase::Started});
	}
	if (GetTriggeredPin()->LinkedTo.Num() > 0)
	{
		ActivePhases.Add(
			{GetTriggeredPin(), EGlobalInputActionBindingPhase::Triggered});
	}
	if (GetCompletedPin()->LinkedTo.Num() > 0)
	{
		ActivePhases.Add(
			{GetCompletedPin(), EGlobalInputActionBindingPhase::Completed});
	}

	if (ActivePhases.Num() == 0)
	{
		BreakAllNodeLinks();
		return;
	}

	const UEdGraphSchema_K2* Schema = CompilerContext.GetSchema();
	const FGuid RuntimeBindingId = NodeGuid.IsValid()
		? NodeGuid
		: FGuid::NewGuid();

	UK2Node_TemporaryVariable* KeyInfoVariable = nullptr;
	if (ActivePhases.Num() > 1)
	{
		KeyInfoVariable =
			CompilerContext.SpawnIntermediateNode<UK2Node_TemporaryVariable>(
				this,
				SourceGraph);
		KeyInfoVariable->VariableType.PinCategory =
			UEdGraphSchema_K2::PC_Struct;
		KeyInfoVariable->VariableType.PinSubCategoryObject =
			FGlobalChordEventInfo::StaticStruct();
		KeyInfoVariable->AllocateDefaultPins();
	}

	for (const FActivePhase& ActivePhase : ActivePhases)
	{
		UK2Node_GlobalInputActionEvent_Internal* InternalEvent =
			CompilerContext.SpawnIntermediateNode<
				UK2Node_GlobalInputActionEvent_Internal>(this, SourceGraph);

		InternalEvent->CustomFunctionName = FName(*FString::Printf(
			TEXT("GlobalInputActionEvt_%s_%s_%s"),
			*Key.ToString(),
			GetPhaseName(ActivePhase.Phase),
			*InternalEvent->GetName()));
		InternalEvent->Chord = MakeChord();
		InternalEvent->BindingId = RuntimeBindingId;
		InternalEvent->Phase = ActivePhase.Phase;
		InternalEvent->EventReference.SetExternalDelegateMember(
			ChordSignatureName);
		InternalEvent->AllocateDefaultPins();

		UEdGraphPin* InternalExecPin =
			Schema->FindExecutionPin(*InternalEvent, EGPD_Output);
		UEdGraphPin* InternalInfoPin =
			InternalEvent->FindPinChecked(KeyInfoPinName);

		if (KeyInfoVariable)
		{
			UK2Node_AssignmentStatement* Assignment =
				CompilerContext.SpawnIntermediateNode<
					UK2Node_AssignmentStatement>(this, SourceGraph);
			Assignment->AllocateDefaultPins();

			Schema->TryCreateConnection(
				KeyInfoVariable->GetVariablePin(),
				Assignment->GetVariablePin());
			Schema->TryCreateConnection(
				InternalInfoPin,
				Assignment->GetValuePin());
			Schema->TryCreateConnection(
				InternalExecPin,
				Assignment->GetExecPin());

			CompilerContext.MovePinLinksToIntermediate(
				*ActivePhase.Pin,
				*Assignment->GetThenPin());
		}
		else
		{
			CompilerContext.MovePinLinksToIntermediate(
				*ActivePhase.Pin,
				*InternalExecPin);
			CompilerContext.MovePinLinksToIntermediate(
				*GetKeyInfoPin(),
				*InternalInfoPin);
		}
	}

	if (KeyInfoVariable)
	{
		CompilerContext.MovePinLinksToIntermediate(
			*GetKeyInfoPin(),
			*KeyInfoVariable->GetVariablePin());
	}

	BreakAllNodeLinks();
}

void UK2Node_GlobalInputActionEvent::GetMenuActions(
	FBlueprintActionDatabaseRegistrar& ActionRegistrar) const
{
	UClass* ActionKey = GetClass();
	if (!ActionRegistrar.IsOpenForRegistration(ActionKey))
	{
		return;
	}

	UBlueprintNodeSpawner* NodeSpawner =
		UBlueprintNodeSpawner::Create(GetClass());
	check(NodeSpawner);
	ActionRegistrar.AddBlueprintAction(ActionKey, NodeSpawner);
}

FText UK2Node_GlobalInputActionEvent::GetMenuCategory() const
{
	return LOCTEXT("MenuCategory", "Global Input|Action Events");
}

UEdGraphPin* UK2Node_GlobalInputActionEvent::GetStartedPin() const
{
	return FindPinChecked(StartedPinName);
}

UEdGraphPin* UK2Node_GlobalInputActionEvent::GetTriggeredPin() const
{
	return FindPinChecked(TriggeredPinName);
}

UEdGraphPin* UK2Node_GlobalInputActionEvent::GetCompletedPin() const
{
	return FindPinChecked(CompletedPinName);
}

UEdGraphPin* UK2Node_GlobalInputActionEvent::GetKeyInfoPin() const
{
	return FindPinChecked(KeyInfoPinName);
}

FGlobalKeyChord UK2Node_GlobalInputActionEvent::MakeChord() const
{
	FGlobalKeyChord Chord;
	Chord.Key = Key;
	Chord.RequiredModifiers = Modifiers;
	Chord.bExactModifiers = bExactModifiers;
	return Chord;
}

FText UK2Node_GlobalInputActionEvent::GetActionDisplayText() const
{
	TArray<FString> Parts;
	FGlobalModifierState DisplayModifiers = Modifiers;
	FGlobalKeyChord::RemoveMainKeyModifier(Key, DisplayModifiers);

	if (DisplayModifiers.bControl)
	{
		Parts.Add(TEXT("Ctrl"));
	}
	if (DisplayModifiers.bAlt)
	{
		Parts.Add(TEXT("Alt"));
	}
	if (DisplayModifiers.bShift)
	{
		Parts.Add(TEXT("Shift"));
	}
	if (DisplayModifiers.bCommand)
	{
		Parts.Add(TEXT("Win"));
	}

	Parts.Add(Key.IsValid()
		? Key.GetDisplayName().ToString()
		: LOCTEXT("UnboundKey", "Unbound").ToString());

	return FText::FromString(FString::Join(Parts, TEXT(" + ")));
}

#undef LOCTEXT_NAMESPACE
