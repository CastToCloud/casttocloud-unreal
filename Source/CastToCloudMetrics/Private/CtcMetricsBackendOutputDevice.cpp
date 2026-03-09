// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcMetricsBackendOutputDevice.h"

#include <Dom/JsonObject.h>
#include <HAL/PlatformTLS.h>
#include <HttpModule.h>
#include <Interfaces/IHttpResponse.h>
#include <Math/UnrealMathUtility.h>
#include <Misc/App.h>
#include <Misc/CommandLine.h>
#include <Misc/CoreGlobals.h>
#include <Misc/Guid.h>
#include <Misc/OutputDeviceRedirector.h>
#include <Misc/ScopeLock.h>
#include <Serialization/JsonSerializer.h>
#include <Serialization/JsonWriter.h>

#include "CtcMetricsLog.h"
#include "CtcSharedHelpers.h"
#include "CtcSharedSettings.h"

namespace
{
	const FName MetricsCategoryName(TEXT("LogCtcMetrics"));
	const FName HttpCategoryName(TEXT("LogHttp"));
	const FName HttpRetryCategoryName(TEXT("LogHttpRetrySystem"));
	const FName HttpResponseCategoryName(TEXT("LogHttpResponse"));
	const TCHAR* LogForwardingEndpoint = TEXT("metrics/logs/record");
	const TCHAR* LogForwardingOverride = TEXT("LogForwardingAnyConfiguration");
}

FCtcMetricsBackendOutputDevice::FCtcMetricsBackendOutputDevice()
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (!Settings)
	{
		return;
	}

	const bool bAllowAnyConfiguration = FParse::Param(FCommandLine::Get(), LogForwardingOverride);
	bEnabled = Settings->LogForwardingEnabled.IsCurrentConfigurationAllowed() || bAllowAnyConfiguration;
	if (!bEnabled)
	{
		return;
	}

	EndpointUrl = Settings->ApiUrl / LogForwardingEndpoint;
	ApiKey = Settings->RuntimeApiKey;
	FlushIntervalSeconds = FMath::Max(Settings->LogForwardingInterval, 0.1f);
	BatchSize = FMath::Max(Settings->LogForwardingBatchSize, 1);
	MaxBufferedEntries = FMath::Max(Settings->LogForwardingMaxBufferedEntries, BatchSize);
	MaxMessageLength = FMath::Max(Settings->LogForwardingMaxMessageLength, 128);

	if (EndpointUrl.IsEmpty() || ApiKey.IsEmpty())
	{
		UE_LOG(LogCtcMetrics, Warning, TEXT("Log forwarding is enabled but missing ApiUrl or RuntimeApiKey. The output device will stay disabled."));
		bEnabled = false;
	}
}

FCtcMetricsBackendOutputDevice::~FCtcMetricsBackendOutputDevice()
{
	TearDown();
}

bool FCtcMetricsBackendOutputDevice::Setup()
{
	if (!bEnabled || GLog == nullptr)
	{
		return false;
	}

	{
		FScopeLock Lock(&CriticalSection);
		if (bIsRegistered)
		{
			return true;
		}

		bIsRegistered = true;
	}

	GLog->AddOutputDevice(this);
#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 7)
	GLog->SerializeBacklog(this);
#endif

	UE_LOG(LogCtcMetrics, Display, TEXT("Backend log forwarding enabled. Endpoint=%s BatchSize=%d FlushIntervalSeconds=%s"), *EndpointUrl, BatchSize, *LexToString(FlushIntervalSeconds));

	return true;
}

void FCtcMetricsBackendOutputDevice::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category)
{
	Serialize(V, Verbosity, Category, -1.0);
}

void FCtcMetricsBackendOutputDevice::Serialize(const TCHAR* V, ELogVerbosity::Type Verbosity, const FName& Category, double Time)
{
	if (!bEnabled || !ShouldCapture(Verbosity, Category))
	{
		return;
	}

	FString Message = V ? V : TEXT("");
	if (Message.IsEmpty())
	{
		return;
	}

	if (Message.Len() > MaxMessageLength)
	{
		Message.LeftInline(MaxMessageLength, EAllowShrinking::No);
	}

	FCtcMetricsCapturedLogEntry Entry;
	Entry.Message = MoveTemp(Message);
	Entry.Category = Category.ToString();
	Entry.Verbosity = GetVerbosityString(Verbosity);
	Entry.TimestampUtc = FDateTime::UtcNow().ToIso8601();
	Entry.ThreadId = FPlatformTLS::GetCurrentThreadId();
	Entry.TimeSeconds = Time;

	FScopeLock Lock(&CriticalSection);
	if (!bIsRegistered)
	{
		return;
	}

	PendingEntries.Add(MoveTemp(Entry));

	const int32 OverflowCount = PendingEntries.Num() - MaxBufferedEntries;
	if (OverflowCount > 0)
	{
		PendingEntries.RemoveAt(0, OverflowCount, EAllowShrinking::No);
		if (!bHasLoggedDropWarning)
		{
			bHasLoggedDropWarning = true;
			UE_LOG(LogCtcMetrics, Warning, TEXT("Log forwarding buffer overflowed. Oldest entries will be dropped until the backend catches up."));
		}
	}
}

void FCtcMetricsBackendOutputDevice::Flush()
{
	FlushPendingLogs(false);
}

void FCtcMetricsBackendOutputDevice::TearDown()
{
	bool bShouldRemove = false;
	{
		FScopeLock Lock(&CriticalSection);
		if (!bIsRegistered)
		{
			return;
		}

		bIsRegistered = false;
		bShouldRemove = true;
	}

	if (bShouldRemove && GLog != nullptr)
	{
		GLog->RemoveOutputDevice(this);
	}

	CancelInFlightRequest();
	FlushPendingLogs(true);
}

bool FCtcMetricsBackendOutputDevice::Tick(float DeltaTime)
{
	if (!bEnabled)
	{
		return true;
	}

	SecondsSinceLastFlush += DeltaTime;
	if (ShouldFlushNow())
	{
		FlushPendingLogs(false);
	}

	return true;
}

bool FCtcMetricsBackendOutputDevice::ShouldCapture(ELogVerbosity::Type Verbosity, const FName& Category) const
{
	const ELogVerbosity::Type MaskedVerbosity = static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask);
	if (MaskedVerbosity == ELogVerbosity::SetColor)
	{
		return false;
	}

	return Category != MetricsCategoryName && Category != HttpCategoryName && Category != HttpRetryCategoryName && Category != HttpResponseCategoryName;
}

bool FCtcMetricsBackendOutputDevice::ShouldFlushNow() const
{
	FScopeLock Lock(&CriticalSection);
	if (!bIsRegistered || InFlightRequest.IsValid() || PendingEntries.IsEmpty())
	{
		return false;
	}

	return PendingEntries.Num() >= BatchSize || SecondsSinceLastFlush >= FlushIntervalSeconds;
}

void FCtcMetricsBackendOutputDevice::FlushPendingLogs(bool bBlocking)
{
	if (!bEnabled)
	{
		return;
	}

	TArray<FCtcMetricsCapturedLogEntry> BatchEntries;
	{
		FScopeLock Lock(&CriticalSection);
		if ((!bBlocking && !bIsRegistered) || InFlightRequest.IsValid() || PendingEntries.IsEmpty())
		{
			return;
		}

		const int32 EntryCount = bBlocking ? PendingEntries.Num() : FMath::Min(PendingEntries.Num(), BatchSize);
		BatchEntries.Append(PendingEntries.GetData(), EntryCount);
		PendingEntries.RemoveAt(0, EntryCount, EAllowShrinking::No);
		SecondsSinceLastFlush = 0.0;
	}

	const FString BatchId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	const FString RequestBody = BuildRequestBody(BatchId, BatchEntries);

	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(TEXT("POST"));
	Request->SetURL(EndpointUrl);
	Request->SetHeader(TEXT("X-API-Key"), ApiKey);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetTimeout(RequestTimeoutSeconds);
	Request->SetActivityTimeout(RequestTimeoutSeconds);
	Request->SetContentAsString(RequestBody);

	if (bBlocking)
	{
		Request->ProcessRequestUntilComplete();
		const bool bSuccess = Request->GetStatus() == EHttpRequestStatus::Succeeded;
		OnFlushResponse(Request, Request->GetResponse(), bSuccess, BatchId, MoveTemp(BatchEntries));
		return;
	}

	{
		FScopeLock Lock(&CriticalSection);
		InFlightEntries = BatchEntries;
		InFlightRequest = Request;
	}

	Request->OnProcessRequestComplete().BindRaw(this, &FCtcMetricsBackendOutputDevice::OnFlushResponse, BatchId, BatchEntries);
	Request->ProcessRequest();
}

void FCtcMetricsBackendOutputDevice::OnFlushResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess, FString BatchId, TArray<FCtcMetricsCapturedLogEntry> Entries)
{
	{
		FScopeLock Lock(&CriticalSection);
		if (Request == InFlightRequest)
		{
			InFlightRequest.Reset();
			InFlightEntries.Reset();
		}
	}

	if (!bSuccess || !Response.IsValid())
	{
		UE_LOG(LogCtcMetrics, Error, TEXT("[%s] Log forwarding request failed before receiving a valid response. Entries=%d"), *BatchId, Entries.Num());
		RequeueEntries(MoveTemp(Entries));
		return;
	}

	if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		UE_LOG(LogCtcMetrics, Error, TEXT("[%s] Log forwarding request failed. ResponseCode=%d Body=%s"), *BatchId, Response->GetResponseCode(), *Response->GetContentAsString());
		RequeueEntries(MoveTemp(Entries));
		return;
	}

	UE_LOG(LogCtcMetrics, VeryVerbose, TEXT("[%s] Forwarded %d log entries to the backend."), *BatchId, Entries.Num());
}

void FCtcMetricsBackendOutputDevice::CancelInFlightRequest()
{
	FHttpRequestPtr RequestToCancel;
	TArray<FCtcMetricsCapturedLogEntry> EntriesToRequeue;

	{
		FScopeLock Lock(&CriticalSection);
		RequestToCancel = InFlightRequest;
		EntriesToRequeue = MoveTemp(InFlightEntries);
		InFlightEntries.Reset();
		InFlightRequest.Reset();
	}

	if (RequestToCancel.IsValid())
	{
		RequestToCancel->OnProcessRequestComplete().Unbind();
		RequestToCancel->CancelRequest();
	}

	RequeueEntries(MoveTemp(EntriesToRequeue));
}

void FCtcMetricsBackendOutputDevice::RequeueEntries(TArray<FCtcMetricsCapturedLogEntry>&& Entries)
{
	if (Entries.IsEmpty())
	{
		return;
	}

	FScopeLock Lock(&CriticalSection);

	TArray<FCtcMetricsCapturedLogEntry> Combined;
	Combined.Reserve(Entries.Num() + PendingEntries.Num());
	Combined.Append(Entries);
	Combined.Append(PendingEntries);
	PendingEntries = MoveTemp(Combined);

	const int32 OverflowCount = PendingEntries.Num() - MaxBufferedEntries;
	if (OverflowCount > 0)
	{
		PendingEntries.RemoveAt(0, OverflowCount, EAllowShrinking::No);
	}
}

FString FCtcMetricsBackendOutputDevice::BuildRequestBody(const FString& BatchId, const TArray<FCtcMetricsCapturedLogEntry>& Entries) const
{
	TSharedRef<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetStringField(TEXT("batchId"), BatchId);
	RequestBody->SetStringField(TEXT("sessionId"), FApp::GetSessionId().ToString());
	RequestBody->SetStringField(TEXT("projectName"), FApp::GetProjectName());
	RequestBody->SetStringField(TEXT("capturedAtUtc"), FDateTime::UtcNow().ToIso8601());
	RequestBody->SetStringField(TEXT("buildConfiguration"), LexToString(FApp::GetBuildConfiguration()));
	RequestBody->SetStringField(TEXT("buildTarget"), LexToString(FApp::GetBuildTargetType()));
	RequestBody->SetStringField(TEXT("world"), CastToCloudSharedHelpers::GetWorldPackage().Get(TEXT("")));
	RequestBody->SetStringField(TEXT("source"), TEXT("unreal"));

	TArray<TSharedPtr<FJsonValue>> EntryValues;
	EntryValues.Reserve(Entries.Num());
	for (const FCtcMetricsCapturedLogEntry& Entry : Entries)
	{
		TSharedRef<FJsonObject> EntryObject = MakeShared<FJsonObject>();
		EntryObject->SetStringField(TEXT("message"), Entry.Message);
		EntryObject->SetStringField(TEXT("category"), Entry.Category);
		EntryObject->SetStringField(TEXT("verbosity"), Entry.Verbosity);
		EntryObject->SetStringField(TEXT("timestampUtc"), Entry.TimestampUtc);
		EntryObject->SetNumberField(TEXT("threadId"), static_cast<double>(Entry.ThreadId));
		if (Entry.TimeSeconds >= 0.0)
		{
			EntryObject->SetNumberField(TEXT("timeSeconds"), Entry.TimeSeconds);
		}

		EntryValues.Add(MakeShared<FJsonValueObject>(EntryObject));
	}

	RequestBody->SetArrayField(TEXT("entries"), EntryValues);

	FString RequestBodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	ensure(FJsonSerializer::Serialize(RequestBody, Writer));
	return RequestBodyString;
}

FString FCtcMetricsBackendOutputDevice::GetVerbosityString(ELogVerbosity::Type Verbosity)
{
	switch (static_cast<ELogVerbosity::Type>(Verbosity & ELogVerbosity::VerbosityMask))
	{
	case ELogVerbosity::Fatal:
		return TEXT("Fatal");
	case ELogVerbosity::Error:
		return TEXT("Error");
	case ELogVerbosity::Warning:
		return TEXT("Warning");
	case ELogVerbosity::Display:
		return TEXT("Display");
	case ELogVerbosity::Log:
		return TEXT("Log");
	case ELogVerbosity::Verbose:
		return TEXT("Verbose");
	case ELogVerbosity::VeryVerbose:
		return TEXT("VeryVerbose");
	default:
		return TEXT("Unknown");
	}
}
