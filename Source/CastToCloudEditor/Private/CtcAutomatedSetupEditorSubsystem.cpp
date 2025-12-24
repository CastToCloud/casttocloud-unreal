// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAutomatedSetupEditorSubsystem.h"

#include <Framework/Application/SlateApplication.h>
#include <JsonObjectConverter.h>
#include <Misc/MessageDialog.h>

#include "CastToCloudEditor.h"
#include "CtcSharedSettings.h"

FString GetPayloadString(const TArray<uint8>& Payload)
{
	TArray<uint8> ZeroTerminatedPayload;
	ZeroTerminatedPayload.AddZeroed(Payload.Num() + 1);
	FMemory::Memcpy(ZeroTerminatedPayload.GetData(), Payload.GetData(), Payload.Num());

	return UTF8_TO_TCHAR(ZeroTerminatedPayload.GetData());
}

void UCtcAutomatedSetupEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FCastToCloudEditorModule::FOnHttpRequestReceived HandleCallback = FCastToCloudEditorModule::FOnHttpRequestReceived::CreateUObject(this, &UCtcAutomatedSetupEditorSubsystem::OnHttpRequestReceived);
	FCastToCloudEditorModule::RegisterHttpRequestHandler(TEXT("express-api-keys"), HandleCallback);
}

TUniquePtr<FHttpServerResponse> UCtcAutomatedSetupEditorSubsystem::OnHttpRequestReceived(const FHttpServerRequest& Request)
{
	const FString Payload = GetPayloadString(Request.Body);
	FCtcExpressApiKeyMessage Message;
	const bool bConvertSuccess = FJsonObjectConverter::JsonObjectStringToUStruct(Payload, &Message, 0, 0);

	if (!bConvertSuccess)
	{
		return FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("Failed to parse body."));
	}

	const FText DialogTitle = INVTEXT("API Keys received from the web");
	const FText DialogText = INVTEXT("Do you want to accept API keys from the web?");

	FSlateApplication& App = FSlateApplication::Get();
	bool bFirstTick = true;
	FDelegateHandle ForceToFrontHandle = App.GetOnModalLoopTickEvent().AddLambda(
		[&App, &bFirstTick](float DeltaTime)
		{
			if (bFirstTick)
			{
				App.GetActiveModalWindow()->HACK_ForceToFront();
				bFirstTick = false;
			}
		}
	);

	const EAppReturnType::Type Choice = FMessageDialog::Open(EAppMsgCategory::Warning, EAppMsgType::YesNo, DialogText, DialogTitle);

	App.GetOnModalLoopTickEvent().Remove(ForceToFrontHandle);

	if (Choice == EAppReturnType::No)
	{
		return FHttpServerResponse::Error(EHttpServerResponseCodes::Forbidden, TEXT("User refused keys."));
	}

	UCtcSharedSettings* MutableSettings = GetMutableDefault<UCtcSharedSettings>();
	MutableSettings->DeveloperApiKey.ApiKey = Message.DeveloperApiKey;
	MutableSettings->RuntimeApiKey.ApiKey = Message.RuntimeApiKey;
	MutableSettings->SaveToDefaultConfig();
	MutableSettings->ShowSettings();

	return FHttpServerResponse::Ok();
}
