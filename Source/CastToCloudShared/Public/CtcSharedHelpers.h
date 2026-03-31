// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>

#include <Templates/ValueOrError.h>

class FMonitoredProcess;
class APlayerController;

namespace CastToCloudSharedHelpers
{
	CASTTOCLOUDSHARED_API UWorld* GetCurrentWorld();
	CASTTOCLOUDSHARED_API TOptional<FString> GetWorldPackage(const UWorld* World = nullptr);
	CASTTOCLOUDSHARED_API APlayerController* GetFirstLocalPlayerController(const UWorld* World = nullptr);
	CASTTOCLOUDSHARED_API TValueOrError<FConfigFile, FString> GetPreInitConfig();

	using FGetAutoTransformContextDelegate = TTSDelegate<TOptional<FTransform>()>;
	CASTTOCLOUDSHARED_API FGetAutoTransformContextDelegate& GetAutoTransformContextDelegate();
	CASTTOCLOUDSHARED_API TOptional<FTransform> GetAutoTransformContext();
	CASTTOCLOUDSHARED_API TOptional<FTransform> GetAutoTransformContextFromPlayer();

	struct CASTTOCLOUDSHARED_API FSpawnCliArgs
	{
		FSpawnCliArgs(const FString& InProcessName, const FLogCategoryBase& InLogCategory) : ProcessName(InProcessName), LogCategory(InLogCategory) {}

		FString ProcessName;
		FString CommandLine;
		FLogCategoryBase LogCategory;
	};
	CASTTOCLOUDSHARED_API TUniquePtr<FMonitoredProcess> SpawnCliProcess(const FSpawnCliArgs& Args);
	CASTTOCLOUDSHARED_API TOptional<int32> GetFreeLocalPort();
} // namespace CastToCloudSharedHelpers
