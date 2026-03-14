#pragma once

#include <HAL/CriticalSection.h>
#include <Misc/OutputDevice.h>

// Upload-based transport kept below as commented reference.
// #include <Interfaces/IHttpRequest.h>

class FArchive;

/**
 * Represents the information about a log message
 */
struct FCtcLogMessage
{
	FString Data;
	FName Category;
	double Time = -1.0;
	ELogVerbosity::Type Verbosity;
};

/**
 * Output device used to write logs to a local file.
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
	bool EnsureLogArchive();
	void SendPendingMessages(bool bWait = false);

	// Upload implementation kept as reference:
	// void OnPendingMessagesSent(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bWasSuccessful);

	TArray<FCtcLogMessage> PendingMessages;
	FCriticalSection PendingMessagesLock;

	FTSTicker::FDelegateHandle TickHandle;

	FString InstanceId;
	FString LogFilePath;
	FArchive* LogFileArchive = nullptr;

	//TODO: Consider those
	//virtual bool CanBeUsedOnAnyThread() const override;
	//virtual bool CanBeUsedOnMultipleThreads() const override;
	//virtual bool CanBeUsedOnPanicThread() const override;
};
