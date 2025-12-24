// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcConfigurationSettingsCustomization.h"

#include <DetailWidgetRow.h>
#include <IDetailChildrenBuilder.h>
#include <Widgets/Input/SCheckBox.h>
#include <Widgets/Layout/SGridPanel.h>
#include <Widgets/Text/STextBlock.h>

#include "CtcConfigurationSettings.h"

FCtcConfigurationSettings* GetConfigurationSettingsFromPropertyHandle(TSharedPtr<IPropertyHandle> PropertyHandle)
{
	TArray<void*> RawData;
	PropertyHandle->AccessRawData(RawData);
	FCtcConfigurationSettings* ConfigurationSettings = static_cast<FCtcConfigurationSettings*>(RawData[0]);
	return ConfigurationSettings;
}

TSharedRef<IPropertyTypeCustomization> FCtcConfigurationSettingsCustomization::MakeInstance()
{
	return MakeShared<FCtcConfigurationSettingsCustomization>();
}

void FCtcConfigurationSettingsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

void FCtcConfigurationSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// clang-format off
	TArray<EBuildConfiguration> BuildConfigurations = {
		EBuildConfiguration::Debug,
		EBuildConfiguration::DebugGame,
		EBuildConfiguration::Development,
		EBuildConfiguration::Test,
		EBuildConfiguration::Shipping
	};
	// clang-format on

	// clang-format off
	const TArray<EBuildTargetType> BuildTargets = {
		EBuildTargetType::Editor,
		EBuildTargetType::Game,
		EBuildTargetType::Client,
		EBuildTargetType::Server,
		EBuildTargetType::Program
	};
	// clang-format on

	TSharedRef<SGridPanel> OptionsGrid = SNew(SGridPanel);

	for (const EBuildConfiguration& BuildConfiguration : BuildConfigurations)
	{
		const int j = static_cast<int>(BuildConfiguration) + 1;
		// clang-format off
		OptionsGrid->AddSlot(0, j)
		           .VAlign(VAlign_Center)
		           .Padding(2.0f)
		[
			SNew(STextBlock).Text(FText::FromString(LexToString(BuildConfiguration)))
		];
		// clang-format on
	}


	for (const EBuildTargetType& BuildTarget : BuildTargets)
	{
		const int i = static_cast<int>(BuildTarget) + 1;
		// clang-format off
		OptionsGrid->AddSlot(i, 0)
		           .VAlign(VAlign_Center)
		           .Padding(2.0f)

		[
			SNew(STextBlock).Text(FText::FromString(LexToString(BuildTarget)))
		];
		// clang-format on
	}

	for (const EBuildConfiguration& BuildConfiguration : BuildConfigurations)
	{
		for (const EBuildTargetType& BuildTarget : BuildTargets)
		{
			const int i = static_cast<int>(BuildTarget) + 1;
			const int j = static_cast<int>(BuildConfiguration) + 1;

			// clang-format off
			OptionsGrid->AddSlot(i, j)
			           .VAlign(VAlign_Center)
			           .Padding(2.0f)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([PropertyHandle, BuildConfiguration, BuildTarget]()
				{
					const FCtcConfigurationSettings* ConfigurationSettings = GetConfigurationSettingsFromPropertyHandle(PropertyHandle);
					return ConfigurationSettings->HasFlavour(BuildConfiguration, BuildTarget)
						       ? ECheckBoxState::Checked
						       : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([PropertyHandle, BuildConfiguration, BuildTarget](ECheckBoxState InState)
				{
					FCtcConfigurationSettings* ConfigurationSettings = GetConfigurationSettingsFromPropertyHandle(
						PropertyHandle);

					PropertyHandle->NotifyPreChange();
					if (InState == ECheckBoxState::Checked)
					{
						ConfigurationSettings->AddFlavor(BuildConfiguration, BuildTarget);
					}
					else if (InState == ECheckBoxState::Unchecked)
					{
						ConfigurationSettings->RemoveFlavor(BuildConfiguration, BuildTarget);
					}

					PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
				})];
			// clang-format on
		}
	}

	PropertyHandle->MarkHiddenByCustomization();

	// clang-format off
	ChildBuilder.AddCustomRow(PropertyHandle->GetPropertyDisplayName())
	            .NameContent()
		[
			PropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()[
			OptionsGrid
		];
	// clang-format on
}
