// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "SCtcAnalyticsEditorViewerSource.h"

#include <Widgets/Layout/SWidgetSwitcher.h>
#include <Widgets/Input/SFilePathPicker.h>
#include <EditorDirectories.h>

namespace
{
	FText GetEditorSourceAsText(ECtcAnalyticsEditorSource InValue)
	{
		const UEnum* Enum = StaticEnum<ECtcAnalyticsEditorSource>();
		return Enum->GetDisplayNameTextByValue(static_cast<int32>(InValue));
	}

	int32 GetEditorSourceAsIndex(ECtcAnalyticsEditorSource InValue)
	{
		const UEnum* Enum = StaticEnum<ECtcAnalyticsEditorSource>();
		return Enum->GetIndexByValue(static_cast<int32>(InValue));
	}
}

void SCtcAnalyticsEditorViewerSource::Construct(const FArguments& InArgs)
{
	static TArray<TSharedPtr<ECtcAnalyticsEditorSource>> AllEnums = []()
	{
		TArray<TSharedPtr<ECtcAnalyticsEditorSource>> Result;

		const UEnum* Enum = StaticEnum<ECtcAnalyticsEditorSource>();
		for (int32 i = 0; i < Enum->NumEnums() - 1; i++)
		{
			ECtcAnalyticsEditorSource EnumIterator = static_cast<ECtcAnalyticsEditorSource>(i);
			Result.Add(MakeShared<ECtcAnalyticsEditorSource>(EnumIterator));
		}
		
		return Result;
	}();

	SourceWidgetSwitcher = SNew(SWidgetSwitcher);

	SourceWidgetSwitcher->AddSlot(GetEditorSourceAsIndex(ECtcAnalyticsEditorSource::FromApi))
	// clang-format off
	[
		SNew(STextBlock)
		.Text(INVTEXT("From API"))
	];
	// clang-format on

	SourceWidgetSwitcher->AddSlot(GetEditorSourceAsIndex(ECtcAnalyticsEditorSource::FromFile))
	// clang-format off
	[
		SNew(SFilePathPicker)
		.BrowseButtonImage(FAppStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
		.BrowseButtonStyle(FAppStyle::Get(), "HoverHintOnly")
		.BrowseButtonToolTip(INVTEXT("Choose a source import file"))
		.BrowseDirectory(this, &SCtcAnalyticsEditorViewerSource::HandleFilePathBrowseDirectory)
		.BrowseTitle(INVTEXT("Source import file picker..."))
		.FilePath(this, &SCtcAnalyticsEditorViewerSource::GetBinaryPathString)
		.FileTypeFilter(TEXT("All files (*.*)|*.*"))
		.OnPathPicked(this, &SCtcAnalyticsEditorViewerSource::OnBinaryPathPicked)
	];
	// clang-format on

	// clang-format off
	ChildSlot
	[
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(SourceComboBox, SComboBox<TSharedPtr<ECtcAnalyticsEditorSource>>)
			.OptionsSource(&AllEnums)
			.InitiallySelectedItem(AllEnums[0])
			.OnGenerateWidget_Lambda([](TSharedPtr<ECtcAnalyticsEditorSource> InItem)
			{
				return SNew(STextBlock)
				.Text(GetEditorSourceAsText(*InItem));
			})
			.OnSelectionChanged_Lambda([this](const TSharedPtr<ECtcAnalyticsEditorSource>& InItem, ESelectInfo::Type)
			{
				SourceWidgetSwitcher->SetActiveWidgetIndex(GetEditorSourceAsIndex(*InItem));
			})
			[
				SNew(STextBlock)
				.Text_Lambda([this]()
				{
					return GetEditorSourceAsText(*SourceComboBox->GetSelectedItem());
				})
			]
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SourceWidgetSwitcher.ToSharedRef()
		]
	];
	// clang-format on
}

FString SCtcAnalyticsEditorViewerSource::GetBinaryPathString() const
{
	return FilePath;
}

void SCtcAnalyticsEditorViewerSource::OnBinaryPathPicked( const FString& PickedPath )
{
	FilePath = FPaths::ConvertRelativePathToFull(PickedPath);;
}

FString SCtcAnalyticsEditorViewerSource::HandleFilePathBrowseDirectory() const
{
	// There is a high chance there are most from where this came from
	if (!FilePath.IsEmpty())
	{
		return FPaths::GetPath(FilePath);
	}

	FString DefaultDirectory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("CastToCloud") / TEXT("Analytics"));
	if (FPaths::DirectoryExists(DefaultDirectory))
	{
		// The user might know not know where these are stored, so let's show him the path initially
		return DefaultDirectory;
	}

	//Fallback to last opened folder
	return FEditorDirectories::Get().GetLastDirectory(ELastDirectory::GENERIC_OPEN);
}
