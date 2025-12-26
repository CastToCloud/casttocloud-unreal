// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include "CtcApiKey.generated.h"

USTRUCT()
struct CASTTOCLOUD_API FCtcApiKey
{
	GENERATED_BODY()

	static const FString PlaceholderKey;

	operator FString() const;

	UPROPERTY(EditAnywhere, Category = "CastToCloud")
	FString ApiKey = PlaceholderKey;
};
