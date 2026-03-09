// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Modules/ModuleManager.h>

class CASTTOCLOUDMETRICS_API FCtcMetricsModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
