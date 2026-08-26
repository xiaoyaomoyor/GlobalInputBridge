#if WITH_DEV_AUTOMATION_TESTS

#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Input/GlobalChordDelegateBinding.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_GlobalInputActionEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/AutomationTest.h"
#include "Subsystem/GlobalInputSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGlobalInputActionEditorNodeCompilationTest,
	"GlobalInputBridge.Editor.InputActionNodeCompilation",
	EAutomationTestFlags::EditorContext |
	EAutomationTestFlags::EngineFilter)

bool FGlobalInputActionEditorNodeCompilationTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;

	const FName BlueprintName = MakeUniqueObjectName(
		GetTransientPackage(),
		UBlueprint::StaticClass(),
		TEXT("BP_GlobalInputActionNodeTest"));

	UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(
		AActor::StaticClass(),
		GetTransientPackage(),
		BlueprintName,
		BPTYPE_Normal,
		UBlueprint::StaticClass(),
		UBlueprintGeneratedClass::StaticClass(),
		NAME_None);

	if (!TestNotNull(TEXT("Transient actor blueprint is created"), Blueprint))
	{
		return false;
	}

	UEdGraph* EventGraph =
		FBlueprintEditorUtils::FindEventGraph(Blueprint);
	if (!TestNotNull(TEXT("Transient blueprint has an event graph"), EventGraph))
	{
		return false;
	}

	FGraphNodeCreator<UK2Node_GlobalInputActionEvent> ActionNodeCreator(
		*EventGraph);
	UK2Node_GlobalInputActionEvent* ActionNode =
		ActionNodeCreator.CreateNode();
	ActionNode->Key = EKeys::R;
	ActionNode->Modifiers.bControl = true;
	ActionNodeCreator.Finalize();
	TestFalse(
		TEXT("Input action nodes default to non-exclusive modifiers"),
		ActionNode->bExactModifiers);

	TestEqual(
		TEXT("Node uses the new palette name"),
		ActionNode->GetNodeTitle(ENodeTitleType::MenuTitle).ToString(),
		FString(TEXT("Global Input Action Event")));
	TestTrue(
		TEXT("Node uses the distinguishing purple title color"),
		ActionNode->GetNodeTitleColor().Equals(
			FLinearColor(0.55f, 0.12f, 0.80f)));

	auto AddSequenceNode = [EventGraph]()
	{
		FGraphNodeCreator<UK2Node_ExecutionSequence> Creator(*EventGraph);
		UK2Node_ExecutionSequence* Node = Creator.CreateNode();
		Creator.Finalize();
		return Node;
	};

	UK2Node_ExecutionSequence* StartedSequence = AddSequenceNode();
	UK2Node_ExecutionSequence* TriggeredSequence = AddSequenceNode();
	UK2Node_ExecutionSequence* CompletedSequence = AddSequenceNode();

	const UEdGraphSchema_K2* Schema =
		CastChecked<UEdGraphSchema_K2>(EventGraph->GetSchema());

	TestTrue(
		TEXT("Started output can be connected"),
		Schema->TryCreateConnection(
			ActionNode->GetStartedPin(),
			StartedSequence->FindPinChecked(UEdGraphSchema_K2::PN_Execute)));
	TestTrue(
		TEXT("Triggered output can be connected"),
		Schema->TryCreateConnection(
			ActionNode->GetTriggeredPin(),
			TriggeredSequence->FindPinChecked(UEdGraphSchema_K2::PN_Execute)));
	TestTrue(
		TEXT("Completed output can be connected"),
		Schema->TryCreateConnection(
			ActionNode->GetCompletedPin(),
			CompletedSequence->FindPinChecked(UEdGraphSchema_K2::PN_Execute)));

	FKismetEditorUtilities::CompileBlueprint(
		Blueprint,
		EBlueprintCompileOptions::SkipGarbageCollection);

	TestEqual(
		TEXT("Blueprint containing Global Input Action Event compiles"),
		Blueprint->Status,
		BS_UpToDate);

	UGlobalChordDelegateBinding* DynamicBinding =
		Cast<UGlobalChordDelegateBinding>(
			UBlueprintGeneratedClass::GetDynamicBindingObject(
				Blueprint->GeneratedClass,
				UGlobalChordDelegateBinding::StaticClass()));

	if (!TestNotNull(TEXT("Compiled chord binding object exists"), DynamicBinding))
	{
		return false;
	}

	TestEqual(
		TEXT("Three phase events merge into one runtime action binding"),
		DynamicBinding->ChordBindings.Num(),
		1);

	const FGlobalChordBlueprintBinding* Binding =
		DynamicBinding->ChordBindings.FindByPredicate(
			[](const FGlobalChordBlueprintBinding& Candidate)
			{
				return Candidate.Chord.Key == EKeys::R;
			});
	if (TestNotNull(
		TEXT("Compiled new action binding exists"),
		Binding))
	{
		TestEqual(TEXT("Compiled binding keeps main key"),
			Binding->Chord.Key, EKeys::R);
		TestTrue(TEXT("Compiled binding keeps Control modifier"),
			Binding->Chord.RequiredModifiers.bControl);
		TestFalse(TEXT("Compiled binding keeps non-exclusive default"),
			Binding->Chord.bExactModifiers);
		TestFalse(TEXT("Started function is generated"),
			Binding->StartedFunctionName.IsNone());
		TestFalse(TEXT("Triggered function is generated"),
			Binding->TriggeredFunctionName.IsNone());
		TestFalse(TEXT("Completed function is generated"),
			Binding->CompletedFunctionName.IsNone());
	}

	UGlobalInputSubsystem* Subsystem = GEngine
		? GEngine->GetEngineSubsystem<UGlobalInputSubsystem>()
		: nullptr;
	if (!TestNotNull(TEXT("Global input subsystem exists"), Subsystem))
	{
		return false;
	}

	UWorld* TestWorld = UWorld::CreateWorld(
		EWorldType::Game,
		false,
		MakeUniqueObjectName(
			GetTransientPackage(),
			UWorld::StaticClass(),
			TEXT("GlobalChordBindingWorld")));
	if (!TestNotNull(TEXT("Transient game world is created"), TestWorld))
	{
		return false;
	}

	AActor* RuntimeActor = TestWorld->SpawnActor<AActor>(
		Blueprint->GeneratedClass,
		FTransform::Identity);
	if (!TestNotNull(TEXT("Compiled chord actor is spawned"), RuntimeActor))
	{
		TestWorld->DestroyWorld(false);
		return false;
	}

	TestEqual(
		TEXT("Normal game-world construction registers action binding once"),
		Subsystem->GetRegisteredGlobalChordBindingCountForTarget(RuntimeActor),
		1);

	Subsystem->UnregisterGlobalChordBindings(RuntimeActor);
	TestEqual(
		TEXT("PIE-duplicate simulation begins without external binding"),
		Subsystem->GetRegisteredGlobalChordBindingCountForTarget(RuntimeActor),
		0);

	Subsystem->RegisterGlobalChordBindingsForWorld(TestWorld);
	TestEqual(
		TEXT("World initialization scan restores copied actor binding"),
		Subsystem->GetRegisteredGlobalChordBindingCountForTarget(RuntimeActor),
		1);

	Subsystem->RegisterGlobalChordBindingsForWorld(TestWorld);
	TestEqual(
		TEXT("Repeated world scans keep action registration idempotent"),
		Subsystem->GetRegisteredGlobalChordBindingCountForTarget(RuntimeActor),
		1);

	Subsystem->HandleWorldCleanup(TestWorld, true, true);
	TestEqual(
		TEXT("World cleanup removes chord registrations"),
		Subsystem->GetRegisteredGlobalChordBindingCountForTarget(RuntimeActor),
		0);

	TestWorld->DestroyWorld(false);

	return true;
}

#endif
