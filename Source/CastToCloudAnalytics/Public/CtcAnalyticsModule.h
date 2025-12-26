// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Interfaces/IAnalyticsProviderModule.h>

class FCtcAnalyticsProvider;

class FCtcAnalyticsModule final : public IAnalyticsProviderModule
{
public:
	static FCtcAnalyticsModule& Get();

	TSharedPtr<FCtcAnalyticsProvider> GetProvider() const { return AnalyticsProvider; }

private:
	// ~Begin IAnalyticsProviderModule interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual TSharedPtr<IAnalyticsProvider> CreateAnalyticsProvider(const FAnalyticsProviderConfigurationDelegate& GetConfigValue) const override;
	// ~End IAnalyticsProviderModule interface

	TSharedPtr<FCtcAnalyticsProvider> AnalyticsProvider;
};
