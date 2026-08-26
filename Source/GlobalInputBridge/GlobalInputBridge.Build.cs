using UnrealBuildTool;

public class GlobalInputBridge : ModuleRules
{
	public GlobalInputBridge(ReadOnlyTargetRules Target) : base(Target)
	{
		//UE5默认推荐：使用共享PCH模式
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		//公共依赖：这些模块会暴露给Public头文件
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",//#include "CoreMinimal.h"
				"CoreUObject",//反射系统
				"DeveloperSettings",//项目设置中的 Global Input Bridge 配置
				"Engine",//UEngineSubsystem
				"InputCore",//UE输入系统
			});

		//私有依赖：只有cpp使用
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ApplicationCore"//Windows窗口和平台交互
			});

		//Windows平台专用
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDefinitions.Add("GLOBALINPUTBRIDGE_PLATFORM_WINDOWS=1");
			PublicSystemLibraries.Add("User32.lib");//Win32 输入核心
		}
		else
		{
			PublicDefinitions.Add("GLOBALINPUTBRIDGE_PLATFORM_WINDOWS=0");
		}
	}
}
