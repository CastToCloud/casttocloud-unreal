// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <EditorSubsystem.h>

#include "CtcAnalyticsBackgroundSubsystem.generated.h"

UCLASS()
class UCtcAnalyticsBackgroundSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	void UploadCurrentViewportAsBackground();
	void UploadCurrentViewportAsBackground(const FString& InName);

private:
	// ~Begin UEditorSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// ~End UEditorSubsystem interface

	/**
	 * Callback executed when the Analytics Editor module wants to build the Gameplay Events Menu
	 */
	void OnBuildGameplayEventsMenu(FMenuBuilder& MenuBuilder);
	/**
	 * Callback executed after a world is initialized
	 */
	void OnPostWorldInitialization(UWorld* World, const FWorldInitializationValues IVS);

	void UploadDataToBackend(UWorld* World, const FBox Bounds, const TArray<uint8> ImageData, const FString& Name);
	FBox GetViewportBounds(TSharedPtr<SViewport> InViewport) const;
	TArray<uint8> GetScreenshotImageData(TSharedPtr<SViewport> InViewport) const;

	UPROPERTY()
	FString BackgroundName;
};
