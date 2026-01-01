// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcHelpers.h"

TOptional<FString> CastToCloudHelpers::GetWorldPackage(const UWorld* World)
{
	if (!World || !World->GetPackage())
	{
		return {};
	}

	return UWorld::StripPIEPrefixFromPackageName(World->GetPackage()->GetName(), World->StreamingLevelsPrefix);
}
