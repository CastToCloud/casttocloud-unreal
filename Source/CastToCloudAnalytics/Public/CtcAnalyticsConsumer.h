// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <HttpFwd.h>
#include <HttpRetrySystem.h>
#include <Templates/SharedPointer.h>

class FJsonValue;

struct FCtcAnalyticsConsumer : TSharedFromThis<FCtcAnalyticsConsumer>, FVirtualDestructor
{
	virtual void HandleEvents(const FString& SessionId, const TArray<TSharedPtr<FJsonValue>>& Events, bool bWait) = 0;
};

struct FCtcAnalyticsApiConsumer : FCtcAnalyticsConsumer
{
	FCtcAnalyticsApiConsumer();

	// ~Begin FCtcAnalyticsConsumer interface
	virtual void HandleEvents(const FString& SessionId, const TArray<TSharedPtr<FJsonValue>>& Events, bool bWait) override;
	// ~End FCtcAnalyticsConsumer interface

private:
	/**
	 * Callback executed when the HTTP response for the event request is retrieved
	 */
	void OnEventResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess);

	TSharedPtr<FHttpRetrySystem::FManager> HttpRetryManager;
};

struct FCtcAnalyticsFileConsumer : FCtcAnalyticsConsumer
{
	// ~Begin FCtcAnalyticsConsumer interface
	virtual void HandleEvents(const FString& SessionId, const TArray<TSharedPtr<FJsonValue>>& Events, bool bWait) override;
	// ~End FCtcAnalyticsConsumer interface
};

struct FCtcAnalyticsLogConsumer : FCtcAnalyticsConsumer
{
	// ~Begin FCtcAnalyticsConsumer interface
	virtual void HandleEvents(const FString& SessionId, const TArray<TSharedPtr<FJsonValue>>& Events, bool bWait) override;
	// ~End FCtcAnalyticsConsumer interface
};
