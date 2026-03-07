// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include "CtcSharedApiKey.generated.h"

USTRUCT()
struct CASTTOCLOUDSHARED_API FCtcSharedApiKey
{
	GENERATED_BODY()

	static const FString PlaceholderKey;

	operator FString() const;

	UPROPERTY(EditAnywhere, Category = "CastToCloud")
	FString ApiKey = PlaceholderKey;
};
