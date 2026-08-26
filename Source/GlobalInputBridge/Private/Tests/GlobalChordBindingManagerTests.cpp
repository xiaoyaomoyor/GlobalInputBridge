#if WITH_DEV_AUTOMATION_TESTS

#include "Input/GlobalChordBindingManager.h"
#include "Input/InputStateManager.h"
#include "Misc/AutomationTest.h"
#include "Settings/GlobalInputSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGlobalChordBindingManagerLifecycleTest,
	"GlobalInputBridge.Input.Chord.MainKeyLifecycle",
	EAutomationTestFlags::EditorContext |
	EAutomationTestFlags::EngineFilter)

bool FGlobalChordBindingManagerLifecycleTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;

	FInputStateManager State;
	FGlobalChordBindingManager Chords;
	UObject* Target = NewObject<UGlobalInputSettings>();
	constexpr int64 Keyboard = 101;

	FGlobalChordBlueprintBinding Binding;
	Binding.BindingId = FGuid::NewGuid();
	Binding.Chord.Key = EKeys::R;
	Binding.Chord.RequiredModifiers.bControl = true;
	Binding.Chord.bExactModifiers = true;
	Binding.StartedFunctionName = TEXT("ChordStarted");
	Binding.TriggeredFunctionName = TEXT("ChordTriggered");
	Binding.CompletedFunctionName = TEXT("ChordCompleted");
	Chords.RegisterBinding(Target, Binding);
	TestEqual(TEXT("Chord binding is registered"), Chords.Num(), 1);

	TArray<FGlobalChordInvocation> Invocations;
	FGlobalKeyEvent Event;
	State.BeginFrame();

	TestTrue(TEXT("Control press is accepted by state"),
		State.ProcessKeyEvent(
			EKeys::LeftControl, true, Keyboard, 1.0, Event));
	Chords.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Modifier first does not start chord"),
		Invocations.Num(), 0);

	TestTrue(TEXT("Main key press is accepted by state"),
		State.ProcessKeyEvent(EKeys::R, true, Keyboard, 2.0, Event));
	Chords.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Main key starts chord once"), Invocations.Num(), 1);
	TestEqual(TEXT("Started callback is selected"),
		Invocations[0].FunctionName, Binding.StartedFunctionName);
	TestEqual(TEXT("Started info carries activation device"),
		Invocations[0].EventInfo.DeviceId, Keyboard);

	Chords.GatherTriggered(State, 2.1, Invocations);
	TestEqual(TEXT("Active chord triggers once per evaluation"),
		Invocations.Num(), 1);
	TestEqual(TEXT("Triggered callback is selected"),
		Invocations[0].FunctionName, Binding.TriggeredFunctionName);
	TestEqual(TEXT("Triggered timestamp is refreshed"),
		Invocations[0].EventInfo.TimestampSeconds, 2.1);

	TestTrue(TEXT("Hardware repeat is retained by state"),
		State.ProcessKeyEvent(EKeys::R, true, Keyboard, 2.2, Event));
	TestTrue(TEXT("Repeated event is marked repeat"), Event.bRepeat);
	Chords.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Hardware repeat does not restart chord"),
		Invocations.Num(), 0);

	TestTrue(TEXT("Control release is accepted by state"),
		State.ProcessKeyEvent(
			EKeys::LeftControl, false, Keyboard, 3.0, Event));
	Chords.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Required modifier release completes chord"),
		Invocations.Num(), 1);
	TestEqual(TEXT("Completed callback is selected"),
		Invocations[0].FunctionName, Binding.CompletedFunctionName);

	TestTrue(TEXT("Control can be pressed while main key remains down"),
		State.ProcessKeyEvent(
			EKeys::LeftControl, true, Keyboard, 4.0, Event));
	Chords.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Reverse order does not restart main-key-driven chord"),
		Invocations.Num(), 0);

	Chords.GatherTriggered(State, 4.1, Invocations);
	TestEqual(TEXT("Inactive reverse-order chord does not trigger"),
		Invocations.Num(), 0);

	Chords.UnregisterBindings(Target);
	TestEqual(TEXT("Target chord bindings are unregistered"),
		Chords.Num(), 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGlobalChordBindingManagerCompletionTest,
	"GlobalInputBridge.Input.Chord.ExactAndLifecycleCompletion",
	EAutomationTestFlags::EditorContext |
	EAutomationTestFlags::EngineFilter)

bool FGlobalChordBindingManagerCompletionTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;

	FInputStateManager State;
	FGlobalChordBindingManager Chords;
	UObject* Target = NewObject<UGlobalInputSettings>();
	constexpr int64 Keyboard = 202;

	FGlobalChordBlueprintBinding Binding;
	Binding.BindingId = FGuid::NewGuid();
	Binding.Chord.Key = EKeys::R;
	Binding.Chord.RequiredModifiers.bControl = true;
	Binding.Chord.bExactModifiers = true;
	Binding.StartedFunctionName = TEXT("ChordStarted");
	Binding.CompletedFunctionName = TEXT("ChordCompleted");
	Chords.RegisterBinding(Target, Binding);

	TArray<FGlobalChordInvocation> Invocations;
	FGlobalKeyEvent Event;
	State.BeginFrame();

	State.ProcessKeyEvent(
		EKeys::LeftControl, true, Keyboard, 1.0, Event);
	Chords.ProcessKeyEvent(Event, State, Invocations);
	State.ProcessKeyEvent(EKeys::R, true, Keyboard, 1.1, Event);
	Chords.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Exact chord starts"), Invocations.Num(), 1);

	State.ProcessKeyEvent(EKeys::LeftShift, true, Keyboard, 1.2, Event);
	Chords.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Unexpected exact modifier completes chord"),
		Invocations.Num(), 1);
	TestEqual(TEXT("Exact completion callback is selected"),
		Invocations[0].FunctionName, Binding.CompletedFunctionName);

	State.ProcessKeyEvent(EKeys::LeftShift, false, Keyboard, 1.3, Event);
	Chords.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Restoring modifiers while main key is held does not restart"),
		Invocations.Num(), 0);

	State.ProcessKeyEvent(EKeys::R, false, Keyboard, 1.4, Event);
	Chords.ProcessKeyEvent(Event, State, Invocations);
	State.ProcessKeyEvent(EKeys::R, true, Keyboard, 1.5, Event);
	Chords.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Fresh main key press restarts chord"),
		Invocations.Num(), 1);

	TArray<FGlobalKeyEvent> StopEvents;
	State.ReleaseAllKeys(2.0, StopEvents);
	for (const FGlobalKeyEvent& StopEvent : StopEvents)
	{
		Chords.ProcessKeyEvent(StopEvent, State, Invocations);
		if (Invocations.Num() > 0)
		{
			break;
		}
	}

	TestEqual(TEXT("Lifecycle release completes active chord"),
		Invocations.Num(), 1);
	TestEqual(TEXT("Lifecycle completion uses synthetic device"),
		Invocations[0].EventInfo.DeviceId, static_cast<int64>(0));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGlobalInputActionModifierMainKeyTest,
	"GlobalInputBridge.Input.Action.ModifierMainKey",
	EAutomationTestFlags::EditorContext |
	EAutomationTestFlags::EngineFilter)

bool FGlobalInputActionModifierMainKeyTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;

	FInputStateManager State;
	FGlobalChordBindingManager Actions;
	UObject* Target = NewObject<UGlobalInputSettings>();
	constexpr int64 Keyboard = 303;

	TArray<FGlobalChordInvocation> Invocations;
	FGlobalKeyEvent Event;
	State.BeginFrame();

	FGlobalChordBlueprintBinding StandaloneBinding;
	StandaloneBinding.BindingId = FGuid::NewGuid();
	StandaloneBinding.Chord.Key = EKeys::LeftControl;
	StandaloneBinding.Chord.bExactModifiers = true;
	StandaloneBinding.StartedFunctionName = TEXT("StandaloneStarted");
	StandaloneBinding.CompletedFunctionName = TEXT("StandaloneCompleted");
	Actions.RegisterBinding(Target, StandaloneBinding);

	TestTrue(TEXT("Standalone Control main-key press is accepted"),
		State.ProcessKeyEvent(
			EKeys::LeftControl, true, Keyboard, 0.5, Event));
	Actions.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Control alone starts an exact action"),
		Invocations.Num(), 1);
	TestEqual(TEXT("Standalone modifier selects Started callback"),
		Invocations[0].FunctionName,
		StandaloneBinding.StartedFunctionName);

	TestTrue(TEXT("Standalone Control release is accepted"),
		State.ProcessKeyEvent(
			EKeys::LeftControl, false, Keyboard, 0.6, Event));
	Actions.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Control release completes standalone action"),
		Invocations.Num(), 1);
	TestEqual(TEXT("Standalone modifier selects Completed callback"),
		Invocations[0].FunctionName,
		StandaloneBinding.CompletedFunctionName);

	Actions.UnregisterBindings(Target);
	State.Reset();
	State.BeginFrame();

	FGlobalChordBlueprintBinding Binding;
	Binding.BindingId = FGuid::NewGuid();
	Binding.Chord.Key = EKeys::LeftControl;
	// Redundant on purpose: the main Control key must not be counted twice.
	Binding.Chord.RequiredModifiers.bControl = true;
	Binding.Chord.RequiredModifiers.bShift = true;
	Binding.Chord.bExactModifiers = true;
	Binding.StartedFunctionName = TEXT("ActionStarted");
	Binding.TriggeredFunctionName = TEXT("ActionTriggered");
	Binding.CompletedFunctionName = TEXT("ActionCompleted");
	Actions.RegisterBinding(Target, Binding);

	TestTrue(TEXT("Required Shift press is accepted"),
		State.ProcessKeyEvent(
			EKeys::LeftShift, true, Keyboard, 1.0, Event));
	Actions.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Required modifier alone does not start action"),
		Invocations.Num(), 0);

	TestTrue(TEXT("Control main-key press is accepted"),
		State.ProcessKeyEvent(
			EKeys::LeftControl, true, Keyboard, 1.1, Event));
	Actions.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Modifier main key starts exact action"),
		Invocations.Num(), 1);
	TestEqual(TEXT("Modifier action selects Started callback"),
		Invocations[0].FunctionName, Binding.StartedFunctionName);

	Actions.GatherTriggered(State, 1.2, Invocations);
	TestEqual(TEXT("Modifier main-key action remains triggered"),
		Invocations.Num(), 1);

	TestTrue(TEXT("Unexpected Alt press is accepted"),
		State.ProcessKeyEvent(
			EKeys::LeftAlt, true, Keyboard, 1.3, Event));
	Actions.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Unexpected exact modifier completes action"),
		Invocations.Num(), 1);
	TestEqual(TEXT("Exact completion callback is selected"),
		Invocations[0].FunctionName, Binding.CompletedFunctionName);

	State.ProcessKeyEvent(EKeys::LeftAlt, false, Keyboard, 1.4, Event);
	Actions.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Removing extra modifier does not restart held main key"),
		Invocations.Num(), 0);

	State.ProcessKeyEvent(EKeys::LeftControl, false, Keyboard, 1.5, Event);
	Actions.ProcessKeyEvent(Event, State, Invocations);
	State.ProcessKeyEvent(EKeys::LeftControl, true, Keyboard, 1.6, Event);
	Actions.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Fresh modifier main-key press restarts action"),
		Invocations.Num(), 1);

	State.ProcessKeyEvent(EKeys::LeftShift, false, Keyboard, 1.7, Event);
	Actions.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Required modifier release completes action"),
		Invocations.Num(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGlobalInputActionConcurrentStandaloneActionsTest,
	"GlobalInputBridge.Input.Action.ConcurrentStandaloneActions",
	EAutomationTestFlags::EditorContext |
	EAutomationTestFlags::EngineFilter)

bool FGlobalInputActionConcurrentStandaloneActionsTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;

	FInputStateManager State;
	FGlobalChordBindingManager Actions;
	UObject* Target = NewObject<UGlobalInputSettings>();
	constexpr int64 Keyboard = 404;

	auto RegisterAction = [this, &Actions, Target](
		const FKey& Key,
		const TCHAR* NamePrefix)
	{
		FGlobalChordBlueprintBinding Binding;
		Binding.BindingId = FGuid::NewGuid();
		Binding.Chord.Key = Key;
		Binding.StartedFunctionName = FName(
			*FString::Printf(TEXT("%sStarted"), NamePrefix));
		Binding.TriggeredFunctionName = FName(
			*FString::Printf(TEXT("%sTriggered"), NamePrefix));
		Binding.CompletedFunctionName = FName(
			*FString::Printf(TEXT("%sCompleted"), NamePrefix));

		TestFalse(
			*FString::Printf(
				TEXT("%s action defaults to non-exclusive modifiers"),
				NamePrefix),
			Binding.Chord.bExactModifiers);
		Actions.RegisterBinding(Target, Binding);
	};

	RegisterAction(EKeys::LeftShift, TEXT("Shift"));
	RegisterAction(EKeys::LeftControl, TEXT("Control"));
	RegisterAction(EKeys::LeftMouseButton, TEXT("Mouse"));
	RegisterAction(EKeys::W, TEXT("Keyboard"));
	RegisterAction(EKeys::LeftAlt, TEXT("Alt"));
	RegisterAction(EKeys::LeftCommand, TEXT("Command"));
	TestEqual(TEXT("All independent actions are registered"),
		Actions.Num(), 6);

	TArray<FGlobalChordInvocation> Invocations;
	FGlobalKeyEvent Event;
	State.BeginFrame();

	auto ProcessAndExpectSingle =
		[this, &State, &Actions, &Invocations, &Event](
			const FKey& Key,
			bool bPressed,
			int64 DeviceId,
			double Timestamp,
			FName ExpectedFunction)
	{
		TestTrue(
			*FString::Printf(TEXT("%s transition is accepted"),
				*Key.ToString()),
			State.ProcessKeyEvent(
				Key,
				bPressed,
				DeviceId,
				Timestamp,
				Event));
		Actions.ProcessKeyEvent(Event, State, Invocations);
		if (TestEqual(
			*FString::Printf(TEXT("%s transition emits one action callback"),
				*Key.ToString()),
			Invocations.Num(),
			1))
		{
			TestEqual(
				*FString::Printf(TEXT("%s selects the expected callback"),
					*Key.ToString()),
				Invocations[0].FunctionName,
				ExpectedFunction);
		}
	};

	// Modifier -> modifier, modifier -> mouse, and modifier -> keyboard.
	ProcessAndExpectSingle(EKeys::LeftShift, true, Keyboard, 1.0,
		TEXT("ShiftStarted"));
	ProcessAndExpectSingle(EKeys::LeftControl, true, Keyboard, 1.1,
		TEXT("ControlStarted"));
	ProcessAndExpectSingle(EKeys::LeftMouseButton, true, 0, 1.2,
		TEXT("MouseStarted"));
	ProcessAndExpectSingle(EKeys::W, true, Keyboard, 1.3,
		TEXT("KeyboardStarted"));

	// Mouse/keyboard actions are already active when more modifiers arrive.
	ProcessAndExpectSingle(EKeys::LeftAlt, true, Keyboard, 1.4,
		TEXT("AltStarted"));
	ProcessAndExpectSingle(EKeys::LeftCommand, true, Keyboard, 1.5,
		TEXT("CommandStarted"));

	Actions.GatherTriggered(State, 1.6, Invocations);
	TestEqual(TEXT("All overlapping actions remain active"),
		Invocations.Num(), 6);

	ProcessAndExpectSingle(EKeys::LeftControl, false, Keyboard, 2.0,
		TEXT("ControlCompleted"));
	Actions.GatherTriggered(State, 2.1, Invocations);
	TestEqual(TEXT("Releasing Control does not complete other actions"),
		Invocations.Num(), 5);

	ProcessAndExpectSingle(EKeys::LeftShift, false, Keyboard, 2.2,
		TEXT("ShiftCompleted"));
	ProcessAndExpectSingle(EKeys::LeftMouseButton, false, 0, 2.3,
		TEXT("MouseCompleted"));
	ProcessAndExpectSingle(EKeys::W, false, Keyboard, 2.4,
		TEXT("KeyboardCompleted"));
	ProcessAndExpectSingle(EKeys::LeftAlt, false, Keyboard, 2.5,
		TEXT("AltCompleted"));
	ProcessAndExpectSingle(EKeys::LeftCommand, false, Keyboard, 2.6,
		TEXT("CommandCompleted"));

	// Reverse-order coverage: Control -> Shift and Mouse -> Shift.
	ProcessAndExpectSingle(EKeys::LeftControl, true, Keyboard, 3.0,
		TEXT("ControlStarted"));
	ProcessAndExpectSingle(EKeys::LeftShift, true, Keyboard, 3.1,
		TEXT("ShiftStarted"));
	ProcessAndExpectSingle(EKeys::LeftShift, false, Keyboard, 3.2,
		TEXT("ShiftCompleted"));
	ProcessAndExpectSingle(EKeys::LeftControl, false, Keyboard, 3.3,
		TEXT("ControlCompleted"));

	ProcessAndExpectSingle(EKeys::LeftMouseButton, true, 0, 4.0,
		TEXT("MouseStarted"));
	ProcessAndExpectSingle(EKeys::LeftShift, true, Keyboard, 4.1,
		TEXT("ShiftStarted"));
	Actions.GatherTriggered(State, 4.2, Invocations);
	TestEqual(TEXT("Mouse and Shift remain active in reverse order"),
		Invocations.Num(), 2);
	ProcessAndExpectSingle(EKeys::LeftShift, false, Keyboard, 4.3,
		TEXT("ShiftCompleted"));
	ProcessAndExpectSingle(EKeys::LeftMouseButton, false, 0, 4.4,
		TEXT("MouseCompleted"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGlobalInputActionExtraModifiersTest,
	"GlobalInputBridge.Input.Action.DefaultAllowsExtraModifiers",
	EAutomationTestFlags::EditorContext |
	EAutomationTestFlags::EngineFilter)

bool FGlobalInputActionExtraModifiersTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;

	FInputStateManager State;
	FGlobalChordBindingManager Actions;
	UObject* Target = NewObject<UGlobalInputSettings>();
	constexpr int64 Keyboard = 505;

	FGlobalChordBlueprintBinding Binding;
	Binding.BindingId = FGuid::NewGuid();
	Binding.Chord.Key = EKeys::R;
	Binding.Chord.RequiredModifiers.bControl = true;
	Binding.StartedFunctionName = TEXT("ActionStarted");
	Binding.TriggeredFunctionName = TEXT("ActionTriggered");
	Binding.CompletedFunctionName = TEXT("ActionCompleted");
	TestFalse(TEXT("Input actions allow extra modifiers by default"),
		Binding.Chord.bExactModifiers);
	Actions.RegisterBinding(Target, Binding);

	TArray<FGlobalChordInvocation> Invocations;
	FGlobalKeyEvent Event;
	State.BeginFrame();

	State.ProcessKeyEvent(EKeys::LeftShift, true, Keyboard, 1.0, Event);
	Actions.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Extra Shift alone does not start the action"),
		Invocations.Num(), 0);

	State.ProcessKeyEvent(EKeys::LeftControl, true, Keyboard, 1.1, Event);
	Actions.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Required Control alone does not start the action"),
		Invocations.Num(), 0);

	State.ProcessKeyEvent(EKeys::R, true, Keyboard, 1.2, Event);
	Actions.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Control+R starts while extra Shift is held"),
		Invocations.Num(), 1);
	TestEqual(TEXT("Started callback is selected with extra Shift"),
		Invocations[0].FunctionName, Binding.StartedFunctionName);

	State.ProcessKeyEvent(EKeys::LeftAlt, true, Keyboard, 1.3, Event);
	Actions.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("A later extra modifier does not complete the action"),
		Invocations.Num(), 0);

	Actions.GatherTriggered(State, 1.4, Invocations);
	TestEqual(TEXT("Action remains active with Shift and Alt held"),
		Invocations.Num(), 1);

	State.ProcessKeyEvent(EKeys::LeftControl, false, Keyboard, 1.5, Event);
	Actions.ProcessKeyEvent(Event, State, Invocations);
	TestEqual(TEXT("Releasing a required modifier completes the action"),
		Invocations.Num(), 1);
	TestEqual(TEXT("Completed callback is selected"),
		Invocations[0].FunctionName, Binding.CompletedFunctionName);

	return true;
}

#endif
