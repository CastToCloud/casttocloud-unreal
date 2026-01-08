// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAnalyticsEditorModule.h"

#include <Framework/Docking/TabManager.h>
#include <Widgets/Docking/SDockTab.h>
#include <WorkspaceMenuStructure.h>
#include <WorkspaceMenuStructureModule.h>

#include "CtcAnalyticsBackgroundSubsystem.h"
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

	UToolMenu* LevelEditorToolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.User");
	FToolMenuSection& Section = LevelEditorToolbar->AddSection("CastToCloud");

	FToolMenuEntry SectionDropdown = FToolMenuEntry::InitComboButton(
		"CastToCloud",
		FUIAction(),
		FOnGetContent::CreateRaw(this, &FCtcAnalyticsEditorModule::GenerateToolbarMenuContent),
		INVTEXT("Cast To Cloud"),
		INVTEXT("Cast To Cloud Actions"),
		FSlateIcon(),
		false);
	SectionDropdown.StyleNameOverride = "CalloutToolbar";
	Section.AddEntry(SectionDropdown);
}

void FCtcAnalyticsEditorModule::UnregisterToolbarExtension()
{
	UToolMenus::UnregisterOwner(this);
}

TSharedRef<SWidget> FCtcAnalyticsEditorModule::GenerateToolbarMenuContent()
{
	FMenuBuilder MenuBuilder(true, nullptr);

	MenuBuilder.BeginSection("GameplayEvents", INVTEXT("Gameplay Events"));
	MenuBuilder.AddMenuEntry(
		INVTEXT("Upload Background"),
		INVTEXT("Uploads the current viewport as a background"),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), TEXT("Icons.Plus")),
		FUIAction(FExecuteAction::CreateLambda(
			[]()
			{
				GEditor->GetEditorSubsystem<UCtcAnalyticsBackgroundSubsystem>()->UploadEventsBackground();
			}
		))
	);
	//TODO: Add input field to change world name
	// ^ for the above check how we store it in the database too

	//TODO: Checkbox if we should warn about configuration issues
	//TODO: Add warning state button - provider not configured, missing API Keys, current configuration doesn't allow sending events
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
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
