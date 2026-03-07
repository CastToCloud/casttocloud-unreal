// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Misc/Optional.h>
#include <Templates/ValueOrError.h>

class APlayerController;

namespace CastToCloudSharedHelpers
{
	CASTTOCLOUDSHARED_API UWorld* GetCurrentWorld();
	CASTTOCLOUDSHARED_API TOptional<FString> GetWorldPackage(const UWorld* World = nullptr);
	CASTTOCLOUDSHARED_API APlayerController* GetFirstLocalPlayerController(const UWorld* World = nullptr);
	CASTTOCLOUDSHARED_API TValueOrError<FConfigFile, FString> GetPreInitConfig();
} // namespace CastToCloudSharedHelpers
