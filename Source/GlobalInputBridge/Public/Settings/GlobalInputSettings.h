#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GlobalInputSettings.generated.h"
//在项目设置中可以配置的插件设置
//目前可配置项：Auto Start，Log Level，Mouse Tracking Mode

/** 控制 GlobalInputBridge 自身日志的最高详细程度。 */
UENUM(BlueprintType)
enum class EGlobalInputLogLevel : uint8
{
	None UMETA(DisplayName="None"),
	Error UMETA(DisplayName="Error"),
	Warning UMETA(DisplayName="Warning"),
	Verbose UMETA(DisplayName="Verbose")
};

/** Beta 版本支持的鼠标采集模式。 */
UENUM(BlueprintType)
enum class EGlobalMouseTrackingMode : uint8
{
	Polling = 0 UMETA(
		DisplayName="Polling",
		ToolTip="Poll mouse buttons, desktop position, and delta."),
	Disabled = 1 UMETA(
		DisplayName="Disabled",
		ToolTip="Disable all mouse polling."),
	ButtonsOnly = 2 UMETA(
		DisplayName="Buttons Only",
		ToolTip="Poll mouse buttons without querying position or delta."),
	RawInput = 3 UMETA(
		DisplayName="Raw Input",
		ToolTip="Track movement via WM_INPUT raw relative deltas; works in cursor-locked FPS games. Buttons and desktop position are still polled.")
};

/**
 * GlobalInputBridge Runtime 设置。
 * 可在 Project Settings > Plugins > Global Input Bridge 中配置。
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Global Input Bridge"))
class GLOBALINPUTBRIDGE_API UGlobalInputSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	/** 在普通 Editor/Game 启动时自动调用 Engine Subsystem 的 StartListening。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Runtime",
		meta=(ConfigRestartRequired=true))
	bool bAutoStart = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Diagnostics")
	EGlobalInputLogLevel LogLevel = EGlobalInputLogLevel::Warning;

	/** 控制鼠标按钮、桌面位置与 Delta 的采集方式；不影响全局键盘监听。 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category="Mouse",
		meta=(ConfigRestartRequired=true))
	EGlobalMouseTrackingMode MouseTrackingMode =
		EGlobalMouseTrackingMode::RawInput;

	virtual FName GetCategoryName() const override
	{
		return TEXT("Plugins");
	}

#if WITH_EDITOR
	virtual void PostEditChangeProperty(
		FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
