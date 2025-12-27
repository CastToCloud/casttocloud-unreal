// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Interfaces/IAnalyticsProvider.h>
#include <Interfaces/IHttpRequest.h>

class CASTTOCLOUDANALYTICS_API FCtcAnalyticsProvider : public IAnalyticsProvider
{
public:
	FCtcAnalyticsProvider();

	// ~Begin IAnalyticsProvider interface
	virtual bool StartSession(const TArray<FAnalyticsEventAttribute>& Attributes) override;
	virtual void EndSession() override;
	virtual FString GetSessionID() const override;
	virtual bool SetSessionID(const FString& InSessionID) override;
	virtual void FlushEvents() override;
	virtual void SetUserID(const FString& InUserID) override;
	virtual FString GetUserID() const override;
	virtual void SetDefaultEventAttributes(TArray<FAnalyticsEventAttribute>&& Attributes) override;
	virtual TArray<FAnalyticsEventAttribute> GetDefaultEventAttributesSafe() const override;
	virtual int32 GetDefaultEventAttributeCount() const override;
	virtual FAnalyticsEventAttribute GetDefaultEventAttribute(int AttributeIndex) const override;
	virtual void RecordEvent(const FString& EventName, const TArray<FAnalyticsEventAttribute>& Attributes) override;
	// ~End IAnalyticsProvider interface

	void RecordEventWithTransform(const FString& EventName, const FTransform& Transform, const TArray<FAnalyticsEventAttribute>& Attributes);

#if !UE_BUILD_SHIPPING
	TArray<FString> GetDebugState() const;
#endif

private:
	/**
	 * Internal Record Event function used by all possible tracking methods
	 */
	void RecordEventInternal(const FString& EventName, TOptional<FTransform>& Transform, const TArray<FAnalyticsEventAttribute>& Attributes);
	/**
	 * Updates the built-in attributes applied to all events
	 */
	void RefreshBuiltInAttributes();
	/**
	 * Callback executed at a fixed interval to regularly send our cached events
	 */
	bool Tick(float DeltaTime);
	/**
	 * Callback executed when the HTTP response for the event request is retrieved
	 */
	void OnEventResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bSuccess);
	/**
	 * Send all the events currently in our cache clearing it
	 * @parm bWait If true, waits for the request to complete before returning
	 */
	void SendCachedEvents(bool bWait = false);
#if WITH_EDITOR
	/**
	 * Callback executed when the Play In Editor (PIE) session starts
	 */
	void OnPIEStarted(bool bIsSimulating);
#endif

#if WITH_EDITOR
	/**
	 * Callback executed when the Play In Editor (PIE) session ends
	 */
	void OnPIEEnded(bool bIsSimulating);
#endif
	/**
	 * Callback executed when the engine has started
	 */
	void OnPostEngineInit();
	/**
	 * Callback executed when the engine is about to exit
	 */
	void OnEnginePreExit();
	/**
	 * Callback executed when the system encounters an error (e.g.: a crash)
	 */
	void OnSystemError();
	/**
	 * Callback executed when the operating system tries to terminate the application
	 */
	void OnApplicationWillTerminate();
	/**
	 * Callback executed when a world's BeginPlay is executed
	 */
	void OnWorldBeginPlay(UWorld* World);
	/**
	 * Callback executed when a world's EndPlay is executed
	 */
	void OnWorldEndPlay(UWorld* World);
	/**
	 * Resets state variables of the provider in preparation for the next session
	 */
	void Reset();
	/**
	 * Checks if this instance is set as the active Analytics Provider
	 */
	bool IsActiveProvider() const;

	/**
	 * Data structure to hold all the information about an individual event
	 */
	struct FCachedEvent
	{
		FString Name;
		FDateTime Timestamp;
		FString World;
		TOptional<FTransform> Transform;
		TArray<FAnalyticsEventAttribute> Attributes;
	};
	/**
	 * Events already recorded we will send next flush
	 */
	TArray<FCachedEvent> CachedEvents;
	/**
	 * Information automatically appended by the plugin every event's extra properties
	 */
	TMap<FString, FString> BuiltInEventAttributes;
	/**
	 * Information automatically appended by the plugin every event's user properties
	 */
	TMap<FString, FString> BuildInUserAttributes;
	/**
	 * Default properties set by the user added to every event
	 */
	TArray<FAnalyticsEventAttribute> DefaultAttributes;
	/**
	 * Unique identifier for this session, generated at the very start
	 */
	TOptional<FString> SessionID;
	/**
	 * Identifier for the current user. We have a default value but it might get overridden
	 */
	TOptional<FString> UserID;
	/*
	 * Last UTC timestamp of the last sent of the cached events
	 */
	FDateTime LastTickSend;

	/**
	 * Current state of the session
	 */
	enum class ESessionState
	{
		None,
		Started,
		Ended
	};
	ESessionState State = ESessionState::None;
};
