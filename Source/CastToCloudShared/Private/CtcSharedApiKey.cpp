// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcSharedApiKey.h"

const FString FCtcSharedApiKey::PlaceholderKey = TEXT("ctc_");

FCtcSharedApiKey::operator FString() const
{
	return ApiKey;
}
