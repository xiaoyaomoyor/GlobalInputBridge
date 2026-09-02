#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/Engine.h"
#include "Misc/AutomationTest.h"
#include "Settings/GlobalInputSettings.h"
#include "Subsystem/GlobalInputSubsystem.h"
#include "Windows/WindowsMousePoller.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGlobalInputSubsystemLifecycleTest,
	"GlobalInputBridge.Input.Subsystem.StartStopLifecycle",
	EAutomationTestFlags::EditorContext |
	EAutomationTestFlags::EngineFilter)

bool FGlobalInputSubsystemLifecycleTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;

	if (!TestNotNull(TEXT("Engine is available"), GEngine))
	{
		return false;
	}

	UGlobalInputSubsystem* Subsystem =
		GEngine->GetEngineSubsystem<UGlobalInputSubsystem>();

	if (!TestNotNull(TEXT("Global input subsystem is available"), Subsystem))
	{
		return false;
	}

	FGlobalKeyEvent ModifierMainKeyEvent;
	ModifierMainKeyEvent.Key = EKeys::LeftControl;
	ModifierMainKeyEvent.EventType = EGlobalInputEventType::Pressed;
	ModifierMainKeyEvent.Modifiers.bControl = true;

	FGlobalKeyChord ModifierMainKeyChord;
	ModifierMainKeyChord.Key = EKeys::LeftControl;
	ModifierMainKeyChord.bExactModifiers = true;
	TestTrue(
		TEXT("Exact chord helper accepts Control as a standalone main key"),
		Subsystem->MatchesGlobalChord(
			ModifierMainKeyEvent,
			ModifierMainKeyChord));

	ModifierMainKeyChord.RequiredModifiers.bControl = true;
	TestTrue(
		TEXT("Chord helper ignores redundant main-key Control modifier"),
		Subsystem->MatchesGlobalChord(
			ModifierMainKeyEvent,
			ModifierMainKeyChord));

	ModifierMainKeyChord.RequiredModifiers.bShift = true;
	TestFalse(
		TEXT("Chord helper still requires other selected modifiers"),
		Subsystem->MatchesGlobalChord(
			ModifierMainKeyEvent,
			ModifierMainKeyChord));

	TestTrue(TEXT("Raw Input worker starts"),
		Subsystem->StartListening());
	TestTrue(TEXT("Subsystem reports listening"),
		Subsystem->IsListening());

	Subsystem->Tick(0.0f);
	const FGlobalInputDebugInfo DebugInfo =
		Subsystem->GetGlobalInputDebugInfo();
	TestTrue(TEXT("Debug snapshot reports listening"),
		DebugInfo.bListening);

	Subsystem->SetGlobalInputEventFilter(
		{EKeys::W, EKeys::W, FKey()});
	TestTrue(TEXT("Event filter is enabled"),
		Subsystem->IsGlobalInputEventFilterEnabled());
	TestFalse(TEXT("Legacy filter calls remain allow-list mode"),
		Subsystem->IsGlobalInputEventFilterExcludeMode());
	TestEqual(TEXT("Event filter removes duplicates and invalid keys"),
		Subsystem->GetGlobalInputEventFilter().Num(), 1);
	TestFalse(TEXT("Allow-listed key is not suppressed"),
		Subsystem->IsGlobalKeyEventSuppressed(EKeys::W));
	TestTrue(TEXT("Key outside allow list is suppressed"),
		Subsystem->IsGlobalKeyEventSuppressed(EKeys::E));
	const FGlobalInputDebugInfo FilteredDebugInfo =
		Subsystem->GetGlobalInputDebugInfo();
	TestTrue(TEXT("Debug snapshot reports event filter"),
		FilteredDebugInfo.bEventFilterEnabled);
	TestEqual(TEXT("Debug snapshot reports filter key count"),
		FilteredDebugInfo.EventFilterKeyCount, 1);
	TestFalse(TEXT("Debug snapshot reports allow-list mode"),
		FilteredDebugInfo.bEventFilterExcludeMode);

	Subsystem->SetGlobalInputEventFilter({EKeys::Escape}, true);
	TestTrue(TEXT("Event filter supports exclude-list mode"),
		Subsystem->IsGlobalInputEventFilterExcludeMode());
	TestTrue(TEXT("Excluded key reports suppressed"),
		Subsystem->IsGlobalKeyEventSuppressed(EKeys::Escape));
	TestFalse(TEXT("Non-excluded key reports not suppressed"),
		Subsystem->IsGlobalKeyEventSuppressed(EKeys::W));
	const FGlobalInputDebugInfo ExcludedDebugInfo =
		Subsystem->GetGlobalInputDebugInfo();
	TestTrue(TEXT("Debug snapshot reports exclude-list mode"),
		ExcludedDebugInfo.bEventFilterExcludeMode);

	Subsystem->SetGlobalInputEventFilter({});
	TestTrue(TEXT("Empty allow list remains an enabled filter"),
		Subsystem->IsGlobalInputEventFilterEnabled());
	TestEqual(TEXT("Empty allow list contains no keys"),
		Subsystem->GetGlobalInputEventFilter().Num(), 0);

	Subsystem->ClearGlobalInputEventFilter();
	TestFalse(TEXT("Event filter can be disabled"),
		Subsystem->IsGlobalInputEventFilterEnabled());
	TestFalse(TEXT("Clearing filter resets exclude-list mode"),
		Subsystem->IsGlobalInputEventFilterExcludeMode());
	TestFalse(TEXT("No key is suppressed after clearing filter"),
		Subsystem->IsGlobalKeyEventSuppressed(EKeys::Escape));

	Subsystem->StopListening();

	TestFalse(TEXT("Subsystem reports stopped"),
		Subsystem->IsListening());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGlobalInputMouseButtonsOnlyModeTest,
	"GlobalInputBridge.Input.Mouse.ButtonsOnlyMode",
	EAutomationTestFlags::EditorContext |
	EAutomationTestFlags::EngineFilter)

bool FGlobalInputMouseButtonsOnlyModeTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;

	TestEqual(
		TEXT("Polling keeps its existing config value"),
		static_cast<uint8>(EGlobalMouseTrackingMode::Polling),
		static_cast<uint8>(0));
	TestEqual(
		TEXT("Disabled keeps its existing config value"),
		static_cast<uint8>(EGlobalMouseTrackingMode::Disabled),
		static_cast<uint8>(1));
	TestEqual(
		TEXT("Buttons Only uses a new config value"),
		static_cast<uint8>(EGlobalMouseTrackingMode::ButtonsOnly),
		static_cast<uint8>(2));

	FWindowsMousePoller Poller;
	FWindowsMousePollResult Result;
	Poller.Poll(Result, false);

	TestFalse(
		TEXT("Buttons Only does not return a mouse position"),
		Result.bPositionValid);
	TestFalse(
		TEXT("Buttons Only does not report position changes"),
		Result.bPositionChanged);
	TestTrue(
		TEXT("Buttons Only leaves mouse position at zero"),
		Result.ScreenPosition.IsNearlyZero());
	TestTrue(
		TEXT("Buttons Only leaves mouse delta at zero"),
		Result.Delta.IsNearlyZero());

	return true;
}

#endif
