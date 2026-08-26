//WindowsKeyMapper.h
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Input/RawInputTypes.h"

//Windows Raw Input 到 Unreal FKey 的转换层。不保存输入状态，不广播事件


class FWindowsKeyMapper final
{
public:
	static void Initialize();
	static FKey ConvertKeyboard(const FRawKeyboardPacket& Packet);
private:
#if PLATFORM_WINDOWS
	static uint16 NormalizeVirtualKey(const FRawKeyboardPacket& Packet);
	static TMap<uint16,FKey> KeyboardMap;//替代大量的Switch代码
#endif
};
