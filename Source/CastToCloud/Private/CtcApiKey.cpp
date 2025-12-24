// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcApiKey.h"

const FString FCtcApiKey::PlaceholderKey = TEXT("ctc_");

FCtcApiKey::operator FString() const
{
	return ApiKey;
}