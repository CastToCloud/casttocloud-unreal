// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAnalytics.h"

#include "CtcAnalyticsProvider.h"

void FCtcAnalytics::StartupModule()
{
	AnalyticsProvider = MakeShared<FCtcAnalyticsProvider>();
}

void FCtcAnalytics::ShutdownModule()
{
	AnalyticsProvider.Reset();
}

TSharedPtr<IAnalyticsProvider> FCtcAnalytics::CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const
{
	return AnalyticsProvider;
}

IMPLEMENT_MODULE(FCtcAnalytics, CastToCloudAnalytics);