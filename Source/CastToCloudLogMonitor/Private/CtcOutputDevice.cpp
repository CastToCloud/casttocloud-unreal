#include "CtcOutputDevice.h"

#include <HttpModule.h>
#include <Interfaces/IHttpResponse.h>
#include <GeneralProjectSettings.h>

#include "CtcLogMonitor.h"
#include "CtcLogMonitoringSettings.h"
#include "CtcLogMonitorLog.h"
#include "CtcSharedSettings.h"

FCtcOutputDevice::FCtcOutputDevice()
{
	InstanceId = FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower);

	UE_LOG(LogCtcLogMonitoring, Display, TEXT("CtcOutputDevice initialized with InstanceId: %s"), *InstanceId);

	GLog->AddOutputDevice(this);
	GLog->SerializeBacklog(this);

	const UCtcLogMonitoringSettings* Settings = GetDefault<UCtcLogMonitoringSettings>();
	const float TickInterval = Settings->LogUploadInterval; 
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FCtcOutputDevice::Tick), TickInterval);
}

FCtcOutputDevice::~FCtcOutputDevice()
{
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
	NewMessage.Time = FDateTime::Now();

	PendingMessages.Add(NewMessage);
}

void FCtcOutputDevice::Flush()
{
	SendPendingMessages();
}

void FCtcOutputDevice::TearDown()
{
	SendPendingMessages(true);
}

bool FCtcOutputDevice::Tick(float DeltaTime)
{
	Flush();

	return true;
}

void FCtcOutputDevice::SendPendingMessages(bool bWait)
{
	if (PendingMessages.IsEmpty())
	{
		return;
	}

	static FCriticalSection SendMessagesLock;
	FScopeLock ScopeLock(&SendMessagesLock);

	const TArray<FCtcLogMessage> MessagesToSend = MoveTemp(PendingMessages);
	ensure(PendingMessages.IsEmpty());

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
}

void FCtcOutputDevice::OnPendingMessagesSent(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful)
{
	if (!bWasSuccessful || !Request.IsValid())
	{
		UE_LOG(LogCtcLogMonitoring, Error, TEXT("Sending logs to backend failed: %s"), *Request->GetURL());
		return;
	}

	if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		// UE_LOG(LogCtcLogMonitoring, Error, TEXT("Sending logs to backend returned: %d"), Response->GetResponseCode());
		return;
	}
	
	UE_LOG(LogCtcLogMonitoring, VeryVerbose, TEXT("Sent messages to backend successful"));
}