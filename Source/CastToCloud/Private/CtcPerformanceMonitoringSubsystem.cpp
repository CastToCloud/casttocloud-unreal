// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcPerformanceMonitoringSubsystem.h"

#include <Algo/Accumulate.h>
#include <Camera/PlayerCameraManager.h>
#include <Engine/GameInstance.h>
#include <Engine/World.h>
#include <GameFramework/Pawn.h>
#include <GameFramework/PlayerController.h>
#include <HAL/IConsoleManager.h>
#include <Misc/App.h>
#include <Misc/CommandLine.h>

#include "CtcAnalyticsBPFL.h"
#include "CtcMetricsLog.h"
#include "CtcMetricsTraceHelpers.h"
#include "CtcSharedHelpers.h"
#include "CtcSharedSettings.h"

extern ENGINE_API float GAverageFPS;

// clang-format off
FAutoConsoleCommand UCtcPerformanceMonitoringSubsystem::TriggerAnomalyDetected(
	TEXT("CastToCloud.PerformanceMonitoring.TriggerAnomalyDetected"),
	TEXT("Debug command that simulates a detected anomaly. Bypasses detection logic and immediately records and uploads a trace snapshot."),
	FConsoleCommandWithWorldArgsAndOutputDeviceDelegate::CreateLambda(
		[](const TArray<FString>& Args, UWorld* World, FOutputDevice& Device)
		{
			UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
			UCtcPerformanceMonitoringSubsystem* Subsystem = GameInstance ? GameInstance->GetSubsystem<UCtcPerformanceMonitoringSubsystem>() : nullptr;
			if (Subsystem)
			{
				Subsystem->OnAnomalyDetected();
			}
		}
	)
);
// clang-format on

bool UCtcPerformanceMonitoringSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (FParse::Param(FCommandLine::Get(), TEXT("MetricsAnyConfiguration")))
	{
		return true;
	}

	// TODO: We should move this to tick and also take other things into account, e.g.: CPU throttling, or not in focus
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	return Settings && Settings->TrackingEnabled.IsCurrentConfigurationAllowed();
}

void UCtcPerformanceMonitoringSubsystem::Tick(float DeltaTime)
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();

	RecentFrames.Add(GAverageFPS);
	const int32 ExtraSamples = RecentFrames.Num() - Settings->LowFpsSampleCount;

	if (ExtraSamples < 0)
	{
		// Not enough samples yet to make a decision
		return;
	}
	if (ExtraSamples > 0)
	{
		// Too many samples, removing old ones before calculating
		RecentFrames.RemoveAt(0, ExtraSamples, EAllowShrinking::No);
	}

	const float CurrentAverage = Algo::Accumulate(RecentFrames, 0.0f) / RecentFrames.Num();
	const bool bLowFramerate = CurrentAverage < Settings->LowFpsAverageThreshold;

	if (AnomalyStart)
	{
		const FTimespan TimePassed = FDateTime::UtcNow() - *AnomalyStart;
		const double SecondsPassed = TimePassed.GetTotalSeconds();

		if (!bLowFramerate)
		{
			UE_LOG(LogCtcMetrics, Display, TEXT("Low framerate recovered. DurationSeconds=%s"), *LexToString(SecondsPassed));
			AnomalyStart.Reset();
		}
		else if (SecondsPassed >= Settings->LowFpsDurationThreshold)
		{
			UE_LOG(LogCtcMetrics, Display, TEXT("Low framerate duration threshold reached. DurationSeconds=%s"), *LexToString(SecondsPassed));
			AnomalyStart.Reset();

			OnAnomalyDetected();
		}
	}
	else if (bLowFramerate)
	{
		UE_LOG(LogCtcMetrics, Display, TEXT("Low framerate anomaly window started."));
		AnomalyStart = FDateTime::UtcNow();
	}
}

ETickableTickType UCtcPerformanceMonitoringSubsystem::GetTickableTickType() const
{
	const bool bIsCDO = HasAnyFlags(RF_ClassDefaultObject);
	return !bIsCDO ? ETickableTickType::Always : ETickableTickType::Never;
}

TStatId UCtcPerformanceMonitoringSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCtcPerformanceMonitoringSubsystem, STATGROUP_Tickables);
}

UWorld* UCtcPerformanceMonitoringSubsystem::GetTickableGameObjectWorld() const
{
	return CastToCloudSharedHelpers::GetCurrentWorld();
}

void UCtcPerformanceMonitoringSubsystem::OnAnomalyDetected()
{
	const FString TraceId = FApp::GetSessionId().ToString() + TEXT("_") + LexToString(NumAnomaliesRecorded++);

	CastToCloudMetricsTraceHelpers::WriteAndUploadTrace(TraceId);

	TOptional<FTransform> PlayerTransform = {};
	if (const APlayerController* LocalController = CastToCloudSharedHelpers::GetFirstLocalPlayerController())
	{
		if (const APawn* PlayerPawn = LocalController->GetPawn())
		{
			PlayerTransform = PlayerPawn->GetActorTransform();
		}
		else if (const APlayerCameraManager* CameraManager = LocalController->PlayerCameraManager)
		{
			PlayerTransform = FTransform(CameraManager->GetCameraRotation(), CameraManager->GetCameraLocation());
		}
	}

	TArray<FAnalyticsEventAttribute> Attributes;
	Attributes.Emplace(TEXT("trace_id"), TraceId);
	// TODO the event above should also contain the upscaling applied if any or "native"
	UCtcAnalyticsBPFL::RecordEventWithOptionalTransform(TEXT("Trace"), PlayerTransform, Attributes);
}
