// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <EditorSubsystem.h>
#include <HttpResultCallback.h>
#include <HttpServerRequest.h>

#include "CtcTraceViewerEditorSubsystem.generated.h"

USTRUCT()
struct FCtcTraceViewerMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FString TraceId;
};

UCLASS()
class CASTTOCLOUDMETRICSEDITOR_API UCtcTraceViewerEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	// ~Begin UEditorSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// ~End UEditorSubsystem interface

private:
	/*
	 * Callback executed when an HTTP Request is received by our endpoint.
	 */
	TUniquePtr<FHttpServerResponse> OnHttpRequestReceived(const FHttpServerRequest& Request);

	bool DownloadTraceFileBlocking(const FString& TraceId, FString& OutTraceFilePath, FString& OutError) const;
	bool LaunchInsights(const FString& TraceFilePath, FString& OutError) const;
};
