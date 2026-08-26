//WindowsKeyMapper.cpp
#include "Windows/WindowsKeyMapper.h"

#include "Input/GlobalInputTypes.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"

TMap<uint16, FKey> FWindowsKeyMapper::KeyboardMap;
//std::once_flag FWindowsKeyMapper::InitFlag;
#endif

void FWindowsKeyMapper::Initialize()
{
#if PLATFORM_WINDOWS
	static bool bInitialized = false;
	if(bInitialized) return;
	bInitialized=true;

	KeyboardMap.Reserve(96);//预先分配内存，避免重新分配内存和 Rehash过多消耗性能

	const FKey LetterKeys[] =
	{
		EKeys::A, EKeys::B, EKeys::C, EKeys::D, EKeys::E, EKeys::F,
		EKeys::G, EKeys::H, EKeys::I, EKeys::J, EKeys::K, EKeys::L,
		EKeys::M, EKeys::N, EKeys::O, EKeys::P, EKeys::Q, EKeys::R,
		EKeys::S, EKeys::T, EKeys::U, EKeys::V, EKeys::W, EKeys::X,
		EKeys::Y, EKeys::Z
	};

	//批量初始化
	for (uint16 Index = 0; Index < UE_ARRAY_COUNT(LetterKeys); ++Index)
	{
		KeyboardMap.Add(static_cast<uint16>('A' + Index), LetterKeys[Index]);
	}

	const FKey NumberKeys[] =
	{
		EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
		EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine
	};

	for (uint16 Index = 0; Index < UE_ARRAY_COUNT(NumberKeys); ++Index)
	{
		KeyboardMap.Add(static_cast<uint16>('0' + Index), NumberKeys[Index]);
	}

	const FKey FunctionKeys[] =
	{
		EKeys::F1, EKeys::F2, EKeys::F3, EKeys::F4, EKeys::F5, EKeys::F6,
		EKeys::F7, EKeys::F8, EKeys::F9, EKeys::F10, EKeys::F11, EKeys::F12
	};

	for (uint16 Index = 0; Index < UE_ARRAY_COUNT(FunctionKeys); ++Index)
	{
		KeyboardMap.Add(static_cast<uint16>(VK_F1 + Index), FunctionKeys[Index]);
	}

	const FKey NumPadKeys[] =
	{
		EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo,
		EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive,
		EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight,
		EKeys::NumPadNine
	};

	for (uint16 Index = 0; Index < UE_ARRAY_COUNT(NumPadKeys); ++Index)
	{
		KeyboardMap.Add(static_cast<uint16>(VK_NUMPAD0 + Index), NumPadKeys[Index]);
	}

	KeyboardMap.Add(VK_BACK, EKeys::BackSpace);
	KeyboardMap.Add(VK_TAB, EKeys::Tab);
	KeyboardMap.Add(VK_RETURN, EKeys::Enter);
	KeyboardMap.Add(VK_PAUSE, EKeys::Pause);
	KeyboardMap.Add(VK_CAPITAL, EKeys::CapsLock);
	KeyboardMap.Add(VK_ESCAPE, EKeys::Escape);
	KeyboardMap.Add(VK_SPACE, EKeys::SpaceBar);

	KeyboardMap.Add(VK_PRIOR, EKeys::PageUp);
	KeyboardMap.Add(VK_NEXT, EKeys::PageDown);
	KeyboardMap.Add(VK_END, EKeys::End);
	KeyboardMap.Add(VK_HOME, EKeys::Home);
	KeyboardMap.Add(VK_LEFT, EKeys::Left);
	KeyboardMap.Add(VK_UP, EKeys::Up);
	KeyboardMap.Add(VK_RIGHT, EKeys::Right);
	KeyboardMap.Add(VK_DOWN, EKeys::Down);
	KeyboardMap.Add(VK_INSERT, EKeys::Insert);
	KeyboardMap.Add(VK_DELETE, EKeys::Delete);

	KeyboardMap.Add(VK_LSHIFT, EKeys::LeftShift);
	KeyboardMap.Add(VK_RSHIFT, EKeys::RightShift);
	KeyboardMap.Add(VK_LCONTROL, EKeys::LeftControl);
	KeyboardMap.Add(VK_RCONTROL, EKeys::RightControl);
	KeyboardMap.Add(VK_LMENU, EKeys::LeftAlt);
	KeyboardMap.Add(VK_RMENU, EKeys::RightAlt);
	KeyboardMap.Add(VK_LWIN, EKeys::LeftCommand);
	KeyboardMap.Add(VK_RWIN, EKeys::RightCommand);

	KeyboardMap.Add(VK_MULTIPLY, EKeys::Multiply);
	KeyboardMap.Add(VK_ADD, EKeys::Add);
	KeyboardMap.Add(VK_SUBTRACT, EKeys::Subtract);
	KeyboardMap.Add(VK_DECIMAL, EKeys::Decimal);
	KeyboardMap.Add(VK_DIVIDE, EKeys::Divide);
	KeyboardMap.Add(VK_NUMLOCK, EKeys::NumLock);
	KeyboardMap.Add(VK_SCROLL, EKeys::ScrollLock);

	KeyboardMap.Add(VK_OEM_1, EKeys::Semicolon);
	KeyboardMap.Add(VK_OEM_PLUS, EKeys::Equals);
	KeyboardMap.Add(VK_OEM_COMMA, EKeys::Comma);
	KeyboardMap.Add(VK_OEM_MINUS, EKeys::Hyphen);
	KeyboardMap.Add(VK_OEM_PERIOD, EKeys::Period);
	KeyboardMap.Add(VK_OEM_2, EKeys::Slash);
	KeyboardMap.Add(VK_OEM_3, EKeys::Tilde);
	KeyboardMap.Add(VK_OEM_4, EKeys::LeftBracket);
	KeyboardMap.Add(VK_OEM_5, EKeys::Backslash);
	KeyboardMap.Add(VK_OEM_6, EKeys::RightBracket);
	KeyboardMap.Add(VK_OEM_7, EKeys::Apostrophe);
	KeyboardMap.Add(VK_OEM_102, EKeys::Backslash);

#endif
}

FKey FWindowsKeyMapper::ConvertKeyboard(const FRawKeyboardPacket& Packet)
{
#if PLATFORM_WINDOWS
	//Initialize(); 放弃懒加载

	const uint16 VirtualKey = NormalizeVirtualKey(Packet);
	if (const FKey* Key = KeyboardMap.Find(VirtualKey))
	{
		return *Key;
	}
#endif

	return FKey();
}


#if PLATFORM_WINDOWS

uint16 FWindowsKeyMapper::NormalizeVirtualKey(const FRawKeyboardPacket& Packet)
{
	UINT VirtualKey = Packet.VirtualKey;

	if (VirtualKey == VK_SHIFT)
	{
		const UINT MappedVirtualKey =
			MapVirtualKeyW(Packet.ScanCode, MAPVK_VSC_TO_VK_EX);
		if (MappedVirtualKey != 0)
		{
			VirtualKey = MappedVirtualKey;
		}
	}
	else if (VirtualKey == VK_CONTROL)
	{
		VirtualKey = (Packet.Flags & RI_KEY_E0) ? VK_RCONTROL : VK_LCONTROL;
	}
	else if (VirtualKey == VK_MENU)//Alt
	{
		VirtualKey = (Packet.Flags & RI_KEY_E0) ? VK_RMENU : VK_LMENU;
	}
	else if ((Packet.Flags & RI_KEY_E0) == 0)
	{
		// Navigation virtual keys without E0 originate from the numeric keypad
		// when Num Lock is off. Preserve their physical keypad identity.
		switch (VirtualKey)
		{
		case VK_INSERT: VirtualKey = VK_NUMPAD0; break;
		case VK_END: VirtualKey = VK_NUMPAD1; break;
		case VK_DOWN: VirtualKey = VK_NUMPAD2; break;
		case VK_NEXT: VirtualKey = VK_NUMPAD3; break;
		case VK_LEFT: VirtualKey = VK_NUMPAD4; break;
		case VK_CLEAR: VirtualKey = VK_NUMPAD5; break;
		case VK_RIGHT: VirtualKey = VK_NUMPAD6; break;
		case VK_HOME: VirtualKey = VK_NUMPAD7; break;
		case VK_UP: VirtualKey = VK_NUMPAD8; break;
		case VK_PRIOR: VirtualKey = VK_NUMPAD9; break;
		case VK_DELETE: VirtualKey = VK_DECIMAL; break;
		default: break;
		}
	}

	return static_cast<uint16>(VirtualKey);
}

#endif
