// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <EditorSubsystem.h>
#include <HttpResultCallback.h>
#include <HttpServerRequest.h>

#include "CtcAutomatedSetupEditorSubsystem.generated.h"

USTRUCT()
struct FCtcExpressApiKeyMessage
{
	GENERATED_BODY()

	UPROPERTY()
	FString RuntimeApiKey;

	UPROPERTY()
	FString DeveloperApiKey;
};

UCLASS()
class CASTTOCLOUDEDITOR_API UCtcAutomatedSetupEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

	// ~Begin UEditorSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	// ~End UEditorSubsystem interface

	/*
	 * Callback executed when an HTTP Request is received by our endpoint
	 */
	TUniquePtr<FHttpServerResponse> OnHttpRequestReceived(const FHttpServerRequest& Request);
};
