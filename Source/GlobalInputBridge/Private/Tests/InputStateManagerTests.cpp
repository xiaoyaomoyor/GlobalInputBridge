#if WITH_DEV_AUTOMATION_TESTS

#include "Input/InputStateManager.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGlobalInputStateManagerAggregationTest,
	"GlobalInputBridge.Input.StateManager.AggregationAndRecovery",
	EAutomationTestFlags::EditorContext |
	EAutomationTestFlags::EngineFilter)

bool FGlobalInputStateManagerAggregationTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;

	FInputStateManager State;
	FGlobalKeyEvent Event;
	constexpr int64 KeyboardA = 101;
	constexpr int64 KeyboardB = 202;
	State.RegisterKeyboardDevice(KeyboardA);
	State.RegisterKeyboardDevice(KeyboardB);
	TestEqual(TEXT("Two keyboard devices are tracked"),
		State.GetKeyboardDeviceCount(), 2);

	State.BeginFrame();

	TestTrue(TEXT("First device creates aggregate Pressed"),
		State.ProcessKeyEvent(EKeys::W, true, KeyboardA, 1.0, Event));
	TestEqual(TEXT("First event is Pressed"), Event.EventType,
		EGlobalInputEventType::Pressed);
	TestFalse(TEXT("First press is not Repeat"), Event.bRepeat);
	TestTrue(TEXT("First edge is Pressed This Frame"),
		State.WasKeyPressedThisFrame(EKeys::W));
	TestFalse(TEXT("First edge is not Released This Frame"),
		State.WasKeyReleasedThisFrame(EKeys::W));

	State.BeginFrame();
	TestFalse(TEXT("BeginFrame clears Pressed This Frame"),
		State.WasKeyPressedThisFrame(EKeys::W));

	TestFalse(TEXT("Second device does not create another aggregate Pressed"),
		State.ProcessKeyEvent(EKeys::W, true, KeyboardB, 2.0, Event));
	TestTrue(TEXT("W remains Down with two devices"),
		State.IsKeyDown(EKeys::W));
	TestFalse(TEXT("Second physical device creates no aggregate frame edge"),
		State.WasKeyPressedThisFrame(EKeys::W));

	TestTrue(TEXT("Hardware repeat is preserved"),
		State.ProcessKeyEvent(EKeys::W, true, KeyboardB, 3.0, Event));
	TestTrue(TEXT("Repeated Down is marked Repeat"), Event.bRepeat);
	TestFalse(TEXT("Hardware Repeat is not Pressed This Frame"),
		State.WasKeyPressedThisFrame(EKeys::W));

	TestFalse(TEXT("First device release does not release aggregate key"),
		State.ProcessKeyEvent(EKeys::W, false, KeyboardA, 4.0, Event));
	TestTrue(TEXT("W remains Down on second device"),
		State.IsKeyDown(EKeys::W));

	TArray<FGlobalKeyEvent> RemovedEvents;
	State.ProcessKeyEvent(EKeys::LeftShift, true, KeyboardB, 5.0, Event);
	State.RemoveDevice(KeyboardB, 6.0, RemovedEvents);

	TestFalse(TEXT("Removed device clears W"), State.IsKeyDown(EKeys::W));
	TestFalse(TEXT("Removed device clears Shift"),
		State.IsKeyDown(EKeys::LeftShift));
	TestEqual(TEXT("Removal releases each aggregate key once"),
		RemovedEvents.Num(), 2);
	TestTrue(TEXT("Device removal records W Released This Frame"),
		State.WasKeyReleasedThisFrame(EKeys::W));

	for (const FGlobalKeyEvent& RemovedEvent : RemovedEvents)
	{
		TestEqual(TEXT("Removal event is Released"),
			RemovedEvent.EventType, EGlobalInputEventType::Released);
		TestFalse(TEXT("Removal release has cleared modifiers"),
			RemovedEvent.Modifiers.bShift);
		TestEqual(TEXT("Removal keeps source device id"),
			RemovedEvent.DeviceId, KeyboardB);
	}

	State.ProcessKeyEvent(EKeys::Q, true, KeyboardA, 6.1, Event);
	State.ProcessKeyEvent(EKeys::Q, true, KeyboardB, 6.2, Event);
	State.RemoveDevice(KeyboardA, 6.3, RemovedEvents);

	TestEqual(TEXT("Removing one of two devices creates no aggregate release"),
		RemovedEvents.Num(), 0);
	TestTrue(TEXT("Q remains Down on the second device"),
		State.IsKeyDown(EKeys::Q));

	State.RemoveDevice(KeyboardB, 6.4, RemovedEvents);
	TestEqual(TEXT("Removing the final device releases the aggregate key"),
		RemovedEvents.Num(), 1);
	TestEqual(TEXT("Final device removal releases Q"),
		RemovedEvents[0].Key, EKeys::Q);
	TestFalse(TEXT("Final device removal clears Q"),
		State.IsKeyDown(EKeys::Q));
	TestEqual(TEXT("Removed devices are no longer tracked"),
		State.GetKeyboardDeviceCount(), 0);

	State.ProcessKeyEvent(EKeys::A, true, KeyboardA, 7.0, Event);
	State.ProcessKeyEvent(EKeys::LeftControl, true, KeyboardA, 8.0, Event);
	const TArray<FKey> PressedKeys = State.GetPressedKeys();
	TestEqual(TEXT("Pressed key query returns aggregate keys"),
		PressedKeys.Num(), 2);

	State.UpdateMouseDelta(FVector2D(4.0, -2.0));
	TestEqual(TEXT("Mouse Delta is stored for the frame"),
		State.GetMouseDelta(), FVector2D(4.0, -2.0));
	State.BeginFrame();
	TestEqual(TEXT("BeginFrame clears mouse Delta"),
		State.GetMouseDelta(), FVector2D::ZeroVector);

	TArray<FGlobalKeyEvent> StopEvents;
	State.ReleaseAllKeys(9.0, StopEvents);

	TestEqual(TEXT("Stop releases each active aggregate key once"),
		StopEvents.Num(), 2);
	TestFalse(TEXT("Stop clears A"), State.IsKeyDown(EKeys::A));
	TestFalse(TEXT("Stop clears Control"),
		State.IsKeyDown(EKeys::LeftControl));

	for (const FGlobalKeyEvent& StopEvent : StopEvents)
	{
		TestEqual(TEXT("Stop release uses synthetic device id"),
			StopEvent.DeviceId, static_cast<int64>(0));
		TestFalse(TEXT("Stop release has cleared modifiers"),
			StopEvent.Modifiers.bControl);
	}

	return true;
}

#endif
