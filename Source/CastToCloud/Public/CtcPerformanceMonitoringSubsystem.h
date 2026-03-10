// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Subsystems/GameInstanceSubsystem.h>

#include "CtcSharedTickable.h"

#include "CtcPerformanceMonitoringSubsystem.generated.h"

enum class EFrameHitchType;
struct FAnalyticsEventAttribute;

/**
 *
 */
UCLASS()
class CASTTOCLOUD_API UCtcPerformanceMonitoringSubsystem : public UGameInstanceSubsystem, public FTickableWithDebug
{
	GENERATED_BODY()

public:
	void RecordBadPerformance(TArray<FAnalyticsEventAttribute> Attributes = TArray<FAnalyticsEventAttribute>());

private:
	// ~Begin UGameInstanceSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	// ~End UGameInstanceSubsystem interface

	// FTickableWithDebug interface
	virtual void OnTick(float DeltaTime) override;
	virtual void OnDebugTick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	// ~FTickableWithDebug interface

	void OnHitchDetected(EFrameHitchType Type, float Duration);
	void OnLowFPSDetected(float Average, float Duration);

	TOptional<FDateTime> LowFPSStartTime;
	TArray<float> RecentFrames;

	int32 BadPerformanceReportsCount = 0;
};
