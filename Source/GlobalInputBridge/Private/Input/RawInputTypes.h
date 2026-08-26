//RawInputTypes.h

#pragma once
#include "CoreMinimal.h"

//描述 Windows Raw Input 层的数据包
//只存在于 Worker线程和转换层。 不允许暴露给Blueprint。

/** Worker 发送给游戏线程的消息类型。 */
enum class ERawInputMessageType : uint8
{
	Keyboard,
	MouseDelta,
	DeviceAdded,
	DeviceRemoved
};

/** Windows Raw Keyboard 原始数据。 */
struct FRawKeyboardPacket
{
	uint16 VirtualKey = 0;
	uint16 ScanCode = 0;
	uint16 Flags = 0;

	bool bPressed = false;

	UPTRINT DeviceId = 0;
	double TimestampSeconds = 0.0;
};

/**
 * Windows Raw Mouse 相对位移原始数据。
 *
 * DeltaX/DeltaY 是 RAWMOUSE.lLastX/lLastY 的设备计数，
 * 不含指针加速度与 DPI 缩放；锁鼠游戏中依然有效。
 */
struct FRawMousePacket
{
	int32 DeltaX = 0;
	int32 DeltaY = 0;

	UPTRINT DeviceId = 0;
	double TimestampSeconds = 0.0;
};

/**
 * Worker → Subsystem 的统一消息。
 *
 * 键盘事件、鼠标位移与设备移除事件使用同一个 SPSC 队列，
 * 从而保持它们在 Windows 消息线程中的原始顺序。
 */
struct FRawInputMessage
{
	ERawInputMessageType Type =
		ERawInputMessageType::Keyboard;

	FRawKeyboardPacket Keyboard;
	FRawMousePacket Mouse;

	UPTRINT DeviceId = 0;
	double TimestampSeconds = 0.0;

	static FRawInputMessage MakeKeyboard(
		const FRawKeyboardPacket& Packet)
	{
		FRawInputMessage Message;
		Message.Type = ERawInputMessageType::Keyboard;
		Message.Keyboard = Packet;
		Message.DeviceId = Packet.DeviceId;
		Message.TimestampSeconds = Packet.TimestampSeconds;
		return Message;
	}

	static FRawInputMessage MakeMouseDelta(
		const FRawMousePacket& Packet)
	{
		FRawInputMessage Message;
		Message.Type = ERawInputMessageType::MouseDelta;
		Message.Mouse = Packet;
		Message.DeviceId = Packet.DeviceId;
		Message.TimestampSeconds = Packet.TimestampSeconds;
		return Message;
	}

	static FRawInputMessage MakeDeviceRemoved(
		UPTRINT InDeviceId,
		double InTimestampSeconds)
	{
		FRawInputMessage Message;
		Message.Type =
			ERawInputMessageType::DeviceRemoved;
		Message.DeviceId = InDeviceId;
		Message.TimestampSeconds =
			InTimestampSeconds;
		return Message;
	}

	static FRawInputMessage MakeDeviceAdded(
		UPTRINT InDeviceId,
		double InTimestampSeconds)
	{
		FRawInputMessage Message;
		Message.Type =
			ERawInputMessageType::DeviceAdded;
		Message.DeviceId = InDeviceId;
		Message.TimestampSeconds =
			InTimestampSeconds;
		return Message;
	}
};

