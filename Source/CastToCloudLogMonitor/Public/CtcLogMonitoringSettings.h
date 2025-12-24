#pragma once

#include <Engine/DeveloperSettings.h>

#include "CtcConfigurationSettings.h"
#include "CtcLogMonitoringSettings.generated.h"

UCLASS(config = CastToCloud, defaultconfig)
class CASTTOCLOUDLOGMONITOR_API UCtcLogMonitoringSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(config, EditAnywhere)
	float LogUploadInterval = 5.0f;

	UPROPERTY(config, EditAnywhere, meta = (ConfigRestartRequired = true))
	FCtcConfigurationSettings AllowedExecutables = ProductionDedicatedServer;
};
