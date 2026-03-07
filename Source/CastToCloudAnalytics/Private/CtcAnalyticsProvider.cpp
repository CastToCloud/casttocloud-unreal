// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAnalyticsProvider.h"

#include <Analytics.h>
#include <Dom/JsonObject.h>
#include <Engine/Engine.h>
#include <Engine/World.h>
#include <GeneralProjectSettings.h>
#include <GenericPlatform/GenericPlatformDriver.h>
#include <HardwareInfo.h>
#include <Interfaces/IPluginManager.h>
#include <Kismet/GameplayStatics.h>
#include <Misc/App.h>
#include <Misc/CommandLine.h>
#include <Misc/FileHelper.h>
#include <RHIGlobals.h>
#include <RHIStrings.h>
#include <Runtime/Launch/Resources/Version.h>
#include <Serialization/JsonSerializer.h>
#if WITH_EDITOR
#include <UnrealEdGlobals.h>
#include <Editor/UnrealEdEngine.h>
#endif

#include "CtcAnalyticsConsumer.h"
#include "CtcAnalyticsLog.h"
#include "CtcSharedHelpers.h"
#include "CtcSharedSettings.h"

// clang-format off
TAutoConsoleVariable CVarCtcAnalyticsPrintDebugFlags(
	TEXT("CastToCloud.Analytics.PrintDebugFlags"),
	false,
	TEXT("Prints to screen the debug state of the provider")
);
// clang-format on


// clang-format off
TAutoConsoleVariable CVarCtcAnalyticsPrintEventsTimeline(
	TEXT("CastToCloud.Analytics.PrintEventsTimeline"),
	false,
	TEXT("Prints to screen the events recorded by the provider")
);
// clang-format on

// TODO: Console command to instantly call flush with both wait & no wait as parameter

namespace
{
	// TOOD: This should be moved to dedicated file and handle all the way attribution should be done - via settings, via command line, via static delegate, maybe even blueprints?
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

	bool IsPlaying()
	{
#if WITH_EDITOR
		// For editor builds we consider the user playing if he is in a PIE session
		FWorldContext* PIEWorldContext = GUnrealEd->GetPIEWorldContext();
		if(PIEWorldContext && PIEWorldContext->WorldType == EWorldType::PIE)
		{
			return true;
		}
		return false;
#else
		// For non-editor builds we consider the user is always playing
		return true;
#endif
	}
} // namespace

FCtcAnalyticsProvider::FCtcAnalyticsProvider()
{
	Consumers.Add(MakeShared<FCtcAnalyticsApiConsumer>());
	Consumers.Add(MakeShared<FCtcAnalyticsFileConsumer>());
	Consumers.Add(MakeShared<FCtcAnalyticsLogConsumer>());
}

void FCtcAnalyticsProvider::RecordEventWithTransform(const FString& EventName, const FTransform& Transform, const TArray<FAnalyticsEventAttribute>& Attributes)
{
	TOptional<FTransform> InputTransform = Transform;
	RecordEventInternal(EventName, InputTransform, Attributes);
}

void FCtcAnalyticsProvider::FlushEvents(bool bBlocking)
{
	SendCachedEvents(bBlocking);
}

bool FCtcAnalyticsProvider::StartSession(const TArray<FAnalyticsEventAttribute>& Attributes)
{
	RefreshBuiltInAttributes();

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

	SendCachedEvents();

	Reset();
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
	FlushEvents(false);
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

	BuildInUserAttributes.Emplace(TEXT("gpu.rhi"), FHardwareInfo::GetHardwareInfo(NAME_RHI));
	BuildInUserAttributes.Emplace(TEXT("gpu.device"), GRHIAdapterName);
	BuildInUserAttributes.Emplace(TEXT("gpu.version"), GRHIAdapterUserDriverVersion);
	BuildInUserAttributes.Emplace(TEXT("gpu.provider"), RHIVendorIdToString());
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

	if (const TOptional<FString> WorldPackage = CastToCloudSharedHelpers::GetWorldPackage())
	{
		Event.World = *WorldPackage;
	}
	
	if (CVarCtcAnalyticsPrintEventsTimeline.GetValueOnAnyThread())
	{
		TArray<FString> AttributesStrings;
		Algo::Transform(Attributes, AttributesStrings, [](const FAnalyticsEventAttribute& Attribute){ return FString::Printf(TEXT("%s:%s"), *Attribute.GetName(), *Attribute.GetValue());});
		const FString AttributesAsString = FString::Join(AttributesStrings, TEXT(","));
		const FString TransformAsString = Event.Transform.IsSet() ? Event.Transform->ToString() : TEXT("-");
		
		const FString EventData = FString::Printf(TEXT("%s - [Transform= %s] [Attributes: %s]"), *Event.Name, *TransformAsString, *AttributesAsString);
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Black, *EventData);
	}


	CachedEvents.Add(Event);
}

bool FCtcAnalyticsProvider::Tick(float DeltaTime)
{
	if (!IsPlaying())
	{
		return true;
	}

	const FDateTime Now = FDateTime::UtcNow();
	if (!LastTickSend.IsSet())
	{
		LastTickSend = Now;
	}
	const FTimespan TimeSinceLast = Now - *LastTickSend;

	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (TimeSinceLast.GetTotalSeconds() > Settings->SendInterval)
	{
		SendCachedEvents();
	}

	if (CVarCtcAnalyticsPrintDebugFlags.GetValueOnAnyThread())
	{
		TArray<FString> DebugFlags;
		DebugFlags.Add(FString::Printf(TEXT("SessionId: %s"), *GetSessionID()));
		DebugFlags.Add(FString::Printf(TEXT("UserId: %s"), *GetUserID()));
		DebugFlags.Add(FString::Printf(TEXT("Events in cache: %s"), *LexToString(CachedEvents.Num())));
		DebugFlags.Add(FString::Printf(TEXT("Next flush in: %.2f"), Settings->SendInterval - TimeSinceLast.GetTotalSeconds()));

		GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, TEXT("Cast To Cloud Analytics"), false);
		for (const FString& DebugFlag : DebugFlags)
		{
			GEngine->AddOnScreenDebugMessage(-1, 0.0f, FColor::Black, *DebugFlag, false);
		}
	}

	return true;
}

void FCtcAnalyticsProvider::SendCachedEvents(bool bWait)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FCtcAnalyticsProvider::SendCachedEvents);

	LastTickSend = FDateTime::UtcNow();

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

	for (const TSharedPtr<FCtcAnalyticsConsumer>& Consumer : Consumers)
	{
		Consumer->HandleEvents(GetSessionID(), EventsArray, bWait);
	}
}

void FCtcAnalyticsProvider::Reset()
{
	State = ESessionState::None;
	UserID.Reset();
	SessionID.Reset();
	LastTickSend.Reset();
}

bool FCtcAnalyticsProvider::IsActiveProvider() const
{
	TSharedPtr<IAnalyticsProvider> Provider = FAnalytics::Get().GetDefaultConfiguredProvider();
	return Provider.IsValid() && Provider.Get() == this;
}
