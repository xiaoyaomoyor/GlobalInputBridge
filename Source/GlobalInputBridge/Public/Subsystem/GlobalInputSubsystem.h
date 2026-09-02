//GlobalInputSubsystem.h

#pragma once

#include "CoreMinimal.h"
#include "Input/GlobalInputTypes.h"
#include "Subsystems/EngineSubsystem.h"
#include "Tickable.h"
#include "GlobalInputSubsystem.generated.h"

struct FGlobalInputSubsystemImpl;
struct FGlobalChordBlueprintBinding;
struct FActorsInitializedParams;
class ULevel;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnGlobalKeyEvent,
	const FGlobalKeyEvent&,
	KeyEvent);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnGlobalMouseMoveEvent,
	const FGlobalMouseMoveEvent&,
	MouseEvent);

/**
 * 引擎级全局输入子系统。
 *
 * 键盘使用 Windows Raw Input 工作线程。
 * 鼠标按钮和桌面位置由 FWindowsMousePoller 在游戏线程轮询；
 * RawInput 模式下移动增量来自同一 Worker 的鼠标 Raw Input
 * （WM_INPUT 相对位移），在锁鼠 FPS 中依然有效。
 *
 * 负责：
 * - 管理键盘/鼠标 Raw Input Worker 生命周期
 * - 消费输入队列
 * - 轮询桌面鼠标位置和按钮
 * - 维护当前输入状态
 * - 向蓝图广播全局输入事件
 */
UCLASS()
class GLOBALINPUTBRIDGE_API UGlobalInputSubsystem
	: public UEngineSubsystem
	, public FTickableGameObject
{
	GENERATED_BODY()

public:
	// UEngineSubsystem
	virtual void Initialize(
		FSubsystemCollectionBase& Collection) override;

	virtual void Deinitialize() override;

	// FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;

	virtual TStatId GetStatId() const override
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(
			UGlobalInputSubsystem,
			STATGROUP_Tickables);
	}

	/** 开始监听，并静默同步当前鼠标位置和按钮状态。 */
	UFUNCTION(BlueprintCallable, Category="Global Input")
	bool StartListening();

	/** 停止监听，为全部 Active Key 广播 Released，然后清除状态。 */
	UFUNCTION(BlueprintCallable, Category="Global Input")
	void StopListening();

	UFUNCTION(BlueprintPure, Category="Global Input")
	bool IsListening() const
	{
		return bListening;
	}

	/**
	 * 查询键盘按键或鼠标按钮当前是否按下。
	 * 默认始终反映物理按键；勾选 Respect Event Filter 后，
	 * 被事件过滤器拦截的键按未按下处理。
	 */
	UFUNCTION(BlueprintPure, Category="Global Input")
	bool IsGlobalKeyDown(
		FKey Key,
		bool bRespectEventFilter = false) const;

	/**
	 * 本次 GlobalInputSubsystem Tick 中，聚合按键是否刚从 Up 变为 Down。
	 * 默认始终反映物理按键；勾选 Respect Event Filter 后，
	 * 被事件过滤器拦截的键不会有帧边沿。
	 */
	UFUNCTION(BlueprintPure, Category="Global Input|State")
	bool WasGlobalKeyPressedThisFrame(
		FKey Key,
		bool bRespectEventFilter = false) const;

	/**
	 * 本次 GlobalInputSubsystem Tick 中，聚合按键是否刚从 Down 变为 Up。
	 * 释放边沿不受事件过滤影响——与 Global Input Action 的 Completed
	 * 语义对齐：收尾信号永远放行，避免消费方悬空。
	 */
	UFUNCTION(BlueprintPure, Category="Global Input|State")
	bool WasGlobalKeyReleasedThisFrame(FKey Key) const;

	/**
	 * 获取所有设备聚合后当前按下的键，结果按 FKey 名称稳定排序。
	 * 勾选 Respect Event Filter 后，被事件过滤器拦截的键不会出现在结果中。
	 */
	UFUNCTION(BlueprintPure, Category="Global Input|State")
	TArray<FKey> GetPressedGlobalKeys(
		bool bRespectEventFilter = false) const;

	/** 获取最近一次有效的 Windows 虚拟桌面光标坐标。 */
	UFUNCTION(BlueprintPure, Category="Global Input|Mouse")
	FVector2D GetGlobalMousePosition() const;

	/**
	 * 获取本次 Subsystem Tick 的全局鼠标位移。
	 * Polling 模式为桌面像素差；RawInput 模式为 Raw Input 设备计数，
	 * 不受游戏锁定光标影响。
	 */
	UFUNCTION(BlueprintPure, Category="Global Input|Mouse")
	FVector2D GetGlobalMouseDelta() const;

	UFUNCTION(BlueprintPure, Category="Global Input|Mouse")
	bool HasValidGlobalMousePosition() const;

	UFUNCTION(BlueprintPure, Category="Global Input")
	FGlobalModifierState GetGlobalModifierState() const;

	/** 判断输入事件是否匹配指定快捷键规则。 */
	UFUNCTION(BlueprintPure, Category="Global Input")
	bool MatchesGlobalChord(
		const FGlobalKeyEvent& KeyEvent,
		const FGlobalKeyChord& Chord) const;

	/** 返回监听、设备、按键、鼠标和事件过滤状态的调试快照。 */
	UFUNCTION(BlueprintPure, Category="Global Input|Debug")
	FGlobalInputDebugInfo GetGlobalInputDebugInfo() const;

	/**
	 * 启用按键事件过滤。默认是允许列表；Exclude Mode 下是排除列表。
	 * 允许列表的空数组不广播任何按键事件；排除列表的空数组不排除任何按键。
	 * 过滤影响 OnGlobalKeyEvent 与 Global Input Action Event 的
	 * Started/Triggered；已开始动作的 Completed 不受影响；
	 * 状态查询、帧边沿和采集仍保持完整，修饰键状态也不受影响。
	 */
	UFUNCTION(BlueprintCallable, Category="Global Input|Filter")
	void SetGlobalInputEventFilter(
		const TArray<FKey>& Keys,
		bool bExcludeMode = false);

	/** 关闭事件过滤，恢复广播全部有效按键事件。 */
	UFUNCTION(BlueprintCallable, Category="Global Input|Filter")
	void ClearGlobalInputEventFilter();

	UFUNCTION(BlueprintPure, Category="Global Input|Filter")
	bool IsGlobalInputEventFilterEnabled() const;

	/** false=allow list, true=exclude list. */
	UFUNCTION(BlueprintPure, Category="Global Input|Filter")
	bool IsGlobalInputEventFilterExcludeMode() const;

	UFUNCTION(BlueprintPure, Category="Global Input|Filter")
	TArray<FKey> GetGlobalInputEventFilter() const;

	/**
	 * 查询指定键的事件当前是否被过滤器拦截。
	 *
	 * 状态查询（Is Global Key Down 等）不受过滤影响、始终反映物理按键；
	 * 消费方若希望与事件过滤保持一致，请配合本函数使用：
	 * IsGlobalKeyDown(Key) && !IsGlobalKeyEventSuppressed(Key)。
	 */
	UFUNCTION(BlueprintPure, Category="Global Input|Filter")
	bool IsGlobalKeyEventSuppressed(FKey Key) const;

	/** C++ runtime hook used by compiled Global Input Action Event bindings. */
	void RegisterGlobalChordBinding(
		UObject* Target,
		const FGlobalChordBlueprintBinding& Binding);

	/** Removes every compiled chord binding owned by a blueprint instance. */
	void UnregisterGlobalChordBindings(UObject* Target);

	UPROPERTY(BlueprintAssignable, Category="Global Input")
	FOnGlobalKeyEvent OnGlobalKeyEvent;

	UPROPERTY(BlueprintAssignable, Category="Global Input|Mouse")
	FOnGlobalMouseMoveEvent OnGlobalMouseMove;

private:
	void ProcessRawInputQueue();
	void PollGlobalMouse(bool bPollPosition);
	void SynchronizeGlobalMouse(bool bPollPosition);
	void StopListeningInternal(bool bBroadcastReleasedEvents);
	void ClearPendingPackets();
	bool ShouldBroadcastKeyEvent(const FKey& Key) const;
	void BroadcastKeyEvent(const FGlobalKeyEvent& KeyEvent);
	void ProcessGlobalChordKeyEvent(const FGlobalKeyEvent& KeyEvent);
	void TriggerActiveGlobalChords();
	void HandleWorldInitializedActors(
		const FActorsInitializedParams& Parameters);
	void HandleLevelAddedToWorld(ULevel* Level, UWorld* World);
	void HandleWorldCleanup(
		UWorld* World,
		bool bSessionEnded,
		bool bCleanupResources);
	void RegisterCompiledGlobalChordBindings(UObject* Target);
	void RegisterGlobalChordBindingsForWorld(UWorld* World);
	void RefreshRuntimeSettings();

#if WITH_DEV_AUTOMATION_TESTS
	int32 GetRegisteredGlobalChordBindingCountForTarget(
		const UObject* Target) const;
	friend class FGlobalInputActionEditorNodeCompilationTest;
#endif

private:
	/**
	 * Worker、队列、StateManager 和 MousePoller
	 * 全部隐藏在 cpp 的 Private Implementation 中。
	 */
	FGlobalInputSubsystemImpl* Impl = nullptr;
	bool bListening = false;
	bool bLifecycleTransition = false;
};
