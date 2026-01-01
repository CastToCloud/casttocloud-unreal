// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <EditorSubsystem.h>

#include "CtcAnalyticsEditorSubsystem.generated.h"

UCLASS()
class UCtcAnalyticsEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	void UploadEventsBackground(UWorld* World = nullptr);

private:
	TSharedPtr<SViewport> GetScreenshotViewport() const;

	FBox GetViewportBounds(TSharedPtr<SViewport> InViewport) const;
	TArray<uint8> GetScreenshotImageData(TSharedPtr<SViewport> InViewport) const;
	void UploadDataToBackend(UWorld* World, const FBox Bounds, const TArray<uint8> ImageData);
};
