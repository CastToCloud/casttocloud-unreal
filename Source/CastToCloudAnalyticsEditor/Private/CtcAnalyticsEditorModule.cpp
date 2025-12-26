#include "CtcAnalyticsEditorModule.h"

#include <WorkspaceMenuStructure.h>
#include <WorkspaceMenuStructureModule.h>

#include "SCtcAnalyticsEditorViewer.h"

const FName EventsViewerTabName = TEXT("EventsViewer");

void FCtcAnalyticsEditorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(EventsViewerTabName, FOnSpawnTab::CreateRaw(this, &FCtcAnalyticsEditorModule::SpawnEventsViewerTab))
		.SetDisplayName(INVTEXT("Events Viewer"))
		.SetTooltipText(INVTEXT("Events Viewer Tooltip"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetDeveloperToolsAuditCategory()) //TODO: make a custom category for us
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "LevelEditor.Audit"));
	FGlobalTabmanager::Get()->RegisterDefaultTabWindowSize(EventsViewerTabName, FVector2D(1080, 600));
}

void FCtcAnalyticsEditorModule::ShutdownModule()
{
	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(EventsViewerTabName);
}

TSharedRef<SDockTab> FCtcAnalyticsEditorModule::SpawnEventsViewerTab(const FSpawnTabArgs& SpawnTabArgs)
{
	// clang-format off
	TSharedRef <SDockTab> Tab = SNew(SDockTab)
		.TabRole(NomadTab)
		[
			SNew(SCtcAnalyticsEditorViewer)
		];
	// clang-format on

	return Tab;
}

IMPLEMENT_MODULE(FCtcAnalyticsEditorModule, CastToCloudAnalyticsEditor)
