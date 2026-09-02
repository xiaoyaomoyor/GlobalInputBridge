//GlobalChordBindingManager.h
#pragma once

#include "CoreMinimal.h"
#include "Input/GlobalChordDelegateBinding.h"

//只用Dynamic Multicast Delegate，无法实现类似事件的蓝图节点
//但是现在使用Dynamic Blueprint Binding，蓝图调用更方便
//K2Node_GlobalInputActionEvent->生成 FGlobalChordBlueprintBinding->UDynamicBlueprintBinding
//->GlobalChordBindingManager->ProcessEvent()

class FInputStateManager;
class UWorld;

/** 编译期绑定一次回调所处的工作阶段。 */
enum class EGlobalChordInvocationPhase : uint8
{
	Started,
	Triggered,
	Completed
};

/** A deferred blueprint callback produced while chord state is evaluated. */
struct FGlobalChordInvocation
{
	TWeakObjectPtr<UObject> Target;
	FGuid BindingId;
	FName FunctionName;
	FGlobalChordEventInfo EventInfo;

	/** 该回调对应的生命周期阶段，用于事件过滤时区分 Triggered 与 Completed。 */
	EGlobalChordInvocationPhase Phase =
		EGlobalChordInvocationPhase::Started;
};

/** Game-thread-only runtime state for compiled Global Input Action Event nodes. */
class FGlobalChordBindingManager final
{
public:
	void RegisterBinding(
		UObject* Target,
		const FGlobalChordBlueprintBinding& Binding);

	void UnregisterBindings(UObject* Target);
	void UnregisterBindingsForWorld(const UWorld* World);

	void ProcessKeyEvent(
		const FGlobalKeyEvent& KeyEvent,
		const FInputStateManager& StateManager,
		TArray<FGlobalChordInvocation>& OutInvocations);

	void GatherTriggered(
		const FInputStateManager& StateManager,
		double TimestampSeconds,
		TArray<FGlobalChordInvocation>& OutInvocations);

	void Reset();
	int32 Num() const;
	int32 NumForTarget(const UObject* Target) const;

private:
	struct FRuntimeBinding
	{
		TWeakObjectPtr<UObject> Target;
		FGlobalChordBlueprintBinding Definition;
		bool bActive = false;
		int64 ActivationDeviceId = 0;
	};

	static bool IsChordSatisfied(
		const FGlobalKeyChord& Chord,
		const FInputStateManager& StateManager);

	static FGlobalChordEventInfo MakeEventInfo(
		const FRuntimeBinding& Binding,
		const FGlobalModifierState& Modifiers,
		int64 DeviceId,
		double TimestampSeconds);

	static void AddInvocation(
		const FRuntimeBinding& Binding,
		FName FunctionName,
		const FGlobalChordEventInfo& EventInfo,
		EGlobalChordInvocationPhase Phase,
		TArray<FGlobalChordInvocation>& OutInvocations);

	void RemoveInvalidTargets();

private:
	TArray<FRuntimeBinding> Bindings;
};
