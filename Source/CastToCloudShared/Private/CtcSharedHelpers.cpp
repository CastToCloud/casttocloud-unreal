// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcSharedHelpers.h"

#include <Engine/GameEngine.h>
#include <HAL/FileManager.h>
#include <Misc/CommandLine.h>
#include <Misc/ConfigCacheIni.h>
#include <Misc/ConfigContext.h>
#include <UObject/Package.h>

#if WITH_EDITOR
#include <Editor.h>
#endif

UWorld* CastToCloudSharedHelpers::GetCurrentWorld()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		if (FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext())
		{
			return PIEWorldContext->World();
		}

		return GEditor->GetEditorWorldContext().World();
	}
#endif
	if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
	{
		return GameEngine->GetGameWorld();
	}

	return nullptr;
}

TOptional<FString> CastToCloudSharedHelpers::GetWorldPackage(const UWorld* World)
{
	const UWorld* Target = World ? World : GetCurrentWorld();

	if (!Target || !Target->GetPackage())
	{
		return {};
	}

	const FString CleanPackageName = UWorld::StripPIEPrefixFromPackageName(Target->GetPackage()->GetName(), Target->StreamingLevelsPrefix);
	if (CleanPackageName.StartsWith(TEXT("/Temp/Untitled")))
	{
		return {};
	}

	return CleanPackageName;
}

APlayerController* CastToCloudSharedHelpers::GetFirstLocalPlayerController(const UWorld* World)
{
	const UWorld* Target = World ? World : GetCurrentWorld();

	if (!Target)
	{
		return nullptr;
	}

	const ULocalPlayer* LocalPlayer = Target->GetFirstLocalPlayerFromController();
	if (!LocalPlayer)
	{
		return nullptr;
	}

	return LocalPlayer->PlayerController;
}

TValueOrError<FConfigFile, FString> CastToCloudSharedHelpers::GetPreInitConfig()
{
	// NOTE: This is basically an inline re-implementation of FTempCommandLineScope
	const bool bWasCommandLineInitialized = FCommandLine::IsInitialized();
	if (!bWasCommandLineInitialized)
	{
		FCommandLine::Set(TEXT(""));
	}

	ON_SCOPE_EXIT
	{
		if (!bWasCommandLineInitialized)
		{
			FCommandLine::Reset();
		}
	};

	const FString PreInitIni = FPaths::ProjectIntermediateDir() / TEXT("CastToCloud") / TEXT("PreInitCastToCloud.ini");
	if (!IFileManager::Get().FileExists(*PreInitIni))
	{
		const FString MissingFileError = FString::Printf(TEXT("PreInitIni not found at %s"), *PreInitIni);
		return MakeError(MissingFileError);
	}

	FConfigFile Ini;
	FConfigContext::ReadSingleIntoLocalFile(Ini).Load(*PreInitIni);

	return MakeValue(Ini);
}
