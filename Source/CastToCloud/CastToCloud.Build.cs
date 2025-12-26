// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

using UnrealBuildTool;

public class CastToCloud : ModuleRules
{
	public CastToCloud(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"Settings",
				"Slate",
				"SlateCore",
			}
		);

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.Add("SourceControl");
		}
	}
}
