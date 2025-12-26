// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Widgets/SCompoundWidget.h>

UENUM()
enum class ECtcAnalyticsEditorSource : uint8
{
	FromApi,
	FromFile,
};

class SCtcAnalyticsEditorViewerSource : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCtcAnalyticsEditorViewerSource) { }
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	FString GetBinaryPathString() const;
	void OnBinaryPathPicked(const FString& PickedPath);
	FString HandleFilePathBrowseDirectory() const;

private:
	TSharedPtr<SComboBox<TSharedPtr<ECtcAnalyticsEditorSource>>> SourceComboBox;

	TSharedPtr<SWidgetSwitcher> SourceWidgetSwitcher;
	
	FString FilePath;
};
