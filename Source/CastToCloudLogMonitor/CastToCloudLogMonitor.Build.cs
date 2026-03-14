using UnrealBuildTool;

public class CastToCloudLogMonitor : ModuleRules
{
	public CastToCloudLogMonitor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"EngineSettings",
				"HTTP",
				"Json",
				"Slate",
				"SlateCore", 
				
				"CastToCloud",
			}
		);
	}
}