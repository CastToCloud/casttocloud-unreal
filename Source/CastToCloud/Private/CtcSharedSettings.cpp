// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcSharedSettings.h"

#include <HAL/FileManager.h>
#include <ISettingsContainer.h>
#include <ISettingsModule.h>
#include <Misc/ConfigContext.h>
#include <Misc/MessageDialog.h>
#include <Modules/ModuleManager.h>

#if WITH_EDITOR
#include <SourceControlHelpers.h>
#endif

namespace
{
	const FName CastToCloudProviderName = TEXT("CastToCloudAnalytics");

#if WITH_EDITOR
	bool VerifyConfigIsWritable(const FString& ConfigPath)
	{
		bool bWriteAccess = !IFileManager::Get().IsReadOnly(*ConfigPath);

		// NOTE: Regardless of writable access, if source control integration is enabled, we try to check out.
		if (USourceControlHelpers::IsEnabled())
		{
			bWriteAccess |= USourceControlHelpers::CheckOutFile(ConfigPath);
		}

		return bWriteAccess;
	}
#endif
} // namespace

void UCtcSharedSettings::SetupAnalyticsProvider()
{
#if WITH_EDITOR
	const FString DefaultEngineIni = FPaths::ConvertRelativePathToFull(FPaths::ProjectConfigDir() / TEXT("DefaultEngine.ini"));

	if (!VerifyConfigIsWritable(DefaultEngineIni))
	{
		const FText FailureTitle = INVTEXT("Failed");
		const FText FailureMessage = FText::Format(INVTEXT("Failed to set Provider to {0}. File is not writeable"), FText::FromName(CastToCloudProviderName));
		FMessageDialog::Open(EAppMsgCategory::Error, EAppMsgType::Ok, FailureMessage, FailureTitle);
		return;
	}

	FConfigCacheIni Config(EConfigCacheType::Temporary);
	FConfigFile& NewFile = Config.Add(DefaultEngineIni, FConfigFile());
	NewFile.SetString(TEXT("Analytics"), TEXT("ProviderModuleName"), *CastToCloudProviderName.ToString());
	NewFile.UpdateSections(*DefaultEngineIni, *GEngineIni);

	FConfigContext::ForceReloadIntoGConfig().Load(*GEngineIni);

	const FText SuccessTitle = INVTEXT("Success");
	const FText SuccessMessage = FText::Format(INVTEXT("Provider set to {0}."), FText::FromName(CastToCloudProviderName));
	FMessageDialog::Open(EAppMsgCategory::Success, EAppMsgType::Ok, SuccessMessage, SuccessTitle);
#endif
}

bool UCtcSharedSettings::NeedsToSetAnalyticsProvider() const
{
	FString ProviderModuleName = TEXT("No Provider");
	GConfig->GetString(TEXT("Analytics"), TEXT("ProviderModuleName"), ProviderModuleName, GEngineIni);

	return ProviderModuleName != CastToCloudProviderName;
}

#if WITH_EDITOR
void UCtcSharedSettings::ShowSettings()
{
	ISettingsModule& SettingsModule = FModuleManager::LoadModuleChecked<ISettingsModule>("Settings");
	SettingsModule.ShowViewer(GetContainerName(), GetCategoryName(), GetSectionName());
}
#endif

#if WITH_EDITOR
void UCtcSharedSettings::SaveToDefaultConfig()
{
	if (!VerifyConfigIsWritable(GetDefaultConfigFilename()))
	{
		const FText FailureTitle = INVTEXT("Failed");
		const FText FailureMessage = INVTEXT("Failed to save default config. File is not writeable");
		FMessageDialog::Open(EAppMsgCategory::Error, EAppMsgType::Ok, FailureMessage, FailureTitle);
		return;
	}

	TryUpdateDefaultConfigFile();
}
#endif

void UCtcSharedSettings::PostInitProperties()
{
	Super::PostInitProperties();

#if WITH_EDITOR
	ISettingsModule& SettingsModule = FModuleManager::GetModuleChecked<ISettingsModule>("Settings");
	if (TSharedPtr<ISettingsContainer> SettingsContainer = SettingsModule.GetContainer(GetContainerName()))
	{
		SettingsContainer->SetCategorySortPriority(TEXT("Project"), -0.7f);
		SettingsContainer->SetCategorySortPriority(TEXT("Game"), -0.6f);
		SettingsContainer->SetCategorySortPriority(GetCategoryName(), -0.5f);
	}
#endif
}
