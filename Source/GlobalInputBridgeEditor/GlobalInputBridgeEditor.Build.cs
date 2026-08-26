using UnrealBuildTool;

public class GlobalInputBridgeEditor : ModuleRules
{
	public GlobalInputBridgeEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore",
				"BlueprintGraph",
				"GlobalInputBridge"
			});

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"KismetCompiler",
				"UnrealEd"
			});
	}
}
