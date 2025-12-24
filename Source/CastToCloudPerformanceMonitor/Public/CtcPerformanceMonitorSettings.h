#pragma once

#include <Engine/DeveloperSettings.h>

#include "CtcPerformanceMonitorSettings.generated.h"

UCLASS(config = CastToCloud, defaultconfig)
class CASTTOCLOUDPERFORMANCEMONITOR_API UCtcPerformanceMonitorSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:

	// Extra arguments to be used when starting the trace
	// NOTE: "default" is the default value from FTraceAuxiliary::Start's Channels parameter
	UPROPERTY(Config, EditAnywhere, Category = "PerformanceMonitor")
	FString TraceChannels = TEXT("default");
};
