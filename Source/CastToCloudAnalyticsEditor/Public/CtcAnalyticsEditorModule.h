// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Modules/ModuleManager.h>

class SDockTab;
class FSpawnTabArgs;

class FCtcAnalyticsEditorModule : public IModuleInterface
{
	// ~Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// ~End IModuleInterface interface

	void RegisterToolbarExtension();
	void UnregisterToolbarExtension();

	TSharedRef<SDockTab> SpawnEventsViewerTab(const FSpawnTabArgs& SpawnTabArgs);
};
