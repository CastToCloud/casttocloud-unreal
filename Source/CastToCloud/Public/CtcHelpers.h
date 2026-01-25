// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Misc/Optional.h>

class APlayerController;

namespace CastToCloudHelpers
{
	CASTTOCLOUD_API UWorld* GetCurrentWorld();
	CASTTOCLOUD_API TOptional<FString> GetWorldPackage(const UWorld* World = nullptr);
	CASTTOCLOUD_API APlayerController* GetFirstLocalPlayerController(const UWorld* World = nullptr);
} // namespace CastToCloudHelpers
