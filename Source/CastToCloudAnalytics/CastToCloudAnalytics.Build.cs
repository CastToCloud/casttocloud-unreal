// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

using UnrealBuildTool;

public class CastToCloudAnalytics : ModuleRules
{
	public CastToCloudAnalytics(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"AnalyticsBlueprintLibrary",
				"ApplicationCore",
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"EngineSettings",
				"HTTP",
				"Json",
				"JsonUtilities",
				"Projects",
				"Slate",
				"SlateCore",
				"StudioTelemetry",

				"CastToCloud",
			}
		);

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.Add("UnrealEd");
		}
	}
}