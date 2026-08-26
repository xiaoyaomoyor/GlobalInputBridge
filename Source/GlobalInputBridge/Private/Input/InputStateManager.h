//InputStateManager.h
#pragma once

#include "CoreMinimal.h"
#include "Input/GlobalInputTypes.h"

/**
 * 全局输入状态管理器。 只允许在 GameThread 上访问。
 * 负责：
 * - 保存每个输入设备当前按下的 FKey
 * - 维护所有设备的聚合按键状态
 * - 识别键盘自动重复
 * - 忽略无效的重复释放
 * - 生成 FGlobalKeyEvent
 * - 计算 Ctrl、Alt、Shift、Command 状态
 * - 保存最近一次有效的桌面鼠标位置
 */
class FInputStateManager final
{
public:
	FInputStateManager() = default;
	~FInputStateManager() = default;

	FInputStateManager(const FInputStateManager&) = delete;
	FInputStateManager& operator=(const FInputStateManager&) = delete;

	/**
	 * 处理一个已经转换为 UE FKey 的键盘或鼠标按钮事件。
	 *
	 * 返回 true：
	 * 产生了一个有效事件，Subsystem 应广播 OutEvent。
	 *
	 * 返回 false：
	 * Key 无效、收到了没有对应 Pressed 的重复 Released，或者物理设备
	 * 状态发生了变化但所有设备聚合后的状态没有发生边沿变化。
	 *
	 * 自动重复 Pressed 仍返回 true，并设置 OutEvent.bRepeat=true，
	 * 由蓝图自行决定是否处理重复事件。
	 */
	bool ProcessKeyEvent(const FKey& Key, bool bPressed, int64 DeviceId,
		double TimestampSeconds, FGlobalKeyEvent& OutEvent);

	/** 清除上一帧边沿状态和鼠标 Delta；每次 Subsystem Tick 开始时调用。 */
	void BeginFrame();

	/** 记录一个已枚举或产生过输入的键盘设备。 */
	void RegisterKeyboardDevice(int64 DeviceId);

	/**
	 * 移除一个物理设备的全部状态。
	 * 只有因此从 Down 变为 Up 的聚合按键才产生 Released 事件。
	 */
	void RemoveDevice(int64 DeviceId, double TimestampSeconds,
		TArray<FGlobalKeyEvent>& OutReleasedEvents);

	/**
	 * 清除全部设备状态，并为每个聚合 Down 按键产生一次 Released。
	 * DeviceId=0 表示该释放由监听生命周期合成，而非某个物理设备。
	 */
	void ReleaseAllKeys(double TimestampSeconds,
		TArray<FGlobalKeyEvent>& OutReleasedEvents);

	/** 查询所有设备聚合后的按键状态。 */
	bool IsKeyDown(const FKey& Key) const;
	bool WasKeyPressedThisFrame(const FKey& Key) const;
	bool WasKeyReleasedThisFrame(const FKey& Key) const;
	TArray<FKey> GetPressedKeys() const;
	int32 GetKeyboardDeviceCount() const;

	/** 更新最近一次桌面鼠标绝对坐标。 */
	void UpdateMousePosition(const FVector2D& Position, bool bPositionValid);
	void UpdateMouseDelta(const FVector2D& Delta);

	FVector2D GetMousePosition() const;
	FVector2D GetMouseDelta() const;
	bool HasValidMousePosition() const;

	/** 获取所有设备聚合后的修饰键状态。 */
	FGlobalModifierState GetModifierState() const;

	/** 清空所有按键、设备和鼠标位置状态。 */
	void Reset();

private:
	/**
	 * 每个设备当前实际按下的键。
	 *
	 * 用于：
	 * - 自动重复识别
	 * - 重复 Released 过滤
	 * - 多键盘状态管理
	 */
	TMap<int64, TSet<FKey>> DevicePressedKeys;

	/**
	 * 当前有多少个设备按住该键。
	 *
	 * 例如两个键盘同时按住 W：
	 * AggregateKeyCounts[W] == 2。
	 */
	TMap<FKey, int32> AggregateKeyCounts;
	TSet<FKey> PressedThisFrame;
	TSet<FKey> ReleasedThisFrame;
	TSet<int64> KnownKeyboardDevices;

	FVector2D MousePosition = FVector2D::ZeroVector;
	FVector2D MouseDelta = FVector2D::ZeroVector;
	bool bMousePositionValid = false;
};
