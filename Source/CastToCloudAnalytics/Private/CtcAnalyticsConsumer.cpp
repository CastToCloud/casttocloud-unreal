// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAnalyticsConsumer.h"

#include <HttpModule.h>
#include <Interfaces/IHttpRequest.h>
#include <Interfaces/IHttpResponse.h>
#include <Misc/CommandLine.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>
#include <Serialization/JsonSerializer.h>
#include <Serialization/JsonWriter.h>

#include "CtcAnalyticsLog.h"
#include "CtcSharedSettings.h"

FCtcAnalyticsApiConsumer::FCtcAnalyticsApiConsumer()
{
	constexpr FHttpRetrySystem::FRetryLimitCountSetting RetryLimit = FHttpRetrySystem::FRetryLimitCountSetting(3);
	constexpr FHttpRetrySystem::FRetryTimeoutRelativeSecondsSetting InfiniteTimeout = FHttpRetrySystem::FRetryTimeoutRelativeSecondsSetting();
	HttpRetryManager = MakeShared<FHttpRetrySystem::FManager>(RetryLimit, InfiniteTimeout, RetryLimit);
}

void FCtcAnalyticsApiConsumer::HandleEvents(const FString& SessionId, const TArray<TSharedPtr<FJsonValue>>& Events, bool bWait)
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	const bool bAllowAnyConfiguration = FParse::Param(FCommandLine::Get(), TEXT("AnalyticsAnyConfiguration"));
	if (Settings && !Settings->AllowedExecutables.IsCurrentConfigurationAllowed() && !bAllowAnyConfiguration)
	{
		UE_LOG(LogCtcAnalytics, Warning, TEXT("ApiConsumer current configuration doesn't allow - session: %s, number of events: %s"), *SessionId, *LexToString(Events.Num()));
		return;
	}

	UE_LOG(LogCtcAnalytics, Verbose, TEXT("ApiConsumer handling - session: %s, number of events: %s"), *SessionId, *LexToString(Events.Num()));

	TSharedRef<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetArrayField(TEXT("eventsPayload"), Events);
	RequestBody->SetBoolField(TEXT("geoTracking"), Settings->bEnableGeolocationAttribution);

	FString RequestBodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	ensure(FJsonSerializer::Serialize(RequestBody, Writer));

	FHttpRequestRef Request = HttpRetryManager->CreateRequest();
	Request->SetVerb(TEXT("POST"));
	Request->SetURL(Settings->ApiUrl / TEXT("events/gameplay/record"));
	Request->SetHeader(TEXT("X-API-Key"), Settings->RuntimeApiKey);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(RequestBodyString);

	if (bWait)
	{
		// TODO: The retry manager doesn't work with ProcessRequestUntilComplete - however we will need to replace this when downgrading anyway.
		Request->ProcessRequestUntilComplete();
		const bool bSuccess = Request->GetStatus() == EHttpRequestStatus::Succeeded;
		OnEventResponse(Request, Request->GetResponse(), bSuccess);
	}
	else
	{
		Request->OnProcessRequestComplete().BindSP(this, &FCtcAnalyticsApiConsumer::OnEventResponse);
		Request->ProcessRequest();
	}
}

void FCtcAnalyticsApiConsumer::OnEventResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
{
	if (!bSuccess || !Response || !Response.IsValid())
	{
		UE_LOG(LogCtcAnalytics, Error, TEXT("ApiConsumer failed to send events to backend."));
		return;
	}
	if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		UE_LOG(LogCtcAnalytics, Error, TEXT("ApiConsumer received error from backend backend - code: %d body: {%s}"), Response->GetResponseCode(), *Response->GetContentAsString());
		return;
	}

	UE_LOG(LogCtcAnalytics, VeryVerbose, TEXT("ApiConsumer sent events to backend successful - code: %d body: {%s}"), Response->GetResponseCode(), *Response->GetContentAsString());
}

void FCtcAnalyticsFileConsumer::HandleEvents(const FString& SessionId, const TArray<TSharedPtr<FJsonValue>>& Events, bool bWait)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("AnalyticsToFile")))
	{
		return;
	}

	UE_LOG(LogCtcAnalytics, Display, TEXT("FileConsumer handling - session: %s, number of events: %s"), *SessionId, *LexToString(Events.Num()));

	const FString TargetFile = FPaths::ProjectSavedDir() / TEXT("CastToCloud") / TEXT("Analytics") / SessionId + TEXT(".json");
	TArray<TSharedPtr<FJsonValue>> AllEvents;

	FString PreviousEventsJSON;
	if (FFileHelper::LoadFileToString(PreviousEventsJSON, *TargetFile))
	{
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PreviousEventsJSON);
		FJsonSerializer::Deserialize(Reader, AllEvents);
	}

	AllEvents.Append(Events);

	FString OutputString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	FJsonSerializer::Serialize(AllEvents, Writer);

	FFileHelper::SaveStringToFile(OutputString, *TargetFile);
}

void FCtcAnalyticsLogConsumer::HandleEvents(const FString& SessionId, const TArray<TSharedPtr<FJsonValue>>& Events, bool bWait)
{
	if (!FParse::Param(FCommandLine::Get(), TEXT("AnalyticsToLog")))
	{
		return;
	}

	UE_LOG(LogCtcAnalytics, Display, TEXT("LogConsumer handling - session: %s, number of events: %s"), *SessionId, *LexToString(Events.Num()));

	for (const TSharedPtr<FJsonValue>& Event : Events)
	{
		FString EventString;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&EventString);
		FJsonSerializer::Serialize(Event->AsObject().ToSharedRef(), Writer);

		UE_LOG(LogCtcAnalytics, Display, TEXT("    %s"), *EventString)
	}
}
