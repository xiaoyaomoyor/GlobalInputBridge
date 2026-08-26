//RawInputWorker.h
#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeBool.h"
#include "Input/RawInputTypes.h"

class FEvent;

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

/**
 * Windows 全局键盘/鼠标 Raw Input 工作线程。
 *
 * 运行独立的 Win32 消息循环，通过 RIDEV_INPUTSINK 接收键盘输入，
 * 并将键盘包与设备事件写入 Subsystem 持有的统一 SPSC 队列。
 *
 * bEnableMouseRawInput=true 时同时注册鼠标 Raw Input，
 * 相对位移（RAWMOUSE.lLastX/lLastY）以 MouseDelta 消息入队。
 * 该增量来自设备计数，不受游戏锁定系统光标的影响，
 * 适用于猎杀对决等把光标钉在屏幕中心的锁鼠 FPS。
 *
 * 同一进程内，同一 (UsagePage, Usage) 的 Raw Input 注册以后注册者为准
 * （微软文档：一个进程内每个设备类只能有一个接收窗口）。
 * UE 的 FWindowsApplication 会在视口捕获鼠标时（编辑器视口点击、PIE、
 * 游戏窗口鼠标捕获）反复注册/注销鼠标 0x01/0x02，从而抢占本 Worker 的
 * 注册。因此启用鼠标时 Worker 会启动心跳定时器：一个周期内没有收到
 * 任何鼠标位移即视为被抢占，自动重新注册夺回。
 * 跨进程互不影响，前台游戏不会触发此抢占。
 *
 * 鼠标按钮和桌面光标位置仍由游戏线程的
 * FWindowsMousePoller 进行无侵入轮询。
 *
 * 此类的工作线程禁止访问 UObject、Actor、UWorld 和蓝图对象。
 */
class FRawInputWorker final : public FRunnable
{
public:
	explicit FRawInputWorker(
		TQueue<FRawInputMessage, EQueueMode::Spsc>& InOutputQueue,
		bool bInEnableMouseRawInput = false);
	virtual ~FRawInputWorker() override;

	FRawInputWorker(const FRawInputWorker&) = delete;
	FRawInputWorker& operator=(const FRawInputWorker&) = delete;

	bool Start();
	virtual void Stop() override;
	virtual uint32 Run() override;

	void Shutdown();
	bool IsRunning() const;

private:
#if PLATFORM_WINDOWS
	static LRESULT CALLBACK WindowProc(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam);

	bool RegisterRawInput();
	void EnumerateConnectedKeyboards();
	void UnregisterRawInput();
	void CleanupWindowsResources();

	void HandleRawInput(HRAWINPUT RawInputHandle);
	void HandleKeyboard(const RAWINPUT& Input);
	void HandleMouse(const RAWINPUT& Input);
	void HandleDeviceAdded(HANDLE DeviceHandle);
	void HandleDeviceRemoved(HANDLE DeviceHandle);
	void HandleHeartbeat();
	void ReArmMouseRegistration();
#endif

private:
	/**
	 * 队列由 UGlobalInputSubsystem 持有。
	 * Worker 是单一生产者，Subsystem Tick 是单一消费者。
	 */
	TQueue<FRawInputMessage, EQueueMode::Spsc>& OutputQueue;

	/** 是否注册鼠标 Raw Input 并转发相对位移。 */
	bool bEnableMouseRawInput = false;

	FRunnableThread* Thread = nullptr;
	FEvent* InitializationEvent = nullptr;

	FThreadSafeBool bStopRequested = false;
	FThreadSafeBool bRunning = false;
	FThreadSafeBool bInitializationSucceeded = false;

#if PLATFORM_WINDOWS
	HWND MessageWindow = nullptr;
	uint32 WorkerThreadId = 0;

	bool bRegisteredWindowClass = false;
	bool bRawInputRegistered = false;
	bool bMouseRawInputRegistered = false;

	/** 心跳周期内是否收到过鼠标位移；仅 Worker 线程访问。 */
	bool bReceivedMouseInputSinceHeartbeat = false;
#endif
};
