// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "SCtcAnalyticsEditorViewer.h"

#include "SCtcAnalyticsEditorViewerSource.h"

void SCtcAnalyticsEditorViewer::Construct(const FArguments& InArgs)
{
	// clang-format off
	ChildSlot
	[
		SAssignNew(ViewerSource, SCtcAnalyticsEditorViewerSource)
	];
	// clang-format on
}
