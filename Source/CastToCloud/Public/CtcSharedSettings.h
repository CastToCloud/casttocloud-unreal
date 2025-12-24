// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Engine/DeveloperSettings.h>

#include "CtcApiKey.h"
#include "CtcConfigurationSettings.h"

#include "CtcSharedSettings.generated.h"

UENUM()
enum class ECtcAnalyticsLocationTracking : uint8
{
	Disabled,
	PlayerPawnLocation,
	CameraLocation,

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

	UPROPERTY(Config, EditAnywhere, Category = "Analytics")
	FCtcConfigurationSettings AllowedExecutables = ProductionGameClients;

	UPROPERTY(Config, EditAnywhere, Category = "Analytics")
	bool bAutoStartSession = true;

	UPROPERTY(Config, EditAnywhere, Category = "Analytics")
	bool bAutomatedWorldChangeTracking = true;

	UPROPERTY(Config, EditAnywhere, Category = "Analytics")
	bool bAutomatedGeoTracking = true;

	UPROPERTY(Config, EditAnywhere, Category = "Analytics")
	ECtcAnalyticsLocationTracking AutomatedLocationTracking = ECtcAnalyticsLocationTracking::Disabled;

	UPROPERTY(Config, EditAnywhere, Category = "Analytics")
	float SendInterval = 60.0f;

	UPROPERTY(Config)
	FString PlatformAttribution = TEXT("");

	UFUNCTION(Category = "Analytics", meta = (CallInSettings, CanCallInSettings = NeedsToSetAnalyticsProvider, DisplayName = "Setup Analytics Provider"))
	void SetupAnalyticsProvider();

	UFUNCTION(Category = "Analytics")
	bool NeedsToSetAnalyticsProvider() const;

	void ShowSettings();

	void SaveToDefaultConfig();

private:
	// Begin UDeveloperSettings interface
	virtual void PostInitProperties() override;
	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override { return TEXT("Cast To Cloud"); }
	// End UDeveloperSettings interface
};