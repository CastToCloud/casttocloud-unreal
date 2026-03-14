#pragma once

#include <Modules/ModuleManager.h>

class FCtcLogMonitor : public IModuleInterface
{
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
