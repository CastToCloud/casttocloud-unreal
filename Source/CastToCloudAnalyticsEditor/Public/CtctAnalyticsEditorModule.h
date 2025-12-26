#pragma once

#include <Modules/ModuleManager.h>

class FCtcAnalyticsEditorModule : public IModuleInterface
{
	// ~Begin IModuleInterface interface
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
	// ~End IModuleInterface interface
};
