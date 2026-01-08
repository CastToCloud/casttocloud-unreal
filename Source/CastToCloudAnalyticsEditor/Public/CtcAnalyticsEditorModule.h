// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Modules/ModuleManager.h>

class FMenuBuilder;
class SWidget;
class SDockTab;
class FSpawnTabArgs;

class FCtcAnalyticsEditorModule : public IModuleInterface
{
public:
	static TMulticastDelegate<void(FMenuBuilder& MenuBuilder)> OnBuildGameplayEventsMenu;

private:
	// ~Begin IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// ~End IModuleInterface interface

	void RegisterToolbarExtension();
	void UnregisterToolbarExtension();

	TSharedRef<SWidget> GenerateToolbarMenuContent();

	TSharedRef<SDockTab> SpawnEventsViewerTab(const FSpawnTabArgs& SpawnTabArgs);
};
