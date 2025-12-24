#pragma once

#include <Subsystems/GameInstanceSubsystem.h>

#include "CtcPerformanceMonitorSubsystem.generated.h"

class FCtcPerformanceMonitorDataConsumer;

/**
 * Responsible for uploading current and previous performance data to the backend.
 */
UCLASS()
class CASTTOCLOUDPERFORMANCEMONITOR_API UCtcPerformanceMonitorSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

	// ~Begin UGameInstanceSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// ~End UGameInstanceSubsystem interface

	TSharedPtr<FCtcPerformanceMonitorDataConsumer> DataConsumer;
};
