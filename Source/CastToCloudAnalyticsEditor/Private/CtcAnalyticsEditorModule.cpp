#include "CtcAnalyticsEditorModule.h"

#include <WorkspaceMenuStructure.h>
#include <WorkspaceMenuStructureModule.h>

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
	TSharedRef<SDockTab> DockTab = SAssignNew(AssetAuditTab, SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SAssignNew(AssetAuditUI, SAssetAuditBrowser)
		];

	DockTab->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateLambda([this](TSharedRef<SDockTab>)
		{
			if (AssetAuditUI.IsValid())
			{
				AssetAuditUI.Pin()->OnClose();
			}
		}));

	return DockTab;
}

IMPLEMENT_MODULE(FCtcAnalyticsEditorModule, CastToCloudAnalyticsEditor)
