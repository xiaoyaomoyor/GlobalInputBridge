//GlobalInputBridgeModule.cpp

#include "GlobalInputBridgeModule.h"
#include "Modules/ModuleManager.h"
#include "Windows/WindowsKeyMapper.h"

// 定义日志类别
DEFINE_LOG_CATEGORY(LogGlobalInput);

#define LOCTEXT_NAMESPACE "FGlobalInputBridgeModule"//用于国际化文本

void FGlobalInputBridgeModule::StartupModule()
{
	UE_LOG(LogGlobalInput, VeryVerbose, TEXT("GlobalInputBridge Module Started!"));
	FWindowsKeyMapper::Initialize();
}

void FGlobalInputBridgeModule::ShutdownModule()
{
	UE_LOG(LogGlobalInput, VeryVerbose, TEXT("GlobalInputBridge Module Shut Down!"));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FGlobalInputBridgeModule, GlobalInputBridge)
