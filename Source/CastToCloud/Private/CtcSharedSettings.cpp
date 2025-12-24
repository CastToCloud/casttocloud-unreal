// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcSharedSettings.h"

#include <ISettingsContainer.h>
#include <ISettingsModule.h>
#include <Misc/ConfigContext.h>
#include <Misc/MessageDialog.h>
#include <Modules/ModuleManager.h>

namespace
{
	const FName CastToCloudProviderName = TEXT("CastToCloudAnalytics");
}

void UCtcSharedSettings::SetupAnalyticsProvider()
{
	const FString DefaultEngineIni = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / TEXT("DefaultEngine.ini"));
	FConfigCacheIni Config(EConfigCacheType::Temporary);
	FConfigFile& NewFile = Config.Add(DefaultEngineIni, FConfigFile());
	NewFile.SetString(TEXT("Analytics"), TEXT("ProviderModuleName"), *CastToCloudProviderName.ToString());
	NewFile.UpdateSections(*DefaultEngineIni, *GEngineIni);

	FConfigContext::ForceReloadIntoGConfig().Load(*GEngineIni);

	const FText SuccessTitle = INVTEXT("Success");
	const FText SuccessMessage = FText::Format(INVTEXT("Provider set to {0}."), FText::FromName(CastToCloudProviderName));
	FMessageDialog::Open(EAppMsgCategory::Success, EAppMsgType::Ok, SuccessMessage, SuccessTitle);
}

bool UCtcSharedSettings::NeedsToSetAnalyticsProvider() const
{
	FString ProviderModuleName = TEXT("No Provider");
	GConfig->GetString(TEXT("Analytics"), TEXT("ProviderModuleName"), ProviderModuleName, GEngineIni);

	return ProviderModuleName != CastToCloudProviderName;
}

void UCtcSharedSettings::ShowSettings()
{
	ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
	SettingsModule.ShowViewer(GetContainerName(), GetCategoryName(), GetSectionName());
}

void UCtcSharedSettings::SaveToDefaultConfig()
{
	TryUpdateDefaultConfigFile();
}

void UCtcSharedSettings::PostInitProperties()
{
	Super::PostInitProperties();

	ISettingsModule& SettingsModule = FModuleManager::GetModuleChecked<ISettingsModule>("Settings");
	if (TSharedPtr<ISettingsContainer> SettingsContainer = SettingsModule.GetContainer(GetContainerName()))
	{
		SettingsContainer->SetCategorySortPriority(TEXT("Project"), -0.7f);
		SettingsContainer->SetCategorySortPriority(TEXT("Game"), -0.6f);
		SettingsContainer->SetCategorySortPriority(GetCategoryName(), -0.5f);
	}
}
