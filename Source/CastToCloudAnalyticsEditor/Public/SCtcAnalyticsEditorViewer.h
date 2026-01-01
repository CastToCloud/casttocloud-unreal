// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Widgets/SCompoundWidget.h>

#include "CtcAnalyticsEditorViewerSettings.h"

class IDetailsView;

class SCtcAnalyticsEditorViewer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCtcAnalyticsEditorViewer) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual ~SCtcAnalyticsEditorViewer() override;

private:
	FText GetSourceStatusText() const;

	TStrongObjectPtr<UCtcAnalyticsEditorViewerSettings> DrawSettings;
};
