#if WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS

#include "Input/RawInputTypes.h"
#include "Misc/AutomationTest.h"
#include "Windows/WindowsHWrapper.h"
#include "Windows/WindowsKeyMapper.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGlobalInputWindowsKeyMapperTest,
	"GlobalInputBridge.Input.WindowsKeyMapper.Normalization",
	EAutomationTestFlags::EditorContext |
	EAutomationTestFlags::EngineFilter)

bool FGlobalInputWindowsKeyMapperTest::RunTest(
	const FString& Parameters)
{
	(void)Parameters;

	FWindowsKeyMapper::Initialize();

	FRawKeyboardPacket Packet;
	Packet.VirtualKey = VK_SHIFT;
	Packet.ScanCode = 0x2A;
	TestEqual(TEXT("Left Shift scan code"),
		FWindowsKeyMapper::ConvertKeyboard(Packet), EKeys::LeftShift);

	Packet.ScanCode = 0x36;
	TestEqual(TEXT("Right Shift scan code"),
		FWindowsKeyMapper::ConvertKeyboard(Packet), EKeys::RightShift);

	Packet.VirtualKey = VK_CONTROL;
	Packet.ScanCode = 0x1D;
	Packet.Flags = RI_KEY_E0;
	TestEqual(TEXT("Extended Control is Right Control"),
		FWindowsKeyMapper::ConvertKeyboard(Packet), EKeys::RightControl);

	Packet.VirtualKey = VK_INSERT;
	Packet.ScanCode = 0x52;
	Packet.Flags = 0;
	TestEqual(TEXT("Non-extended Insert is NumPad Zero"),
		FWindowsKeyMapper::ConvertKeyboard(Packet), EKeys::NumPadZero);

	Packet.Flags = RI_KEY_E0;
	TestEqual(TEXT("Extended Insert remains navigation Insert"),
		FWindowsKeyMapper::ConvertKeyboard(Packet), EKeys::Insert);

	return true;
}

#endif
