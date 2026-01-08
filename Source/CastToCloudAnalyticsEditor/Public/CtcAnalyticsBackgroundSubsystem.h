// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <EditorSubsystem.h>

#include "CtcAnalyticsBackgroundSubsystem.generated.h"

UCLASS()
class UCtcAnalyticsBackgroundSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	void UploadEventsBackground(UWorld* World = nullptr);

private:
	FBox GetViewportBounds(TSharedPtr<SViewport> InViewport) const;
	TArray<uint8> GetScreenshotImageData(TSharedPtr<SViewport> InViewport) const;
	void UploadDataToBackend(UWorld* World, const FBox Bounds, const TArray<uint8> ImageData);
};
