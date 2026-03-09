// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcMetricsBackendOutputDevice.h"

#include <Dom/JsonObject.h>
#include <HAL/FileManager.h>
#include <HAL/PlatformTLS.h>
#include <Math/UnrealMathUtility.h>
#include <Misc/App.h>
#include <Misc/CommandLine.h>
#include <Misc/CoreGlobals.h>
#include <Misc/FileHelper.h>
#include <Misc/Guid.h>
#include <Misc/OutputDeviceRedirector.h>
#include <Misc/Paths.h>
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
	const TCHAR* LogForwardingOverride = TEXT("LogForwardingAnyConfiguration");
	const TCHAR* LogForwardingOutputPathOverride = TEXT("LogForwardingOutputPath=");
	const TCHAR* DefaultLogForwardingOutputFile = TEXT("CastToCloud/ForwardedLogs.jsonl");
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

	FlushIntervalSeconds = FMath::Max(Settings->LogForwardingInterval, 0.1f);
	BatchSize = FMath::Max(Settings->LogForwardingBatchSize, 1);
	MaxBufferedEntries = FMath::Max(Settings->LogForwardingMaxBufferedEntries, BatchSize);
	MaxMessageLength = FMath::Max(Settings->LogForwardingMaxMessageLength, 128);
	OutputFilePath = FPaths::Combine(FPaths::ProjectSavedDir(), DefaultLogForwardingOutputFile);

	FString OutputPathOverride;
	if (FParse::Value(FCommandLine::Get(), LogForwardingOutputPathOverride, OutputPathOverride) && !OutputPathOverride.IsEmpty())
	{
		OutputFilePath = OutputPathOverride;
	}

	if (FPaths::IsRelative(OutputFilePath))
	{
		OutputFilePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), OutputFilePath);
	}

	FPaths::NormalizeFilename(OutputFilePath);
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

	const FString OutputDirectory = FPaths::GetPath(OutputFilePath);
	if (!OutputDirectory.IsEmpty() && !IFileManager::Get().MakeDirectory(*OutputDirectory, true))
	{
		UE_LOG(LogCtcMetrics, Warning, TEXT("Log forwarding output is enabled but output directory could not be created: %s. The output device will stay disabled."), *OutputDirectory);
		bEnabled = false;
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

	UE_LOG(LogCtcMetrics, Display, TEXT("Log forwarding output enabled. OutputFile=%s BatchSize=%d FlushIntervalSeconds=%s"), *OutputFilePath, BatchSize, *LexToString(FlushIntervalSeconds));

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
			UE_LOG(LogCtcMetrics, Warning, TEXT("Log forwarding buffer overflowed. Oldest entries will be dropped until file writes catch up."));
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
	if (!bIsRegistered || PendingEntries.IsEmpty())
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
		if ((!bBlocking && !bIsRegistered) || PendingEntries.IsEmpty())
		{
			return;
		}

		const int32 EntryCount = bBlocking ? PendingEntries.Num() : FMath::Min(PendingEntries.Num(), BatchSize);
		BatchEntries.Append(PendingEntries.GetData(), EntryCount);
		PendingEntries.RemoveAt(0, EntryCount, EAllowShrinking::No);
		SecondsSinceLastFlush = 0.0;
	}

	const FString BatchId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);
	if (!AppendBatchToOutputFile(BatchId, BatchEntries))
	{
		RequeueEntries(MoveTemp(BatchEntries));
		return;
	}

	UE_LOG(LogCtcMetrics, VeryVerbose, TEXT("[%s] Appended %d log entries to %s."), *BatchId, BatchEntries.Num(), *OutputFilePath);
}

bool FCtcMetricsBackendOutputDevice::AppendBatchToOutputFile(const FString& BatchId, const TArray<FCtcMetricsCapturedLogEntry>& Entries)
{
	FString BatchPayload = BuildBatchPayload(BatchId, Entries);
	BatchPayload += LINE_TERMINATOR;

	constexpr uint32 WriteFlags = FILEWRITE_Append;
	bool bWriteSucceeded = false;
	{
		FScopeLock Lock(&OutputFileCriticalSection);
		bWriteSucceeded = FFileHelper::SaveStringToFile(BatchPayload, *OutputFilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), WriteFlags);
	}

	if (!bWriteSucceeded)
	{
		UE_LOG(LogCtcMetrics, Error, TEXT("[%s] Failed to append %d log entries to %s."), *BatchId, Entries.Num(), *OutputFilePath);
		return false;
	}

	return true;
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

FString FCtcMetricsBackendOutputDevice::BuildBatchPayload(const FString& BatchId, const TArray<FCtcMetricsCapturedLogEntry>& Entries) const
{
	TSharedRef<FJsonObject> BatchPayload = MakeShared<FJsonObject>();
	BatchPayload->SetStringField(TEXT("batchId"), BatchId);
	BatchPayload->SetStringField(TEXT("sessionId"), FApp::GetSessionId().ToString());
	BatchPayload->SetStringField(TEXT("projectName"), FApp::GetProjectName());
	BatchPayload->SetStringField(TEXT("capturedAtUtc"), FDateTime::UtcNow().ToIso8601());
	BatchPayload->SetStringField(TEXT("buildConfiguration"), LexToString(FApp::GetBuildConfiguration()));
	BatchPayload->SetStringField(TEXT("buildTarget"), LexToString(FApp::GetBuildTargetType()));
	BatchPayload->SetStringField(TEXT("world"), CastToCloudSharedHelpers::GetWorldPackage().Get(TEXT("")));
	BatchPayload->SetStringField(TEXT("source"), TEXT("unreal"));

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

	BatchPayload->SetArrayField(TEXT("entries"), EntryValues);

	FString BatchPayloadString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BatchPayloadString);
	ensure(FJsonSerializer::Serialize(BatchPayload, Writer));
	return BatchPayloadString;
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
