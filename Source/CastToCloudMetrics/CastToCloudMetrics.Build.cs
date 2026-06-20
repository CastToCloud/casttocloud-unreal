// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildTool;

public class CastToCloudMetrics : ModuleRules
{
	public CastToCloudMetrics(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"HTTP",
				"Json",

				"CastToCloudShared",
			}
		);

		ExportDefinesFromInis(Target);
	}

	// Certain properties need to be access very early during the engine initialization before the config is ready
	// For those cases, we "export" the configured values as defines at build time
	private void ExportDefinesFromInis(ReadOnlyTargetRules Target)
	{
		if (Target.ProjectFile == null)
		{
			return;
		}

		FileReference Config = FileReference.Combine(Target.ProjectFile.Directory, "Config", "DefaultCastToCloud.ini");
		ExternalDependencies.Add(Config.FullName);

		if (!FileReference.Exists(Config))
		{
			return;
		}

		ConfigFile Ini = new ConfigFile(Config);
		if (!Ini.TryGetSection("/Script/CastToCloudShared.CtcSharedSettings", out var Section))
		{
			return;
		}

		if (Section.TryGetLine("GameTailSizeBytes", out var TailSizeLine)
			&& int.TryParse(TailSizeLine.Value, out int TailSize))
		{
			Target.Logger.LogInformation("Exporting CTC_EXPORTED_GAME_TAIL_SIZE_BYTES={TailSize}", TailSize);
			PublicDefinitions.Add("CTC_EXPORTED_GAME_TAIL_SIZE_BYTES=" + TailSize);
		}
		else
		{
			Target.Logger.LogInformation("Skipping CTC_EXPORTED_GAME_TAIL_SIZE_BYTES");
		}
	}
}
