// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Engine/DeveloperSettings.h>

#include "CtcApiKey.h"
#include "CtcConfigurationSettings.h"

#include "CtcSharedSettings.generated.h"

UENUM()
enum class ECtcAnalyticsSpatialTracking : uint8
{
	Disabled,
	PlayerPawn,
	Camera,

	// TODO: Would it make sense to have MouseLocation (slate cursor coords) and maybe ProjectMouseLocation (cursor in 3d space) ?
};

/**
 * Shared configuration settings shared between all CastToCloud modules
 */
UCLASS(config = CastToCloud, defaultconfig, meta = (DisplayName = "Cast To Cloud"))
class CASTTOCLOUD_API UCtcSharedSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Shared", AdvancedDisplay, meta = (ConfigRestartRequired = true))
	FString ApiUrl = TEXT("https://api.casttocloud.com");

	UPROPERTY(Config, EditAnywhere, Category = "Shared", AdvancedDisplay, meta = (ConfigRestartRequired = true))
	FString DashboardUrl = TEXT("https://dashboard.casttocloud.com");

#if WITH_EDITORONLY_DATA
	// NOTE: WITH_EDITORONLY_DATA here is to ensure we get an error when using the private key inside runtime modules
	/*
	 * API Key used by the editor to perform requests only developers should have access to
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Shared", meta = (AllowedSensitivity = "low+medium+high"))
	FCtcApiKey DeveloperApiKey;
#endif

	/*
	 * API Key used by the packaged clients (and PIE sessions) to perform end-user requests.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Shared", meta = (AllowedSensitivity = "low"))
	FCtcApiKey RuntimeApiKey;

	UFUNCTION(Category = "Analytics", meta = (CallInSettings, CanCallInSettings = NeedsToSetAnalyticsProvider, DisplayName = "Setup Analytics Provider"))
	void SetupAnalyticsProvider();

	UFUNCTION(Category = "Analytics")
	bool NeedsToSetAnalyticsProvider() const;

	UPROPERTY(Config, EditAnywhere, Category = "Analytics|Sending")
	FCtcConfigurationSettings AllowedExecutables = ProductionGameClients;

	UPROPERTY(Config, EditAnywhere, Category = "Analytics|Sending", meta = (Units = "s"))
	float SendInterval = 60.0f;

	UPROPERTY(Config, BlueprintReadOnly, Category = "Analytics|Attribution")
	FString PlatformAttribution = TEXT("");

	UPROPERTY(Config, EditAnywhere, Category = "Analytics|Attribution")
	bool bEnableGeolocationAttribution = true;

	UPROPERTY(Config, EditAnywhere, Category = "Analytics|AutoTracking")
	bool bAutoStartSession = true;

	UPROPERTY(Config, EditAnywhere, Category = "Analytics|AutoTracking")
	bool bAutoWorldChangeTracking = true;

	UPROPERTY(Config, EditAnywhere, Category = "Analytics|AutoTracking")
	bool bAutoPlayerMoveTracking = false;

	UPROPERTY(Config, EditAnywhere, Category = "Analytics|AutoTracking", meta = (editcondition = "bAutoPlayerMoveTracking", Units = "s"))
	float AutoPlayerMoveTrackingInterval = 10.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Analytics|AutoTracking", meta = (editcondition = "bAutoPlayerMoveTracking", InvalidEnumValues = "Disabled"))
	ECtcAnalyticsSpatialTracking AutoPlayerMoveTrackingMethod = ECtcAnalyticsSpatialTracking::PlayerPawn;

#if WITH_EDITOR
	void ShowSettings();
#endif

#if WITH_EDITOR
	void SaveToDefaultConfig();
#endif

private:
	// Begin UDeveloperSettings interface
	virtual void PostInitProperties() override;
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Cast To Cloud"); }
	// End UDeveloperSettings interface
};
