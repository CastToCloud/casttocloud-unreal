// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

using UnrealBuildTool;

public class CastToCloudConnectEditor : ModuleRules
{
	public CastToCloudConnectEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"HTTP",
				"Json",
				"Messaging",
				"Slate",
				"SlateCore",

				"CastToCloudShared",
				"CastToCloudConnect",
			}
		);
	}
}
