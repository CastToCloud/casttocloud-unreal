// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

using System.IO;
using EpicGames.Core;
using UnrealBuildTool;

public class CastToCloudShared : ModuleRules
{
	public CastToCloudShared(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"DeveloperSettings",
				"Engine",
				"Projects",
				"Slate",
				"SlateCore",
				"Sockets",
			}
		);

		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Settings",
					"SourceControl",
					"UnrealEd",
				}
			);
		}

		RuntimeDependencies.Add("$(ProjectDir)/Intermediate/CastToCloud/PreInitCastToCloud.ini", StagedFileType.NonUFS);

		StageCli(Target);
	}

	private void StageCli(ReadOnlyTargetRules Target)
	{
		string ProjectDir = Target.ProjectFile != null ? DirectoryReference.FromFile(Target.ProjectFile).FullName : string.Empty;

		if (!ShouldStageCli(ProjectDir, Target.Type))
		{
			System.Console.WriteLine("[CastToCloud] CLI will not be staged for {0}.", Target.Type);
			return;
		}

		string CliPath = "-";

		if (Target.Platform == UnrealTargetPlatform.Win64)
			CliPath = Path.Combine(PluginDirectory, "Binaries", "Win64", "casttocloud-cli.exe");
		else if (Target.Platform == UnrealTargetPlatform.Mac)
			CliPath = Path.Combine(PluginDirectory, "Binaries", "Mac", "casttocloud-cli");
		else if (Target.Platform == UnrealTargetPlatform.Linux)
			CliPath = Path.Combine(PluginDirectory, "Binaries", "Linux", "casttocloud-cli");

		System.Console.WriteLine("[CastToCloud] CLI will be staged from {0}.", CliPath);

		if (!File.Exists(CliPath))
		{
			System.Console.WriteLine("[CastToCloud] CLI not found on disk. Will not be staged.");
			return;
		}

		RuntimeDependencies.Add(CliPath);
	}

	private bool ShouldStageCli(string ProjectDir, TargetType Type)
	{
		string IniPath = Path.Combine(ProjectDir, "Config", "DefaultCastToCloud.ini");
		string Section = "/Script/CastToCloudShared.CtcSharedSettings";
		string Key = "bStageCliFor" + Type;

		if (!File.Exists(IniPath))
		{
			return false;
		}

		ConfigFile Config = new ConfigFile(new FileReference(IniPath));
		if (!Config.TryGetSection(Section, out ConfigFileSection ConfigSection))
		{
			return false;
		}

		if (!ConfigSection.TryGetLine(Key, out ConfigLine ConfigLine))
		{
			return  false;
		}

		return ConfigLine.Value.Equals("true", System.StringComparison.OrdinalIgnoreCase);
	}
}
