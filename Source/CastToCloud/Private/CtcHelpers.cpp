// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcHelpers.h"

#include <Engine/GameEngine.h>
#include <UObject/Package.h>

#if WITH_EDITOR
#include <Editor.h>
#endif

UWorld* CastToCloudHelpers::GetCurrentWorld()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		if (FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext())
		{
			return PIEWorldContext->World();
		}

		return GEditor->GetEditorWorldContext().World();
	}
#endif
	if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
	{
		return GameEngine->GetGameWorld();
	}

	return nullptr;
}

TOptional<FString> CastToCloudHelpers::GetWorldPackage(const UWorld* World)
{
	const UWorld* Target = World ? World : GetCurrentWorld();

	if (!Target || !Target->GetPackage())
	{
		return {};
	}

	const FString CleanPackageName = UWorld::StripPIEPrefixFromPackageName(Target->GetPackage()->GetName(), Target->StreamingLevelsPrefix);
	if (CleanPackageName.StartsWith(TEXT("/Temp/Untitled")))
	{
		return {};
	}

	return CleanPackageName;
}
