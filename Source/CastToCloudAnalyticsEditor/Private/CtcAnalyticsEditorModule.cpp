// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAnalyticsEditorModule.h"

#include <Framework/Docking/TabManager.h>
#include <Widgets/Docking/SDockTab.h>
#include <WorkspaceMenuStructure.h>
#include <WorkspaceMenuStructureModule.h>

#include "CtcAnalyticsEditorSubsystem.h"
#include "CtcDefines.h"
#include "SCtcAnalyticsEditorViewer.h"

namespace
{
	FText const CastToCloudCategory = INVTEXT("Cast To Cloud");

	// TODO: - This should be moved to a shared module (ideally CastToCloudEditor, but CastToCloud with #ifdef is also fine)
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
} // namespace

const FName EventsViewerTabName = TEXT("EventsViewer");

void FCtcAnalyticsEditorModule::StartupModule()
{
	RegisterToolbarExtension();

	// clang-format off
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(EventsViewerTabName, FOnSpawnTab::CreateRaw(this, &FCtcAnalyticsEditorModule::SpawnEventsViewerTab))
		.SetDisplayName(INVTEXT("Events Viewer"))
		.SetTooltipText(INVTEXT("Visualize events data inside the level viewport"))
		.SetGroup(GetWorkspaceCategoryItem())
		.SetIcon(FSlateIcon(FAppStyle::GetAppStyleSetName(), "EditorViewport.QuadOverdrawMode"));
	// clang-format on

	FGlobalTabmanager::Get()->RegisterDefaultTabWindowSize(EventsViewerTabName, FVector2D(1080, 600));
}

void FCtcAnalyticsEditorModule::ShutdownModule()
{
	UnregisterToolbarExtension();

	FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(EventsViewerTabName);
}

void FCtcAnalyticsEditorModule::RegisterToolbarExtension()
{
	FToolMenuOwnerScoped OwnerScoped(this);

#if UE_VERSION_NEWER_THAN_OR_EQUAL(5, 6, 0)
	const FName MenuToExtend = "LevelEditor.ViewportToolbar.Settings";
#else
	const FName MenuToExtend = "LevelEditor.LevelEditorToolBar.LevelToolbarQuickSettings";
#endif

	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu(MenuToExtend);
	FToolMenuSection& SettingsSection = Menu->AddSection(FName("CastToCloud"), INVTEXT("Cast To Cloud"));

	FToolMenuEntry& Entry = SettingsSection.AddMenuEntry(
		FName("UploadBackground"),
		INVTEXT("Upload Background"),
		INVTEXT("Upload the current viewport as a screenshot to be used as an analytics background."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Plus")),
		FExecuteAction::CreateLambda(
			[]()
			{
				GEditor->GetEditorSubsystem<UCtcAnalyticsEditorSubsystem>()->UploadEventsBackground();
			}
		)
	);
}

void FCtcAnalyticsEditorModule::UnregisterToolbarExtension()
{
	UToolMenus::UnregisterOwner(this);
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
