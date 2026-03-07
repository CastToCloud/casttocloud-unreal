// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

using UnrealBuildTool;

public class CastToCloud : ModuleRules
{
	public CastToCloud(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",

				"CastToCloudAnalytics",
				"CastToCloudMetrics",
				"CastToCloudShared",
			}
		);
	}
}
