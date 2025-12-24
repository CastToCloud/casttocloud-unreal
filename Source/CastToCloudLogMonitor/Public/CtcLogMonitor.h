#pragma once

#include <Modules/ModuleManager.h>

class FCtcOutputDevice;

class FCtcLogMonitor : public IModuleInterface
{
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	TUniquePtr<FCtcOutputDevice> OutputDevice;
};
