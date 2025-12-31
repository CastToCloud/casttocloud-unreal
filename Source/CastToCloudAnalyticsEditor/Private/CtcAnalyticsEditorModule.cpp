#include "CtcAnalyticsEditorModule.h"

#include <Framework/Docking/TabManager.h>
#include <Widgets/Docking/SDockTab.h>
#include <WorkspaceMenuStructure.h>
#include <WorkspaceMenuStructureModule.h>

#include "SCtcAnalyticsEditorViewer.h"

namespace
{
	FText const CastToCloudCategory = INVTEXT("Cast To Cloud");

	//TODO: - This should be moved to a shared module (ideally CastToCloudEditor, but CastToCloud with #ifdef is also fine)
	TSharedRef<FWorkspaceItem> GetWorkspaceCategoryItem()
	{
		TSharedPtr<FWorkspaceItem> FoundCategory;

		const TSharedRef<FWorkspaceItem> Parent = WorkspaceMenu::GetMenuStructure().GetStructureRoot();
		TArray<TSharedRef<FWorkspaceItem>> ExistingCategories = Parent->GetChildItems();
		const TSharedRef<FWorkspaceItem>* ExistingCategory = ExistingCategories.FindByPredicate(
			[=](TSharedRef<FWorkspaceItem> const& Category)
			{
				return Category->GetDisplayName().EqualTo(CastToCloudCategory);
			}
		);

		if (ExistingCategory)
		{
			FoundCategory = *ExistingCategory;
		}
		else
		{
			FoundCategory = Parent->AddGroup(CastToCloudCategory);
		}

		return FoundCategory.ToSharedRef();
	}
}

const FName EventsViewerTabName = TEXT("EventsViewer");

void FCtcAnalyticsEditorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(EventsViewerTabName, FOnSpawnTab::CreateRaw(this, &FCtcAnalyticsEditorModule::SpawnEventsViewerTab))
		.SetDisplayName(INVTEXT("Events Viewer"))
		.SetTooltipText(INVTEXT("Visualize events data inside the level viewport"))
		.SetGroup(GetWorkspaceCategoryItem())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.QuadOverdrawMode"));

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
