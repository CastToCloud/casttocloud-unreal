// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <HAL/IConsoleManager.h>
#include <Subsystems/GameInstanceSubsystem.h>

#include "CtcSharedTickable.h"

#include "CtcPerformanceMonitoringSubsystem.generated.h"

/**
 *
 */
UCLASS()
class CASTTOCLOUD_API UCtcPerformanceMonitoringSubsystem : public UGameInstanceSubsystem, public FTickableWithDebug
{
	GENERATED_BODY()

	// ~Begin UGameInstanceSubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	// ~End UGameInstanceSubsystem interface

	// FTickableWithDebug interface
	virtual void OnTick(float DeltaTime) override;
	virtual void OnDebugTick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	// ~FTickableWithDebug interface

	void OnAnomalyDetected();

	TOptional<FDateTime> AnomalyStart;
	int32 NumAnomaliesRecorded = 0;
	static FAutoConsoleCommand TriggerAnomalyDetected;

	TArray<float> RecentFrames;
};
