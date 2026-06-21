// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Subsystems/GameInstanceSubsystem.h>
#include <Tickable.h>

#include "CtcPerformanceMonitoringSubsystem.generated.h"

enum class EFrameHitchType : uint8;
struct FAnalyticsEventAttribute;

/**
 *
 */
UCLASS()
class CASTTOCLOUD_API UCtcPerformanceMonitoringSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	void RecordBadPerformance(TArray<FAnalyticsEventAttribute> Attributes = TArray<FAnalyticsEventAttribute>());

private:
	// ~Begin UGameInstanceSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	// ~End UGameInstanceSubsystem interface

	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	// ~FTickableGameObject interface

	void StartTracing();
	void TickLowFPSDetection();
	void TickDebug();

	void OnHitchDetected(EFrameHitchType Type, float Duration);
	void OnLowFPSDetected(float Average, float Duration);
	void OnApplicationActivationStateChanged(const bool bIsActive);

	TOptional<FDateTime> LowFPSStartTime;
	TArray<float> RecentFrames;

	int32 BadPerformanceReportsCount = 0;
};
