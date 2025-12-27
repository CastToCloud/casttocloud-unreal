// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAnalyticsProvider.h"

#include <Analytics.h>
#include <Dom/JsonObject.h>
#include <Engine/World.h>
#include <GameFramework/Pawn.h>
#include <GeneralProjectSettings.h>
#include <GenericPlatform/GenericPlatformDriver.h>
#include <HttpModule.h>
#include <Interfaces/IHttpResponse.h>
#include <Interfaces/IPluginManager.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/App.h>
#include <Misc/CommandLine.h>
#include <Misc/FileHelper.h>
#include <Runtime/Launch/Resources/Version.h>
#include <Serialization/JsonSerializer.h>
#include <UObject/Package.h>

#if WITH_EDITOR
#include <Editor.h>
#endif

#include "CtcAnalyticsLog.h"
#include "CtcSharedSettings.h"

namespace
{
	FString GetPlatformAttribution()
	{
		FString PotentialValue;
		FParse::Value(FCommandLine::Get(), TEXT("AnalyticsPlatformAttribution="), PotentialValue, false);
		if (!PotentialValue.IsEmpty())
		{
			return PotentialValue;
		}

		const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
		if (!Settings->PlatformAttribution.IsEmpty())
		{
			return Settings->PlatformAttribution;
		}

		// TODO: It would be super interesting if we could get this based on the native OSS ? Maybe not by default but in general a potential idea.
		return {};
	}

	void SaveEventsToFile(const FString& SessionId, const TArray<TSharedPtr<FJsonValue>>& Events)
	{
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

	void PrintEventsToLog(const FString& SessionId, const TArray<TSharedPtr<FJsonValue>>& Events)
	{
		UE_LOG(LogCtcAnalytics, Display, TEXT("Printing %s cached events for session %s:"), *LexToString(Events.Num()), *SessionId);
		for (const TSharedPtr<FJsonValue>& Event : Events)
		{
			FString EventString;
			TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&EventString);
			FJsonSerializer::Serialize(Event->AsObject().ToSharedRef(), Writer);

			UE_LOG(LogCtcAnalytics, Display, TEXT("    %s"), *EventString)
		}
	}
} // namespace

FCtcAnalyticsProvider::FCtcAnalyticsProvider()
{
	FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateRaw(this, &FCtcAnalyticsProvider::Tick), 0.0f);

#if WITH_EDITOR
	FEditorDelegates::StartPIE.AddRaw(this, &FCtcAnalyticsProvider::OnPIEStarted);
	FEditorDelegates::ShutdownPIE.AddRaw(this, &FCtcAnalyticsProvider::OnPIEEnded);
#else
	FCoreDelegates::OnPostEngineInit.AddRaw(this, &FCtcAnalyticsProvider::OnPostEngineInit);
	FCoreDelegates::OnEnginePreExit.AddRaw(this, &FCtcAnalyticsProvider::OnEnginePreExit);
	FCoreDelegates::OnHandleSystemError.AddRaw(this, &FCtcAnalyticsProvider::OnSystemError);
	FCoreDelegates::GetApplicationWillTerminateDelegate().AddRaw(this, &FCtcAnalyticsProvider::OnApplicationWillTerminate);
#endif

	// NOTE: There is no FWorldDelegates::BeginPlay so we register for world creation and then bind to the World's BeginPlay delegate.
	FWorldDelegates::OnPostWorldInitialization.AddLambda(
		[this](UWorld* World, FWorldInitializationValues WorldInitializationValues)
		{
			World->OnWorldBeginPlay.AddRaw(this, &FCtcAnalyticsProvider::OnWorldBeginPlay, World);
		}
	);

	// NOTE: There is no FWorldDelegates::EndPlay, but OnWorldBeginTearDown is called during UWorld::EndPlay.
	FWorldDelegates::OnWorldBeginTearDown.AddRaw(this, &FCtcAnalyticsProvider::OnWorldEndPlay);
}

void FCtcAnalyticsProvider::RecordEventWithTransform(const FString& EventName, const FTransform& Transform, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	TOptional<FTransform> InputTransform = Transform;
	RecordEventInternal(EventName, InputTransform, Attributes);
}

#if !UE_BUILD_SHIPPING
TArray<FString> FCtcAnalyticsProvider::GetDebugState() const
{
	TArray<FString> InfoLines;

	InfoLines.Add(FString::Printf(TEXT("SessionId: %s"), *GetSessionID()));
	InfoLines.Add(FString::Printf(TEXT("UserId: %s"), *GetUserID()));
	InfoLines.Add(FString::Printf(TEXT("Events in cache: %s"), *LexToString(CachedEvents.Num())));

	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	const FDateTime Now = FDateTime::UtcNow();
	const FTimespan TimeSinceLast = Now - LastTickSend;
	const float NextSendIn = Settings->SendInterval - TimeSinceLast.GetTotalSeconds();
	InfoLines.Add(FString::Printf(TEXT("Next flush in: %.2f"), NextSendIn));

	return InfoLines;
}
#endif

bool FCtcAnalyticsProvider::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
	if (State == ESessionState::None)
	{
		State = ESessionState::Started;
		RecordEvent(TEXT("SessionStart"), Attributes);
	}
	else
	{
		UE_LOG(LogCtcAnalytics, Warning, TEXT("Session already started. Ignoring StartSession call."));
	}

	return true;
}

void FCtcAnalyticsProvider::EndSession()
{
	if (State == ESessionState::Started)
	{
		RecordEvent(TEXT("SessionEnd"), TArray<FAnalyticsEventAttribute>());
		State = ESessionState::Ended;
	}
	else
	{
		UE_LOG(LogCtcAnalytics, Warning, TEXT("Session not started or already ended. Ignoring EndSession call."));
	}
}

FString FCtcAnalyticsProvider::GetSessionID() const
{
	return SessionID.Get(TEXT("-"));
}

bool FCtcAnalyticsProvider::SetSessionID(const FString& InSessionID)
{
	SessionID = InSessionID;
	return true;
}

void FCtcAnalyticsProvider::FlushEvents()
{
	SendCachedEvents();
}

void FCtcAnalyticsProvider::SetUserID(const FString& InUserID)
{
	UserID = InUserID;
}

FString FCtcAnalyticsProvider::GetUserID() const
{
	return UserID.Get(TEXT("-"));
}

void FCtcAnalyticsProvider::SetDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes)
{
	DefaultAttributes = Attributes;
}

TArray<FAnalyticsEventAttribute> FCtcAnalyticsProvider::GetDefaultEventAttributesSafe() const
{
	return DefaultAttributes;
}

int32 FCtcAnalyticsProvider::GetDefaultEventAttributeCount() const
{
	return DefaultAttributes.Num();
}

FAnalyticsEventAttribute FCtcAnalyticsProvider::GetDefaultEventAttribute(int AttributeIndex) const
{
	return DefaultAttributes[AttributeIndex];
}

void FCtcAnalyticsProvider::RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	TOptional<FTransform> NoTransform;
	RecordEventInternal(EventName, NoTransform, Attributes);
}

void FCtcAnalyticsProvider::RefreshBuiltInAttributes()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCtcAnalyticsProvider::RefreshBuiltInAttributes);

	BuiltInEventAttributes.Empty();
	BuiltInEventAttributes.Emplace(TEXT("build_configuration"), LexToString(FApp::GetBuildConfiguration()));
	BuiltInEventAttributes.Emplace(TEXT("build_target"), LexToString(FApp::GetBuildTargetType()));
	BuiltInEventAttributes.Emplace(TEXT("project_version"), GetDefault<UGeneralProjectSettings>()->ProjectVersion);
	BuiltInEventAttributes.Emplace(TEXT("engine_version"), FString::Printf(TEXT("%d.%d.%d"), ENGINE_MAJOR_VERSION, ENGINE_MINOR_VERSION, ENGINE_PATCH_VERSION));
	BuiltInEventAttributes.Emplace(TEXT("plugin_version"), IPluginManager::Get().FindPlugin(TEXT("CastToCloud"))->GetDescriptor().VersionName);

	BuildInUserAttributes.Empty();
	BuildInUserAttributes.Emplace(TEXT("device"), UGameplayStatics::GetPlatformName());
	BuildInUserAttributes.Emplace(TEXT("device_version"), FPlatformMisc::GetOSVersion());
	BuildInUserAttributes.Emplace(TEXT("platform"), GetPlatformAttribution());

	FGPUDriverInfo GpuDriverInfo = FPlatformMisc::GetGPUDriverInfo(FPlatformMisc::GetPrimaryGPUBrand());
	BuildInUserAttributes.Emplace(TEXT("gpu.device"), GpuDriverInfo.DeviceDescription);
	BuildInUserAttributes.Emplace(TEXT("gpu.provider"), GpuDriverInfo.ProviderName);
	BuildInUserAttributes.Emplace(TEXT("gpu.version"), GpuDriverInfo.UserDriverVersion);
}

void FCtcAnalyticsProvider::RecordEventInternal(const FString& EventName, TOptional<FTransform>& Transform, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCtcAnalyticsProvider::RecordEventInternal);

	if (!IsActiveProvider())
	{
		UE_LOG(LogCtcAnalytics, VeryVerbose, TEXT("Event %s was skipped because CastToCloud is not the current provider."), *EventName);
		return;
	}

	if (State == ESessionState::None)
	{
		UE_LOG(LogCtcAnalytics, Warning, TEXT("Event %s was recorded before session start."), *EventName);
	}
	else if (State == ESessionState::Ended)
	{
		UE_LOG(LogCtcAnalytics, Warning, TEXT("Event %s was recorded after session end."), *EventName);
	}

	FCachedEvent Event;
	Event.Name = EventName;
	Event.Transform = Transform;
	Event.Timestamp = FDateTime::UtcNow();
	Event.Attributes = Attributes;

	if (GWorld && GWorld->GetPackage())
	{
		Event.World = UWorld::StripPIEPrefixFromPackageName(GWorld->GetPackage()->GetName(), GWorld->StreamingLevelsPrefix);
	}

	CachedEvents.Add(Event);
}

bool FCtcAnalyticsProvider::Tick(float DeltaTime)
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();

	const FDateTime Now = FDateTime::UtcNow();
	const FTimespan TimeSinceLast = Now - LastTickSend;
	if (TimeSinceLast.GetTotalSeconds() > Settings->SendInterval)
	{
		SendCachedEvents();
	}

	return true;
}

void FCtcAnalyticsProvider::SendCachedEvents(bool bWait)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCtcAnalyticsProvider::SendCachedEvents);

	const FDateTime Now = FDateTime::UtcNow();
	LastTickSend = Now;

	if (CachedEvents.IsEmpty())
	{
		return;
	}

	if (!UserID.IsSet())
	{
		const FString UniqueUserId = FGenericPlatformMisc::GetLoginId();
		SetUserID(UniqueUserId);
	}

	if (!SessionID.IsSet())
	{
		SetSessionID(FGuid::NewGuid().ToString());
	}

	UE_LOG(LogCtcAnalytics, Verbose, TEXT("Sending %s cached events"), *LexToString(CachedEvents.Num()));
	TArray<TSharedPtr<FJsonValue>> EventsArray;
	for (const FCachedEvent& Event : CachedEvents)
	{
		TSharedRef<FJsonObject> EventObject = MakeShared<FJsonObject>();
		EventObject->SetStringField(TEXT("event_name"), Event.Name);
		EventObject->SetStringField(TEXT("created_at"), Event.Timestamp.ToIso8601());
		EventObject->SetStringField(TEXT("session_id"), GetSessionID());
		EventObject->SetStringField(TEXT("user_id"), GetUserID());

		// Merge the event's properties with the default attributes. Event attributes are appended last so they can override
		TSharedPtr<FJsonObject> EventProperties = MakeShared<FJsonObject>();
		for (const TTuple<FString, FString>& Attribute : BuiltInEventAttributes)
		{
			EventProperties->SetField(Attribute.Key, MakeShared<FJsonValueString>(Attribute.Value));
		}
		for (const FAnalyticsEventAttribute& Attribute : DefaultAttributes)
		{
			EventProperties->SetField(Attribute.GetName(), MakeShared<FJsonValueString>(Attribute.GetValue()));
		}
		for (const FAnalyticsEventAttribute& Attribute : Event.Attributes)
		{
			EventProperties->SetField(Attribute.GetName(), MakeShared<FJsonValueString>(Attribute.GetValue()));
		}
		EventObject->SetObjectField(TEXT("event_properties"), EventProperties);

		TSharedPtr<FJsonObject> UserProperties = MakeShared<FJsonObject>();
		for (const TTuple<FString, FString>& Attribute : BuildInUserAttributes)
		{
			UserProperties->SetField(Attribute.Key, MakeShared<FJsonValueString>(Attribute.Value));
		}
		EventObject->SetObjectField(TEXT("user_properties"), UserProperties);

		EventObject->SetStringField(TEXT("world"), Event.World);

		if (Event.Transform.IsSet())
		{
			const FVector Position = Event.Transform->GetTranslation();
			EventObject->SetNumberField(TEXT("position_x"), Position.X);
			EventObject->SetNumberField(TEXT("position_y"), Position.Y);
			EventObject->SetNumberField(TEXT("position_z"), Position.Z);

			const FQuat Rotation = Event.Transform->GetRotation();
			EventObject->SetNumberField(TEXT("rotation_x"), Rotation.X);
			EventObject->SetNumberField(TEXT("rotation_y"), Rotation.Y);
			EventObject->SetNumberField(TEXT("rotation_z"), Rotation.Z);
			EventObject->SetNumberField(TEXT("rotation_w"), Rotation.W);
		}

		EventsArray.Add(MakeShared<FJsonValueObject>(EventObject));
	}
	CachedEvents.Empty();

	if (FParse::Param(FCommandLine::Get(), TEXT("AnalyticsToFile")))
	{
		SaveEventsToFile(GetSessionID(), EventsArray);
		return;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("AnalyticsToLog")))
	{
		PrintEventsToLog(GetSessionID(), EventsArray);
		return;
	}

	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	const bool bAllowAnyConfiguration = FParse::Param(FCommandLine::Get(), TEXT("AnalyticsAnyConfiguration"));
	if (Settings && !Settings->AllowedExecutables.IsCurrentConfigurationAllowed() && !bAllowAnyConfiguration)
	{
		UE_LOG(LogCtcAnalytics, Warning, TEXT("Skipping %s events for session %s because current configuration is not allowed"), *LexToString(EventsArray.Num()), *GetSessionID());
		return;
	}

	TSharedRef<FJsonObject> RequestBody = MakeShared<FJsonObject>();
	RequestBody->SetArrayField(TEXT("eventsPayload"), EventsArray);
	RequestBody->SetBoolField(TEXT("geoTracking"), Settings->bEnableGeolocationAttribution);

	FString RequestBodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&RequestBodyString);
	ensure(FJsonSerializer::Serialize(RequestBody, Writer));

	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(TEXT("POST"));
	Request->SetURL(Settings->ApiUrl / TEXT("events/record"));
	Request->SetHeader(TEXT("X-API-Key"), Settings->RuntimeApiKey);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(RequestBodyString);

	if (bWait)
	{
		Request->ProcessRequestUntilComplete();
		const bool bSuccess = Request->GetStatus() == EHttpRequestStatus::Succeeded;
		OnEventResponse(Request, Request->GetResponse(), bSuccess);
	}
	else
	{
		Request->OnProcessRequestComplete().BindRaw(this, &FCtcAnalyticsProvider::OnEventResponse);
		Request->ProcessRequest();
	}
}

#if WITH_EDITOR
void FCtcAnalyticsProvider::OnPIEStarted(bool bIsSimulating)
{
	UE_LOG(LogCtcAnalytics, Verbose, TEXT("OnPIEStarted called. Starting session."));

	RefreshBuiltInAttributes();

	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (Settings->bAutoStartSession && State == ESessionState::None)
	{
		StartSession({});
	}
}
#endif

#if WITH_EDITOR
void FCtcAnalyticsProvider::OnPIEEnded(bool bIsSimulating)
{
	UE_LOG(LogCtcAnalytics, Verbose, TEXT("OnPIEEnded called. Ending session and sending cached events."));

	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (Settings->bAutoStartSession && State == ESessionState::Started)
	{
		EndSession();
	}

	SendCachedEvents(true);

	Reset();
}
#endif

void FCtcAnalyticsProvider::OnPostEngineInit()
{
	UE_LOG(LogCtcAnalytics, Verbose, TEXT("OnPostEngineInit called. Starting session."));

	RefreshBuiltInAttributes();

	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (Settings->bAutoStartSession && State == ESessionState::None)
	{
		StartSession({});
	}
}

void FCtcAnalyticsProvider::OnEnginePreExit()
{
	UE_LOG(LogCtcAnalytics, Verbose, TEXT("OnEnginePreExit called. Ending session and sending cached events."));

	RecordEvent(TEXT("EngineExit"), TArray<FAnalyticsEventAttribute>());

	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (Settings->bAutoStartSession && State == ESessionState::Started)
	{
		EndSession();
	}


	SendCachedEvents(true);

	Reset();
}

void FCtcAnalyticsProvider::OnSystemError()
{
	UE_LOG(LogCtcAnalytics, Verbose, TEXT("OnSystemError called. Sending cached events."));

	RecordEvent(TEXT("SystemError"), TArray<FAnalyticsEventAttribute>());

	SendCachedEvents(true);
}

void FCtcAnalyticsProvider::OnApplicationWillTerminate()
{
	UE_LOG(LogCtcAnalytics, Verbose, TEXT("OnApplicationWillTerminate called. Sending cached events."));

	RecordEvent(TEXT("ApplicationWillTerminate"), TArray<FAnalyticsEventAttribute>());

	SendCachedEvents(true);
}

void FCtcAnalyticsProvider::OnWorldBeginPlay(UWorld* World)
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (!Settings->bAutoWorldChangeTracking)
	{
		return;
	}

	const FString WorldPath = World->GetOutermost()->GetPathName();
	if (WorldPath.IsEmpty() || WorldPath.StartsWith(TEXT("/Temp/Untitled")))
	{
		return;
	}

	if (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)
	{
		return;
	}

	RecordEvent(TEXT("WorldStart"), TArray<FAnalyticsEventAttribute>());
}

void FCtcAnalyticsProvider::OnWorldEndPlay(UWorld* World)
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (!Settings->bAutoWorldChangeTracking)
	{
		return;
	}

	const FString WorldPath = World->GetOutermost()->GetPathName();
	if (WorldPath.IsEmpty() || WorldPath.StartsWith(TEXT("/Temp/Untitled")))
	{
		return;
	}

	if (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)
	{
		return;
	}

	RecordEvent(TEXT("WorldEnd"), TArray<FAnalyticsEventAttribute>());
}

void FCtcAnalyticsProvider::Reset()
{
	State = ESessionState::None;
	UserID.Reset();
	SessionID.Reset();
}

bool FCtcAnalyticsProvider::IsActiveProvider() const
{
	TSharedPtr<IAnalyticsProvider> Provider = FAnalytics::Get().GetDefaultConfiguredProvider();
	return Provider.IsValid() && Provider.Get() == this;
}

void FCtcAnalyticsProvider::OnEventResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCtcAnalyticsProvider::OnEventResponse);

	// TODO: Implement a retry system via HttpRetryManager
	if (!bSuccess || !Response || !Response.IsValid())
	{
		UE_LOG(LogCtcAnalytics, Error, TEXT("Sending events to backend failed."));
		return;
	}
	if (!EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		UE_LOG(LogCtcAnalytics, Error, TEXT("Request to send events to backend failed with code: %d body: {%s}"), Response->GetResponseCode(), *Response->GetContentAsString());
		return;
	}

	UE_LOG(LogCtcAnalytics, VeryVerbose, TEXT("Sending events to backend successful. Response: {%s}"), *Response->GetContentAsString());
}
