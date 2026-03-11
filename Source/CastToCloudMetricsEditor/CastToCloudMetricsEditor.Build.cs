// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

using UnrealBuildTool;

public class CastToCloudMetricsEditor : ModuleRules
{
	public CastToCloudMetricsEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"EditorSubsystem",
				"HTTP",
				"HTTPServer",
				"Json",
				"JsonUtilities",
				"TargetPlatform",
				"TraceAnalysis",
				"TraceServices",
				"UnrealEd",

				"CastToCloudShared",
				"CastToCloudSharedEditor",
			}
		);
	}
}
