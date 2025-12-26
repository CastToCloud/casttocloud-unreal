// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAnalyticsModule.h"

#include "CtcAnalyticsProvider.h"

void FCtcAnalyticsModule::StartupModule()
{
	AnalyticsProvider = MakeShared<FCtcAnalyticsProvider>();
}

void FCtcAnalyticsModule::ShutdownModule()
{
	AnalyticsProvider.Reset();
}

TSharedPtr<IAnalyticsProvider> FCtcAnalyticsModule::CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const
{
	return AnalyticsProvider;
}

IMPLEMENT_MODULE(FCtcAnalyticsModule, CastToCloudAnalytics);
