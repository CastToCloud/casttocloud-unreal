// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <CoreMinimal.h>
#include <Subsystems/GameInstanceSubsystem.h>
#include <Tickable.h>

#include "CtcPerformanceMonitoringSubsystem.generated.h"

/**
 *
 */
UCLASS()
class CASTTOCLOUD_API UCtcPerformanceMonitoringSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

	// ~Begin UGameInstanceSubsystem interface
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	// ~End UGameInstanceSubsystem interface

	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	// ~FTickableGameObject interface

	void OnAnomalyDetected();

	TOptional<FDateTime> AnomalyStart;
	int32 NumAnomaliesRecorded = 0;
	static FAutoConsoleCommand TriggerAnomalyDetected;

	TArray<float> RecentFrames;
};
