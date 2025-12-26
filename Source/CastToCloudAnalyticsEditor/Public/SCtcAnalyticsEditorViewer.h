// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Widgets/SCompoundWidget.h>

class SCtcAnalyticsEditorViewerSource;

class SCtcAnalyticsEditorViewer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCtcAnalyticsEditorViewer){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	TSharedPtr<SCtcAnalyticsEditorViewerSource> ViewerSource;
};