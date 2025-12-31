// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include "CtcPathReference.generated.h"

USTRUCT()
struct FCtcPathReference
{
	GENERATED_BODY()

	operator FString() const;

	/**
	 * The path to the file.
	 */
	UPROPERTY(EditAnywhere, Category = CastToCloud)
	FString Path;
};
