// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Templates/ValueOrError.h>

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
} // namespace CastToCloudSharedHelpers
