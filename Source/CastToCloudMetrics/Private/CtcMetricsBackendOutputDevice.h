// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Containers/Ticker.h>
#include <HAL/CriticalSection.h>
#include <Interfaces/IHttpRequest.h>
#include <Misc/OutputDevice.h>

struct FCtcMetricsCapturedLogEntry
{
	FString Message;
	FString Category;
	FString Verbosity;
	FString TimestampUtc;
	uint32 ThreadId = 0;
	double TimeSeconds = -1.0;
};

class FCtcMetricsBackendOutputDevice final : public FOutputDevice, public FTSTickerObjectBase
{
public:
	FCtcMetricsBackendOutputDevice();
	virtual ~FCtcMetricsBackendOutputDevice() override;

	bool Setup();

	// ~Begin FOutputDevice interface
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category) override;
	virtual void Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, double Time) override;
	virtual void Flush() override;
	virtual void TearDown() override;
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	// ~End FOutputDevice interface

	// ~Begin FTSTickerObjectBase interface
	virtual bool Tick(float DeltaTime) override;
	// ~End FTSTickerObjectBase interface

private:
	bool ShouldCapture(ELogVerbosity::Type Verbosity, const FName& Category) const;
	bool ShouldFlushNow() const;
	void FlushPendingLogs(bool bBlocking);
	void OnFlushResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, FString BatchId, TArray<FCtcMetricsCapturedLogEntry> Entries);
	void CancelInFlightRequest();
	void RequeueEntries(TArray<FCtcMetricsCapturedLogEntry>&& Entries);
	FString BuildRequestBody(const FString& BatchId, const TArray<FCtcMetricsCapturedLogEntry>& Entries) const;
	static FString GetVerbosityString(ELogVerbosity::Type Verbosity);

	mutable FCriticalSection CriticalSection;
	TArray<FCtcMetricsCapturedLogEntry> PendingEntries;
	TArray<FCtcMetricsCapturedLogEntry> InFlightEntries;
	FHttpRequestPtr InFlightRequest;

	FString EndpointUrl;
	FString ApiKey;
	float FlushIntervalSeconds = 2.0f;
	float RequestTimeoutSeconds = 5.0f;
	int32 BatchSize = 50;
	int32 MaxBufferedEntries = 1000;
	int32 MaxMessageLength = 2048;
	double SecondsSinceLastFlush = 0.0;
	bool bEnabled = false;
	bool bIsRegistered = false;
	bool bHasLoggedDropWarning = false;
};
