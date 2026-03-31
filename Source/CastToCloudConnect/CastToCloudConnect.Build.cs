// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

using UnrealBuildTool;

public class CastToCloudConnect : ModuleRules
{
	public CastToCloudConnect(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Messaging",
				"MessagingCommon",
				"Projects",
				"SessionMessages",
				"UdpMessaging",

				"CastToCloudShared",
			}
		);
	}
}
