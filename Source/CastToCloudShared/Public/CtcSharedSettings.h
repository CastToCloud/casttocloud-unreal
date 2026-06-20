// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Engine/DeveloperSettings.h>

#include "CtcSharedApiKey.h"
#include "CtcSharedConfigurationSettings.h"

#include "CtcSharedSettings.generated.h"

UENUM()
enum class ECtcSharedAnalyticsSpatialTracking : uint8
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
class CASTTOCLOUDSHARED_API UCtcSharedSettings : public UDeveloperSettings
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
	FCtcSharedApiKey DeveloperApiKey;
#endif

	/*
	 * API Key used by the packaged clients (and PIE sessions) to perform end-user requests.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Shared", meta = (AllowedSensitivity = "low"))
	FCtcSharedApiKey RuntimeApiKey;

	UFUNCTION(Category = "Analytics", meta = (CallInSettings, CanCallInSettings = NeedsToSetAnalyticsProvider, DisplayName = "Setup Analytics Provider"))
	void SetupAnalyticsProvider();

	UFUNCTION(Category = "Analytics")
	bool NeedsToSetAnalyticsProvider() const;

	UPROPERTY(Config, EditAnywhere, Category = "Analytics|Sending")
	FCtcSharedConfigurationSettings AllowedExecutables = PackagedGameClients;

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
	bool bAutoPlayerMoveTracking = true;

	UPROPERTY(Config, EditAnywhere, Category = "Analytics|AutoTracking", meta = (editcondition = "bAutoPlayerMoveTracking", Units = "s"))
	float AutoPlayerMoveTrackingInterval = 5.0f;

	UPROPERTY(Config, EditAnywhere, Category = "Analytics|AutoTracking", meta = (editcondition = "bAutoPlayerMoveTracking", InvalidEnumValues = "Disabled"))
	ECtcSharedAnalyticsSpatialTracking AutoPlayerMoveTrackingMethod = ECtcSharedAnalyticsSpatialTracking::PlayerPawn;

	UPROPERTY(Config, EditAnywhere, Category = "Monitoring|PerformanceTracking")
	FCtcSharedConfigurationSettings TrackingEnabled = DevGameClients;
	/*
	 * Tail buffer size used in Editor builds (Unreal Insights default).
	 * Not configurable via the plugin, but can be overridden at launch with `-tracetailmb=`.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Monitoring|PerformanceTracking", meta = (Units = "Bytes"))
	uint32 EditorTailSizeBytes = 0;
	/*
	 * Tail buffer size used in packaged games.
	 * Larger values retain more trace history but increase memory usage and output file size.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Monitoring|PerformanceTracking", meta = (Units = "Bytes"))
	uint32 GameTailSizeBytes = 4 << 20;
	/*
	 * Trace channels used by the in-memory trace
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Monitoring|PerformanceTracking")
	FString TraceChannels = TEXT("cpu,gpu,frame,log,bookmark,screenshot,region"); // TODO: make this bitmap thingy with all possible values
	/*
	 * Number of recent frames kept for FPS averaging
	 * The average FPS will not be evaluated until at least this many frames have been collected.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Monitoring|PerformanceTracking")
	int LowFpsSampleCount = 500;
	/*
	 * Average FPS threshold below which low performance monitoring starts.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Monitoring|PerformanceTracking")
	float LowFpsAverageThreshold = 30.0f;
	/*
	 * Time (in seconds) the average FPS must remain below the threshold before reporting.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Monitoring|PerformanceTracking", meta = (Units = "s"))
	float LowFpsDurationThreshold = 5.0f;

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
