// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

using UnrealBuildTool;

public class CastToCloudEditor : ModuleRules
{
	public CastToCloudEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"DeveloperToolSettings",
				"EditorSubsystem",
				"Engine",
				"FunctionalTesting",
				"PropertyEditor",
				"LevelEditor",
				"UnrealEd",
				"ToolMenus",
				"Projects",
				"HTTP",
				"HTTPServer",
				"Json",
				"JsonUtilities",
				"Slate",
				"SlateCore",

				"CastToCloud",
				"CastToCloudAnalytics",
			}
		);
	}
}