//GlobalInputTypes.h
#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "GlobalInputTypes.generated.h"

/**
 * 全局按键事件类型。
 * 鼠标按钮同样通过该类型报告。
 */
UENUM(BlueprintType)
enum class EGlobalInputEventType : uint8
{
	Pressed UMETA(DisplayName="Pressed"),
	Released UMETA(DisplayName="Released")
};

USTRUCT(BlueprintType)
struct GLOBALINPUTBRIDGE_API FGlobalModifierState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Global Input")
	bool bControl = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Global Input")
	bool bAlt = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Global Input")
	bool bShift = false;

	/** Windows 平台对应左/右 Win 键。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Global Input")
	bool bCommand = false;

	bool operator==(const FGlobalModifierState& Other) const//被用于在Subsystem中进行快捷键匹配
	{
		return bControl == Other.bControl
			&& bAlt == Other.bAlt
			&& bShift == Other.bShift
			&& bCommand == Other.bCommand;
	}

	bool operator!=(const FGlobalModifierState& Other) const
	{
		return !(*this == Other);
	}
};

/**
 * 全局键盘或鼠标按钮事件。
 *
 * 例如：
 * EKeys::W
 * EKeys::SpaceBar
 */
USTRUCT(BlueprintType)
struct GLOBALINPUTBRIDGE_API FGlobalKeyEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Global Input")
	FKey Key;

	UPROPERTY(BlueprintReadOnly, Category="Global Input")
	EGlobalInputEventType EventType = EGlobalInputEventType::Pressed;

	UPROPERTY(BlueprintReadOnly, Category="Global Input")
	FGlobalModifierState Modifiers;

	/**
	 * Windows/硬件再次发送 Down 时为 true。插件不合成统一 Repeat；
	 * 持续行为应查询 IsGlobalKeyDown()，不要依赖 Repeat 数量。
	 */
	UPROPERTY(BlueprintReadOnly, Category="Global Input")
	bool bRepeat = false;

	/**
	 * Raw keyboard device identifier. 0 is reserved for aggregate mouse input
	 * and lifecycle-synthesized Released events.
	 */
	UPROPERTY(BlueprintReadOnly, Category="Global Input")
	int64 DeviceId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Global Input")
	double TimestampSeconds = 0.0;
};

/**
 * 全局鼠标移动事件。
 */
USTRUCT(BlueprintType)
struct GLOBALINPUTBRIDGE_API FGlobalMouseMoveEvent
{
	GENERATED_BODY()

	/** Windows 虚拟桌面绝对坐标。 */
	UPROPERTY(BlueprintReadOnly, Category="Global Input")
	FVector2D ScreenPosition = FVector2D::ZeroVector;

	/**
	 * 相对上一帧的鼠标位移。
	 * Polling 模式为桌面像素差；RawInput 模式为 Raw Input 设备计数，
	 * 不受游戏锁定光标影响，也不含指针加速度。
	 */
	UPROPERTY(BlueprintReadOnly, Category="Global Input")
	FVector2D Delta = FVector2D::ZeroVector;

	/** GetCursorPos 是否成功。 */
	UPROPERTY(BlueprintReadOnly, Category="Global Input")
	bool bPositionValid = false;

	/** Mouse polling is aggregate input and therefore always uses DeviceId=0. */
	UPROPERTY(BlueprintReadOnly, Category="Global Input")
	int64 DeviceId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Global Input")
	double TimestampSeconds = 0.0;
};


/**
 * 全局输入动作规则。Key 可以是普通按键、鼠标按键或左右修饰键。
 */
USTRUCT(BlueprintType)
struct GLOBALINPUTBRIDGE_API FGlobalKeyChord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Global Input")
	FKey Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Global Input")
	FGlobalModifierState RequiredModifiers;

	/**
	 * true：显式互斥模式，Ctrl+Q 不匹配 Ctrl+Shift+Q。
	 * false：默认 Action Event 语义，只要求选中的修饰键存在。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Global Input",
		meta=(DisplayName="Exact Modifiers (Exclusive)"))
	bool bExactModifiers = false;

	/** Main modifier keys are matched by Key and are not counted twice. */
	static void RemoveMainKeyModifier(
		const FKey& MainKey,
		FGlobalModifierState& InOutModifiers)
	{
		if (MainKey == EKeys::LeftControl ||
			MainKey == EKeys::RightControl)
		{
			InOutModifiers.bControl = false;
		}
		else if (MainKey == EKeys::LeftAlt ||
			MainKey == EKeys::RightAlt)
		{
			InOutModifiers.bAlt = false;
		}
		else if (MainKey == EKeys::LeftShift ||
			MainKey == EKeys::RightShift)
		{
			InOutModifiers.bShift = false;
		}
		else if (MainKey == EKeys::LeftCommand ||
			MainKey == EKeys::RightCommand)
		{
			InOutModifiers.bCommand = false;
		}
	}

	FGlobalModifierState GetEffectiveRequiredModifiers() const
	{
		FGlobalModifierState Effective = RequiredModifiers;
		RemoveMainKeyModifier(Key, Effective);
		return Effective;
	}
};

/**
 * Runtime information emitted by a Global Input Action Event node.
 *
 * Started uses the main-key Pressed event snapshot. Triggered refreshes the
 * timestamp and modifiers every subsystem tick. Completed uses the event that
 * made the chord invalid, or DeviceId=0 for a lifecycle-synthesized release.
 */
USTRUCT(BlueprintType, meta=(DisplayName="Global Input Action Event Info"))
struct GLOBALINPUTBRIDGE_API FGlobalChordEventInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Global Input")
	FKey Key;

	UPROPERTY(BlueprintReadOnly, Category="Global Input")
	FGlobalModifierState Modifiers;

	UPROPERTY(BlueprintReadOnly, Category="Global Input")
	int64 DeviceId = 0;

	UPROPERTY(BlueprintReadOnly, Category="Global Input")
	double TimestampSeconds = 0.0;
};

/** 可直接用于蓝图调试面板或日志输出的当前输入快照。 */
USTRUCT(BlueprintType)
struct GLOBALINPUTBRIDGE_API FGlobalInputDebugInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category="Global Input|Debug")
	bool bListening = false;

	/** 当前监听会话中已枚举或观察到的键盘设备数。 */
	UPROPERTY(BlueprintReadOnly, Category="Global Input|Debug")
	int32 KeyboardCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Global Input|Debug")
	int32 PressedKeyCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Global Input|Debug")
	FVector2D MousePosition = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Global Input|Debug")
	FVector2D MouseDelta = FVector2D::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category="Global Input|Debug")
	bool bMousePositionValid = false;

	UPROPERTY(BlueprintReadOnly, Category="Global Input|Debug")
	bool bEventFilterEnabled = false;

	/** false=allow list, true=exclude list. */
	UPROPERTY(BlueprintReadOnly, Category="Global Input|Debug")
	bool bEventFilterExcludeMode = false;

	UPROPERTY(BlueprintReadOnly, Category="Global Input|Debug")
	int32 EventFilterKeyCount = 0;

	/** 以逗号分隔并稳定排序的当前按下键名。 */
	UPROPERTY(BlueprintReadOnly, Category="Global Input|Debug")
	FString ActiveKeys;
};
