// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "SCtcAnalyticsEditorViewer.h"

#include <Components/VerticalBox.h>
#include <Modules/ModuleManager.h>
#include <PropertyEditorModule.h>
#include <Widgets/Text/STextBlock.h>

#include "CtcAnalyticsEditorViewerSettings.h"

void SCtcAnalyticsEditorViewer::Construct(const FArguments& InArgs)
{
	DrawSettings = TStrongObjectPtr(NewObject<UCtcAnalyticsEditorViewerSettings>());

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.ColumnWidth = 0.8f;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bShowHeaders = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	TSharedPtr<IDetailsView> DrawSettingsDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	TSharedPtr<IDetailsView> DataSourceDetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	// The main settings object will remain stable so we can set it once.
	DrawSettingsDetailsView->SetObject(DrawSettings.Get());

	// But we need to refresh the DataSource in case the class inside DrawSettings has been changed.
	DrawSettingsDetailsView->OnFinishedChangingProperties().AddLambda(
		[DataSourceDetailsView, this](const FPropertyChangedEvent& PropertyChangedEvent)
		{
			UCtcAnalyticsEditorViewerDataSource* DataSourceInstance = DrawSettings->GetDataSourceInstance();
			DataSourceDetailsView->SetObject(DataSourceInstance);
		}
	);

	// clang-format off
	ChildSlot
	[
		SNew(SVerticalBox)

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			DrawSettingsDetailsView.ToSharedRef()
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			DataSourceDetailsView.ToSharedRef()
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(STextBlock)
			.Text(this, &SCtcAnalyticsEditorViewer::GetSourceStatusText)
		]
	];
	// clang-format on
}

FText SCtcAnalyticsEditorViewer::GetSourceStatusText() const
{
	const UCtcAnalyticsEditorViewerDataSource* SourceInstance = DrawSettings->GetDataSourceInstance();
	TValueOrError<FCtcAnalyticsEditorHeatmapPoints, FString> Result = SourceInstance ? SourceInstance->GetResult() : MakeError(TEXT("Missing Source"));
	if (Result.HasError())
	{
		return FText::FromString(Result.GetError());
	}

	int NumEvents = 0;
	for (const FCtcAnalyticsEditorHeatmapPoint& Point : Result.GetValue().Points)
	{
		NumEvents += Point.Count;
	}

	return FText::Format(INVTEXT("{0} Events"), FText::AsNumber(NumEvents));
}
