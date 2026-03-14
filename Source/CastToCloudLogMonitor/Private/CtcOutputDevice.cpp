#include "CtcOutputDevice.h"

// Upload-based implementation kept below as commented reference.
// #include <HttpModule.h>
// #include <Interfaces/IHttpResponse.h>
#include <GeneralProjectSettings.h>

#include "CtcLogMonitor.h"
#include "CtcLogMonitoringSettings.h"
#include "CtcLogMonitorLog.h"
// #include "CtcSharedSettings.h"

#include <CoreGlobals.h>
#include <HAL/FileManager.h>
#include <HAL/PlatformOutputDevices.h>
#include <HAL/PlatformTime.h>
#include <Misc/OutputDeviceHelper.h>
#include <Misc/Paths.h>
#include <Misc/ScopeLock.h>
#include <Serialization/Archive.h>

FCtcOutputDevice::FCtcOutputDevice()
{
	InstanceId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);

	const FString DefaultLogFilePath = FPlatformOutputDevices::GetAbsoluteLogFilename();
	const FString LogDirectory = FPaths::GetPath(DefaultLogFilePath);
	const FString LogBaseFilename = FPaths::GetBaseFilename(DefaultLogFilePath);
	const FString LogExtension = FPaths::GetExtension(DefaultLogFilePath, true).IsEmpty()
		? TEXT(".log")
		: FPaths::GetExtension(DefaultLogFilePath, true);

	LogFilePath = FPaths::Combine(LogDirectory, FString::Printf(TEXT("%s-structured%s"), *LogBaseFilename, *LogExtension));

	UE_LOG(LogCtcLogMonitoring, Display, TEXT("CtcOutputDevice initialized with InstanceId: %s"), *InstanceId);
	UE_LOG(LogCtcLogMonitoring, Display, TEXT("CtcOutputDevice writing logs to: %s"), *LogFilePath);

	GLog->AddOutputDevice(this);
	GLog->SerializeBacklog(this);

	const UCtcLogMonitoringSettings* Settings = GetDefault<UCtcLogMonitoringSettings>();
	const float TickInterval = Settings->LogUploadInterval;
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FCtcOutputDevice::Tick), TickInterval);
}

FCtcOutputDevice::~FCtcOutputDevice()
{
	TearDown();

	if (GLog)
	{
		GLog->RemoveOutputDevice(this);
	}

	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
}

void FCtcOutputDevice::Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category)
{
	FCtcLogMessage NewMessage;
	NewMessage.Verbosity = Verbosity;
	NewMessage.Category = Category;
	NewMessage.Data = Data;
	NewMessage.Time = FPlatformTime::Seconds() - GStartTime;

	FScopeLock ScopeLock(&PendingMessagesLock);
	PendingMessages.Add(MoveTemp(NewMessage));
}

void FCtcOutputDevice::Flush()
{
	SendPendingMessages();
}

void FCtcOutputDevice::TearDown()
{
	SendPendingMessages(true);

	if (LogFileArchive)
	{
		LogFileArchive->Flush();
		LogFileArchive->Close();
		delete LogFileArchive;
		LogFileArchive = nullptr;
	}
}

bool FCtcOutputDevice::Tick(float DeltaTime)
{
	Flush();

	return true;
}

bool FCtcOutputDevice::EnsureLogArchive()
{
	if (LogFileArchive)
	{
		return true;
	}

	IFileManager::Get().MakeDirectory(*FPaths::GetPath(LogFilePath), true);
	LogFileArchive = IFileManager::Get().CreateFileWriter(
		*LogFilePath,
		FILEWRITE_AllowRead | FILEWRITE_Append | FILEWRITE_Silent
	);

	if (!LogFileArchive)
	{
		return false;
	}

	return true;
}

void FCtcOutputDevice::SendPendingMessages(bool bWait)
{
	if (bWait)
	{
		// File writing is synchronous, so there is nothing extra to wait for.
	}

	TArray<FCtcLogMessage> MessagesToWrite;
	{
		FScopeLock ScopeLock(&PendingMessagesLock);
		if (PendingMessages.IsEmpty())
		{
			return;
		}

		MessagesToWrite = MoveTemp(PendingMessages);
		PendingMessages.Reset();
	}

	if (!EnsureLogArchive())
	{
		FScopeLock ScopeLock(&PendingMessagesLock);
		PendingMessages.Append(MoveTemp(MessagesToWrite));
		return;
	}

	for (const FCtcLogMessage& Message : MessagesToWrite)
	{
		FOutputDeviceHelper::FormatCastAndSerializeLine(
			*LogFileArchive,
			*Message.Data,
			Message.Verbosity,
			Message.Category,
			Message.Time,
			false,
			true
		);
	}

	LogFileArchive->Flush();

	/*
	Old upload flow (kept commented for reference as requested):

	const FString ProjectVersion = GetDefault<UGeneralProjectSettings>()->ProjectVersion;

	TArray<TSharedPtr<FJsonValue>> JsonArray;
	for (const FCtcLogMessage& Message : MessagesToSend)
	{
		TSharedPtr<FJsonObject> JsonObject = MakeShared<FJsonObject>();
		JsonObject->SetStringField(TEXT("timestamp"), Message.Time.ToIso8601());
		JsonObject->SetStringField(TEXT("data"), Message.Data);
		JsonObject->SetStringField(TEXT("verbosity"), ToString(Message.Verbosity));
		JsonObject->SetStringField(TEXT("category"), Message.Category.ToString());
		JsonObject->SetStringField(TEXT("instanceId"), InstanceId);
		JsonObject->SetStringField(TEXT("projectVersion"), ProjectVersion);

		JsonArray.Add(MakeShared<FJsonValueObject>(JsonObject));
	}

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	ensure(FJsonSerializer::Serialize(JsonArray, Writer));

	const UCtcSharedSettings* SharedSettings = GetDefault<UCtcSharedSettings>();
	const FString UrlEndpoint = FString::Printf(TEXT("%s/logs/upload"), *SharedSettings->ApiUrl);

	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb("POST");
	Request->SetURL(UrlEndpoint);
	Request->SetHeader("Content-Type", "application/json");
	Request->SetHeader("X-API-Key", SharedSettings->RuntimeApiKey);
	Request->SetContentAsString(JsonString);

	if (bWait)
	{
		Request->ProcessRequestUntilComplete();
		const bool bSuccess = Request->GetStatus() == EHttpRequestStatus::Succeeded;
		OnPendingMessagesSent(Request, Request->GetResponse(), bSuccess);
	}
	else
	{
		Request->OnProcessRequestComplete().BindRaw(this, &FCtcOutputDevice::OnPendingMessagesSent);
		Request->ProcessRequest();
	}
	*/
}

/*
void FCtcOutputDevice::OnPendingMessagesSent(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Request.IsValid())
	{
		UE_LOG(LogCtcLogMonitoring, Error, TEXT("Sending logs to backend failed: %s"), *Request->GetURL());
		return;
	}

	if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		return;
	}

	UE_LOG(LogCtcLogMonitoring, VeryVerbose, TEXT("Sent messages to backend successful"));
}
*/
