//RawInputWorker.cpp

#include "Input/RawInputWorker.h"

#include "GlobalInputBridgeModule.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
#if PLATFORM_WINDOWS
	constexpr wchar_t RawInputWindowClassName[] =
		L"GlobalInputBridge_RawKeyboardMessageWindow";

	constexpr UINT MouseReArmTimerId = 1;
	constexpr UINT MouseReArmIntervalMs = 500;

	/** WM_INPUT_DEVICE_CHANGE 不区分设备类型，需主动查询后过滤。 */
	bool IsKeyboardDevice(HANDLE DeviceHandle)
	{
		if (!DeviceHandle) return false;

		RID_DEVICE_INFO DeviceInfo{};
		DeviceInfo.cbSize = sizeof(DeviceInfo);

		UINT Size = sizeof(DeviceInfo);

		if (GetRawInputDeviceInfoW(
			DeviceHandle,
			RIDI_DEVICEINFO,
			&DeviceInfo,
			&Size) == static_cast<UINT>(-1))
		{
			UE_LOG(LogGlobalInput, VeryVerbose,
				TEXT("GetRawInputDeviceInfoW failed. Error=%lu"),
				GetLastError());
			return false;
		}

		return DeviceInfo.dwType == RIM_TYPEKEYBOARD;
	}
#endif
}

FRawInputWorker::FRawInputWorker(
	TQueue<FRawInputMessage, EQueueMode::Spsc>& InOutputQueue,
	bool bInEnableMouseRawInput)
	: OutputQueue(InOutputQueue)
	, bEnableMouseRawInput(bInEnableMouseRawInput)
{
	InitializationEvent = FPlatformProcess::GetSynchEventFromPool(true);
}

FRawInputWorker::~FRawInputWorker()
{
	Shutdown();

	if (InitializationEvent)
	{
		FPlatformProcess::ReturnSynchEventToPool(InitializationEvent);
		InitializationEvent = nullptr;
	}
}

bool FRawInputWorker::Start()
{
#if !PLATFORM_WINDOWS
	UE_LOG(LogGlobalInput, Error,
		TEXT("Global keyboard Raw Input is only supported on Windows."));
	return false;
#else
	if (Thread)
	{
		UE_LOG(LogGlobalInput, Warning,
			TEXT("Raw Input worker has already been started."));
		return IsRunning();
	}

	if (!InitializationEvent)
	{
		UE_LOG(LogGlobalInput, Error,
			TEXT("Raw Input initialization event is invalid."));
		return false;
	}

	bStopRequested = false;
	bRunning = false;
	bInitializationSucceeded = false;

	WorkerThreadId = 0;
	MessageWindow = nullptr;
	bRegisteredWindowClass = false;
	bRawInputRegistered = false;
	bMouseRawInputRegistered = false;

	InitializationEvent->Reset();

	Thread = FRunnableThread::Create(
		this,
		TEXT("GlobalInputBridge_RawInputThread"),
		0,
		TPri_Normal);

	if (!Thread)
	{
		UE_LOG(LogGlobalInput, Error,
			TEXT("Failed to create Raw Input worker thread."));
		return false;
	}

	constexpr uint32 InitializationTimeoutMs = 5000;

	if (!InitializationEvent->Wait(InitializationTimeoutMs))
	{
		UE_LOG(LogGlobalInput, Error,
			TEXT("Timed out while initializing Raw Input worker."));
		Shutdown();
		return false;
	}

	if (!bInitializationSucceeded)
	{
		UE_LOG(LogGlobalInput, Error,
			TEXT("Raw Input worker initialization failed."));
		Shutdown();
		return false;
	}

	UE_LOG(LogGlobalInput, Log,
		TEXT("Raw Input worker started successfully."));
	return true;
#endif
}

void FRawInputWorker::Stop()
{
	bStopRequested = true;

#if PLATFORM_WINDOWS
	const uint32 TargetThreadId =
		Thread ? Thread->GetThreadID() : WorkerThreadId;

	if (TargetThreadId != 0 &&
		!PostThreadMessageW(TargetThreadId, WM_QUIT, 0, 0))
	{
		UE_LOG(LogGlobalInput, VeryVerbose,
			TEXT("PostThreadMessageW(WM_QUIT) failed. Error=%lu"),
			GetLastError());

		if (MessageWindow)
		{
			PostMessageW(MessageWindow, WM_CLOSE, 0, 0);
		}
	}
#endif
}

void FRawInputWorker::Shutdown()
{
	Stop();

	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}

	bRunning = false;
	bInitializationSucceeded = false;
}

bool FRawInputWorker::IsRunning() const
{
	return bRunning;
}

uint32 FRawInputWorker::Run()
{
#if !PLATFORM_WINDOWS
	return 1;
#else
	bool bInitializationSignaled = false;

	auto SignalInitialization =
		[this, &bInitializationSignaled](bool bSucceeded)
	{
		if (bInitializationSignaled) return;

		bInitializationSucceeded = bSucceeded;

		if (InitializationEvent)
		{
			InitializationEvent->Trigger();
		}

		bInitializationSignaled = true;
	};

	WorkerThreadId = GetCurrentThreadId();

	// 强制创建当前线程的 Windows 消息队列。
	MSG InitialMessage{};
	PeekMessageW(
		&InitialMessage,
		nullptr,
		WM_USER,
		WM_USER,
		PM_NOREMOVE);

	if (bStopRequested)
	{
		SignalInitialization(false);
		WorkerThreadId = 0;
		return 0;
	}

	const HINSTANCE InstanceHandle = GetModuleHandleW(nullptr);

	if (!InstanceHandle)
	{
		UE_LOG(LogGlobalInput, Error,
			TEXT("GetModuleHandleW failed. Error=%lu"),
			GetLastError());

		SignalInitialization(false);
		WorkerThreadId = 0;
		return 1;
	}

	WNDCLASSEXW WindowClass{};
	WindowClass.cbSize = sizeof(WNDCLASSEXW);
	WindowClass.lpfnWndProc = &FRawInputWorker::WindowProc;
	WindowClass.hInstance = InstanceHandle;
	WindowClass.lpszClassName = RawInputWindowClassName;

	const ATOM WindowClassAtom =
		RegisterClassExW(&WindowClass);

	if (WindowClassAtom != 0)
	{
		bRegisteredWindowClass = true;
	}
	else if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
	{
		UE_LOG(LogGlobalInput, Error,
			TEXT("RegisterClassExW failed. Error=%lu"),
			GetLastError());

		SignalInitialization(false);
		WorkerThreadId = 0;
		return 1;
	}

	MessageWindow = CreateWindowExW(
		0,
		RawInputWindowClassName,
		L"GlobalInputBridge",
		WS_POPUP,
		0,
		0,
		1,
		1,
		nullptr,
		nullptr,
		InstanceHandle,
		this);

	if (!MessageWindow)
	{
		UE_LOG(LogGlobalInput, Error,
			TEXT("CreateWindowExW failed. Error=%lu"),
			GetLastError());

		SignalInitialization(false);
		CleanupWindowsResources();
		WorkerThreadId = 0;
		return 1;
	}

	ShowWindow(MessageWindow, SW_HIDE);

	if (!RegisterRawInput())
	{
		SignalInitialization(false);
		CleanupWindowsResources();
		WorkerThreadId = 0;
		return 1;
	}

	EnumerateConnectedKeyboards();

	// UE 视口可能随时抢占鼠标注册（见类注释），启用心跳自愈。
	if (bEnableMouseRawInput)
	{
		SetTimer(
			MessageWindow,
			MouseReArmTimerId,
			MouseReArmIntervalMs,
			nullptr);
	}

	bRunning = true;
	SignalInitialization(true);

	UE_LOG(LogGlobalInput, Log,
		TEXT("Raw Input thread started (Mouse=%d). ThreadId=%u"),
		bEnableMouseRawInput ? 1 : 0,
		WorkerThreadId);

	MSG Message{};

	while (!bStopRequested)
	{
		const BOOL Result =
			GetMessageW(&Message, nullptr, 0, 0);

		if (Result > 0)
		{
			TranslateMessage(&Message);
			DispatchMessageW(&Message);
			continue;
		}

		if (Result == 0)
		{
			break;
		}

		UE_LOG(LogGlobalInput, Error,
			TEXT("GetMessageW failed. Error=%lu"),
			GetLastError());
		break;
	}

	CleanupWindowsResources();

	bRunning = false;
	WorkerThreadId = 0;

	UE_LOG(LogGlobalInput, Log,
		TEXT("Raw Input thread stopped."));

	return 0;
#endif
}

#if PLATFORM_WINDOWS

bool FRawInputWorker::RegisterRawInput()
{
	if (!MessageWindow)
	{
		UE_LOG(LogGlobalInput, Error,
			TEXT("Cannot register Raw Input without a window."));
		return false;
	}

	RAWINPUTDEVICE Devices[2];
	int32 DeviceCount = 0;

	// Generic Desktop Controls / Keyboard
	Devices[DeviceCount].usUsagePage = 0x01;
	Devices[DeviceCount].usUsage = 0x06;
	Devices[DeviceCount].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
	Devices[DeviceCount].hwndTarget = MessageWindow;
	++DeviceCount;

	if (bEnableMouseRawInput)
	{
		// Generic Desktop Controls / Mouse
		Devices[DeviceCount].usUsagePage = 0x01;
		Devices[DeviceCount].usUsage = 0x02;
		Devices[DeviceCount].dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
		Devices[DeviceCount].hwndTarget = MessageWindow;
		++DeviceCount;
	}

	if (!RegisterRawInputDevices(
		Devices,
		DeviceCount,
		sizeof(RAWINPUTDEVICE)))
	{
		UE_LOG(LogGlobalInput, Error,
			TEXT("Register Raw Input failed (Keyboard=%d Mouse=%d). Error=%lu"),
			1,
			bEnableMouseRawInput ? 1 : 0,
			GetLastError());
		return false;
	}

	bRawInputRegistered = true;
	bMouseRawInputRegistered = bEnableMouseRawInput;

	UE_LOG(LogGlobalInput, Log,
		TEXT("Raw Input registered (Keyboard=1 Mouse=%d). HWND=%p"),
		bEnableMouseRawInput ? 1 : 0,
		MessageWindow);

	return true;
}

void FRawInputWorker::EnumerateConnectedKeyboards()
{
	UINT DeviceCount = 0;
	if (GetRawInputDeviceList(
		nullptr,
		&DeviceCount,
		sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1))
	{
		UE_LOG(LogGlobalInput, Warning,
			TEXT("GetRawInputDeviceList count query failed. Error=%lu"),
			GetLastError());
		return;
	}

	if (DeviceCount == 0)
	{
		return;
	}

	TArray<RAWINPUTDEVICELIST> Devices;
	Devices.SetNumUninitialized(DeviceCount);

	const UINT ReturnedCount = GetRawInputDeviceList(
		Devices.GetData(),
		&DeviceCount,
		sizeof(RAWINPUTDEVICELIST));

	if (ReturnedCount == static_cast<UINT>(-1))
	{
		UE_LOG(LogGlobalInput, Warning,
			TEXT("GetRawInputDeviceList failed. Error=%lu"),
			GetLastError());
		return;
	}

	for (UINT Index = 0; Index < ReturnedCount; ++Index)
	{
		const RAWINPUTDEVICELIST& Device = Devices[Index];
		if (Device.dwType == RIM_TYPEKEYBOARD && Device.hDevice)
		{
			HandleDeviceAdded(Device.hDevice);
		}
	}
}

void FRawInputWorker::UnregisterRawInput()
{
	RAWINPUTDEVICE Devices[2];
	int32 DeviceCount = 0;

	const auto AppendRemoval =
		[&Devices, &DeviceCount](uint16 Usage)
	{
		Devices[DeviceCount].usUsagePage = 0x01;
		Devices[DeviceCount].usUsage = Usage;
		Devices[DeviceCount].dwFlags = RIDEV_REMOVE;
		Devices[DeviceCount].hwndTarget = nullptr;
		++DeviceCount;
	};

	if (bRawInputRegistered)
	{
		AppendRemoval(0x06);
	}

	if (bMouseRawInputRegistered)
	{
		AppendRemoval(0x02);
	}

	if (DeviceCount == 0) return;

	if (!RegisterRawInputDevices(
		Devices,
		DeviceCount,
		sizeof(RAWINPUTDEVICE)))
	{
		UE_LOG(LogGlobalInput, Warning,
			TEXT("Unregister Raw Input failed. Error=%lu"),
			GetLastError());
	}

	bRawInputRegistered = false;
	bMouseRawInputRegistered = false;
}

void FRawInputWorker::CleanupWindowsResources()
{
	UnregisterRawInput();

	if (MessageWindow)
	{
		KillTimer(MessageWindow, MouseReArmTimerId);
		DestroyWindow(MessageWindow);
		MessageWindow = nullptr;
	}

	bReceivedMouseInputSinceHeartbeat = false;

	if (bRegisteredWindowClass)
	{
		const HINSTANCE InstanceHandle =
			GetModuleHandleW(nullptr);

		if (InstanceHandle &&
			!UnregisterClassW(
				RawInputWindowClassName,
				InstanceHandle))
		{
			UE_LOG(LogGlobalInput, VeryVerbose,
				TEXT("UnregisterClassW failed. Error=%lu"),
				GetLastError());
		}

		bRegisteredWindowClass = false;
	}
}

LRESULT CALLBACK FRawInputWorker::WindowProc(
	HWND Window,
	UINT Message,
	WPARAM WParam,
	LPARAM LParam)
{
	FRawInputWorker* Worker = nullptr;

	if (Message == WM_NCCREATE)
	{
		const CREATESTRUCTW* CreateInfo =
			reinterpret_cast<const CREATESTRUCTW*>(LParam);

		if (!CreateInfo) return false;

		Worker =
			static_cast<FRawInputWorker*>(
				CreateInfo->lpCreateParams);

		if (!Worker) return false;

		SetWindowLongPtrW(
			Window,
			GWLP_USERDATA,
			reinterpret_cast<LONG_PTR>(Worker));

		Worker->MessageWindow = Window;
		return true;
	}

	Worker = reinterpret_cast<FRawInputWorker*>(
		GetWindowLongPtrW(Window, GWLP_USERDATA));

	switch (Message)
	{
	case WM_INPUT:
	{
		const UINT InputCode =
			GET_RAWINPUT_CODE_WPARAM(WParam);

		if (Worker)
		{
			Worker->HandleRawInput(
				reinterpret_cast<HRAWINPUT>(LParam));
		}

		// 前台 Raw Input 需要交给 DefWindowProc 完成系统清理。
		if (InputCode == RIM_INPUT)
		{
			return DefWindowProcW(
				Window,
				Message,
				WParam,
				LParam);
		}

		return 0;
	}

	case WM_INPUT_DEVICE_CHANGE:
	{
		if (Worker && WParam == GIDC_ARRIVAL)
		{
			Worker->HandleDeviceAdded(
				reinterpret_cast<HANDLE>(LParam));
		}
		else if (Worker && WParam == GIDC_REMOVAL)
		{
			Worker->HandleDeviceRemoved(
				reinterpret_cast<HANDLE>(LParam));
		}

		UE_LOG(LogGlobalInput, VeryVerbose,
			TEXT("Raw input device %s. Device=%p"),
			WParam == GIDC_ARRIVAL
				? TEXT("connected")
				: TEXT("removed"),
			reinterpret_cast<void*>(LParam));

		return 0;
	}

	case WM_TIMER:
	{
		if (Worker && WParam == MouseReArmTimerId)
		{
			Worker->HandleHeartbeat();
		}
		return 0;
	}

	case WM_CLOSE:
	{
		DestroyWindow(Window);
		return 0;
	}

	case WM_DESTROY:
	{
		if (Worker)
		{
			Worker->MessageWindow = nullptr;
		}

		SetWindowLongPtrW(Window, GWLP_USERDATA, 0);
		PostQuitMessage(0);
		return 0;
	}

	default:
		return DefWindowProcW(Window, Message, WParam, LParam);
	}
}

void FRawInputWorker::HandleRawInput(
	HRAWINPUT RawInputHandle)
{
	if (!RawInputHandle) return;

	RAWINPUT Input{};
	UINT InputSize = sizeof(RAWINPUT);

	const UINT BytesRead = GetRawInputData(
		RawInputHandle,
		RID_INPUT,
		&Input,
		&InputSize,
		sizeof(RAWINPUTHEADER));

	if (BytesRead == static_cast<UINT>(-1))
	{
		UE_LOG(LogGlobalInput, Warning,
			TEXT("GetRawInputData failed. Error=%lu"),
			GetLastError());
		return;
	}

	if (BytesRead < sizeof(RAWINPUTHEADER))
	{
		UE_LOG(LogGlobalInput, Warning,
			TEXT("Raw Input packet is smaller than RAWINPUTHEADER."));
		return;
	}

	// Worker 只注册键盘和鼠标，其他设备类型全部忽略。
	if (Input.header.dwType == RIM_TYPEKEYBOARD)
	{
		const UINT KeyboardPacketSize =
			sizeof(RAWINPUTHEADER) + sizeof(RAWKEYBOARD);

		if (BytesRead < KeyboardPacketSize)
		{
			UE_LOG(LogGlobalInput, Warning,
				TEXT("Raw keyboard packet is smaller than expected."));
			return;
		}

		HandleKeyboard(Input);
	}
	else if (Input.header.dwType == RIM_TYPEMOUSE &&
		bMouseRawInputRegistered)
	{
		const UINT MousePacketSize =
			sizeof(RAWINPUTHEADER) + sizeof(RAWMOUSE);

		if (BytesRead < MousePacketSize)
		{
			UE_LOG(LogGlobalInput, Warning,
				TEXT("Raw mouse packet is smaller than expected."));
			return;
		}

		HandleMouse(Input);
	}
}

void FRawInputWorker::HandleKeyboard(
	const RAWINPUT& Input)
{
	const RAWKEYBOARD& Keyboard =
		Input.data.keyboard;

	// Windows 保留或伪按键事件。
	if (Keyboard.VKey == 255)
	{
		return;
	}

	FRawKeyboardPacket Packet;

	Packet.VirtualKey = Keyboard.VKey;
	Packet.ScanCode = Keyboard.MakeCode;
	Packet.Flags = Keyboard.Flags;
	Packet.bPressed =
		(Keyboard.Flags & RI_KEY_BREAK) == 0;
	Packet.DeviceId =
		reinterpret_cast<UPTRINT>(
			Input.header.hDevice);
	Packet.TimestampSeconds =
		FPlatformTime::Seconds();

	OutputQueue.Enqueue(
		FRawInputMessage::MakeKeyboard(Packet));

	//调试用：打印原始按键输入信息
	UE_LOG(LogGlobalInput, VeryVerbose,
		TEXT("Raw keyboard: VK=%u Scan=%u Pressed=%d Device=%p"),
		Packet.VirtualKey,
		Packet.ScanCode,
		Packet.bPressed ? 1 : 0,
		Input.header.hDevice);
}

void FRawInputWorker::HandleMouse(
	const RAWINPUT& Input)
{
	const RAWMOUSE& Mouse = Input.data.mouse;

	// RDP / 绝对坐标设备不产生可用的相对位移。
	if ((Mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0)
	{
		UE_LOG(LogGlobalInput, VeryVerbose,
			TEXT("Raw mouse absolute move ignored. Device=%p"),
			Input.header.hDevice);
		return;
	}

	// 纯按键包等零位移事件不值得占用队列。
	if (Mouse.lLastX == 0 && Mouse.lLastY == 0)
	{
		return;
	}

	FRawMousePacket Packet;

	Packet.DeltaX = Mouse.lLastX;
	Packet.DeltaY = Mouse.lLastY;
	Packet.DeviceId =
		reinterpret_cast<UPTRINT>(
			Input.header.hDevice);
	Packet.TimestampSeconds =
		FPlatformTime::Seconds();

	OutputQueue.Enqueue(
		FRawInputMessage::MakeMouseDelta(Packet));

	// 心跳依据：本周期内确实持有鼠标 Raw Input 注册。
	bReceivedMouseInputSinceHeartbeat = true;

	//调试用：打印原始鼠标位移
	UE_LOG(LogGlobalInput, VeryVerbose,
		TEXT("Raw mouse: DX=%d DY=%d Device=%p"),
		Packet.DeltaX,
		Packet.DeltaY,
		Input.header.hDevice);
}

void FRawInputWorker::HandleDeviceRemoved(
	HANDLE DeviceHandle)
{
	if (!DeviceHandle || !IsKeyboardDevice(DeviceHandle))
	{
		return;
	}

	const UPTRINT DeviceId =
		reinterpret_cast<UPTRINT>(DeviceHandle);

	OutputQueue.Enqueue(
		FRawInputMessage::MakeDeviceRemoved(
			DeviceId,
			FPlatformTime::Seconds()));
}

void FRawInputWorker::HandleHeartbeat()
{
	if (!bEnableMouseRawInput)
	{
		return;
	}

	// 心跳周期内未收到任何鼠标位移，可能已被同进程内其他窗口
	// （UE 视口高精度鼠标模式）抢占注册，重新夺回。
	// 鼠标空闲时重复注册到同一窗口是无害的自我覆盖。
	if (!bReceivedMouseInputSinceHeartbeat)
	{
		ReArmMouseRegistration();
	}

	bReceivedMouseInputSinceHeartbeat = false;
}

void FRawInputWorker::ReArmMouseRegistration()
{
	if (!MessageWindow)
	{
		return;
	}

	RAWINPUTDEVICE Device{};

	// Generic Desktop Controls / Mouse
	Device.usUsagePage = 0x01;
	Device.usUsage = 0x02;
	Device.dwFlags = RIDEV_INPUTSINK | RIDEV_DEVNOTIFY;
	Device.hwndTarget = MessageWindow;

	if (!RegisterRawInputDevices(
		&Device,
		1,
		sizeof(RAWINPUTDEVICE)))
	{
		UE_LOG(LogGlobalInput, VeryVerbose,
			TEXT("Re-arm mouse Raw Input failed. Error=%lu"),
			GetLastError());
		return;
	}

	bMouseRawInputRegistered = true;

	UE_LOG(LogGlobalInput, VeryVerbose,
		TEXT("Re-armed mouse Raw Input. HWND=%p"),
		MessageWindow);
}

void FRawInputWorker::HandleDeviceAdded(
	HANDLE DeviceHandle)
{
	if (!DeviceHandle || !IsKeyboardDevice(DeviceHandle))
	{
		return;
	}

	const UPTRINT DeviceId =
		reinterpret_cast<UPTRINT>(DeviceHandle);

	OutputQueue.Enqueue(
		FRawInputMessage::MakeDeviceAdded(
			DeviceId,
			FPlatformTime::Seconds()));
}

#endif
