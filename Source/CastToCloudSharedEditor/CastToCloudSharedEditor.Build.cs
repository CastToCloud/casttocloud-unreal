// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

using UnrealBuildTool;

public class CastToCloudSharedEditor : ModuleRules
{
	public CastToCloudSharedEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"DesktopWidgets",
				"DeveloperToolSettings",
				"EditorSubsystem",
				"Engine",
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

				"CastToCloudShared",
				"CastToCloudAnalytics",
			}
		);
	}
}
