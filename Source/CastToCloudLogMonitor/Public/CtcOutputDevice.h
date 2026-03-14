#pragma once

#include <Misc/OutputDevice.h>

#include <Interfaces/IHttpRequest.h>

/**
 * Represents the information about a log message
 */
struct FCtcLogMessage
{
	FString Data;
	FName Category;
	FDateTime Time;
	ELogVerbosity::Type Verbosity;
};

/**
 * Output device used to upload logs to the monitoring platform
 */
class FCtcOutputDevice final : public FOutputDevice
{
public:
	FCtcOutputDevice();
	virtual ~FCtcOutputDevice() override;

private:
	//~ Begin FOutputDevice interface
	virtual void Serialize(const TCHAR* Data, ELogVerbosity::Type Verbosity, const class FName& Category) override;
	virtual void Flush() override;
	virtual void TearDown() override;
	//~ End FOutputDevice interface

	bool Tick(float DeltaTime);
	void SendPendingMessages(bool bWait = false);
	void OnPendingMessagesSent(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	TArray<FCtcLogMessage> PendingMessages;

	FTSTicker::FDelegateHandle TickHandle;
	
	FString InstanceId;

	//TODO: Consider those
	//virtual bool CanBeUsedOnAnyThread() const override;
	//virtual bool CanBeUsedOnMultipleThreads() const override;
	//virtual bool CanBeUsedOnPanicThread() const override;
};
