// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Widgets/SCompoundWidget.h>

class SCtcAnalyticsEditorViewer : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCtcAnalyticsEditorViewer){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	enum EDataSource
	{
		FromApi = 0,
		FromFile
	};

	
};