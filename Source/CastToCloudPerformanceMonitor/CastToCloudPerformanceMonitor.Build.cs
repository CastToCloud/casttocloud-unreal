using UnrealBuildTool;

public class CastToCloudPerformanceMonitor : ModuleRules
{
	public CastToCloudPerformanceMonitor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"EngineSettings",
				"StudioTelemetry",
				"Projects",
				"HTTP",
				"Json",
				"JsonUtilities",
				"DeveloperSettings",
			}
		);
	}
}