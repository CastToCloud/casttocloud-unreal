// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Interfaces/IAnalyticsProviderModule.h>

class IAnalyticsProvider;

class FCtcAnalytics final : public IAnalyticsProviderModule
{
	// ~Begin IAnalyticsProviderModule interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual TSharedPtr<IAnalyticsProvider> CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const override;
	// ~End IAnalyticsProviderModule interface

	TSharedPtr<IAnalyticsProvider> AnalyticsProvider;
};
