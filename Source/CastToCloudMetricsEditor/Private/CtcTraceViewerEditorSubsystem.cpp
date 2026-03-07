// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcTraceViewerEditorSubsystem.h"

#include <GenericPlatform/GenericPlatformHttp.h>
#include <HAL/FileManager.h>
#include <HAL/PlatformProcess.h>
#include <HttpModule.h>
#include <Interfaces/IHttpRequest.h>
#include <Interfaces/IHttpResponse.h>
#include <JsonObjectConverter.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>

#include "CtcSharedApiKey.h"
#include "CtcSharedEditorModule.h"
#include "CtcSharedLog.h"
#include "CtcSharedSettings.h"

namespace
{
	const FString OpenTraceInInsightsMessage = TEXT("open-trace-in-insights");

	FString GetPayloadString(const TArray<uint8>& Payload)
	{
		TArray<uint8> ZeroTerminatedPayload;
		ZeroTerminatedPayload.AddZeroed(Payload.Num() + 1);
		FMemory::Memcpy(ZeroTerminatedPayload.GetData(), Payload.GetData(), Payload.Num());

		return UTF8_TO_TCHAR(ZeroTerminatedPayload.GetData());
	}
} // namespace

void UCtcTraceViewerEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	FCtcSharedEditorModule::FOnHttpRequestReceived HandleCallback = FCtcSharedEditorModule::FOnHttpRequestReceived::CreateUObject(this, &UCtcTraceViewerEditorSubsystem::OnHttpRequestReceived);
	FCtcSharedEditorModule::RegisterHttpRequestHandler(OpenTraceInInsightsMessage, HandleCallback);
}

void UCtcTraceViewerEditorSubsystem::Deinitialize()
{
	FCtcSharedEditorModule::UnregisterHttpRequestHandler(OpenTraceInInsightsMessage);

	Super::Deinitialize();
}

TUniquePtr<FHttpServerResponse> UCtcTraceViewerEditorSubsystem::OnHttpRequestReceived(const FHttpServerRequest& Request)
{
	const FString Payload = GetPayloadString(Request.Body);
	FCtcTraceViewerMessage Message;
	const bool bConvertSuccess = FJsonObjectConverter::JsonObjectStringToUStruct(Payload, &Message, 0, 0);

	Message.TraceId.TrimStartAndEndInline();
	if (!bConvertSuccess || Message.TraceId.IsEmpty())
	{
		return FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, TEXT("Failed to parse TraceId from body."));
	}

	FString TraceFilePath;
	FString DownloadError;
	if (!DownloadTraceFileBlocking(Message.TraceId, TraceFilePath, DownloadError))
	{
		UE_LOG(LogCtcShared, Error, TEXT("Failed to download trace file. TraceId=%s Error=%s"), *Message.TraceId, *DownloadError);
		return FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, *DownloadError);
	}

	FString LaunchError;
	if (!LaunchInsights(TraceFilePath, LaunchError))
	{
		UE_LOG(LogCtcShared, Error, TEXT("Failed to launch Unreal Insights. TraceId=%s TraceFile=%s Error=%s"), *Message.TraceId, *TraceFilePath, *LaunchError);
		return FHttpServerResponse::Error(EHttpServerResponseCodes::BadRequest, *LaunchError);
	}

	UE_LOG(LogCtcShared, Display, TEXT("Trace opened in Unreal Insights. TraceId=%s TraceFile=%s"), *Message.TraceId, *TraceFilePath);
	return FHttpServerResponse::Ok();
}

bool UCtcTraceViewerEditorSubsystem::DownloadTraceFileBlocking(const FString& TraceId, FString& OutTraceFilePath, FString& OutError) const
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (!Settings)
	{
		OutError = TEXT("Shared settings are unavailable.");
		return false;
	}

#if WITH_EDITORONLY_DATA
	const FString DeveloperApiKey = Settings->DeveloperApiKey;
#else
	const FString DeveloperApiKey;
#endif

	if (DeveloperApiKey.IsEmpty() || DeveloperApiKey.Equals(FCtcSharedApiKey::PlaceholderKey))
	{
		OutError = TEXT("Developer API Key is not configured.");
		return false;
	}

	const FString DownloadUrl = Settings->ApiUrl / TEXT("metrics/traces/download") + TEXT("?traceId=") + FGenericPlatformHttp::UrlEncode(TraceId);

	FHttpRequestRef DownloadRequest = FHttpModule::Get().CreateRequest();
	DownloadRequest->SetVerb(TEXT("GET"));
	DownloadRequest->SetURL(DownloadUrl);
	DownloadRequest->SetHeader(TEXT("X-API-Key"), DeveloperApiKey);
	DownloadRequest->ProcessRequestUntilComplete();

	FHttpResponsePtr DownloadResponse = DownloadRequest->GetResponse();
	if (!DownloadResponse.IsValid())
	{
		OutError = TEXT("Trace download request failed with no response.");
		return false;
	}

	int32 ResponseCode = DownloadResponse->GetResponseCode();
	if (ResponseCode == EHttpResponseCodes::BadRequest)
	{
		const FString RedirectUrl = DownloadResponse->GetHeader(TEXT("Location"));
		if (RedirectUrl.IsEmpty())
		{
			OutError = TEXT("Trace download redirect is missing Location header.");
			return false;
		}

		FHttpRequestRef RedirectRequest = FHttpModule::Get().CreateRequest();
		RedirectRequest->SetVerb(TEXT("GET"));
		RedirectRequest->SetURL(RedirectUrl);
		RedirectRequest->ProcessRequestUntilComplete();

		DownloadResponse = RedirectRequest->GetResponse();
		if (!DownloadResponse.IsValid())
		{
			OutError = TEXT("Trace download redirect request failed with no response.");
			return false;
		}

		ResponseCode = DownloadResponse->GetResponseCode();
	}

	if (!EHttpResponseCodes::IsOk(ResponseCode))
	{
		const FString ResponseBody = DownloadResponse->GetContentAsString();
		OutError = FString::Printf(TEXT("Trace download failed. ResponseCode=%s ResponseBody=%s"), *LexToString(ResponseCode), *ResponseBody);
		return false;
	}

	const TArray<uint8>& TraceData = DownloadResponse->GetContent();
	if (TraceData.IsEmpty())
	{
		OutError = TEXT("Trace download returned empty content.");
		return false;
	}

	const FString MonitoringFolder = FPaths::ProjectSavedDir() / TEXT("CastToCloud") / TEXT("Monitoring");
	IFileManager& FileManager = IFileManager::Get();
	if (!FileManager.DirectoryExists(*MonitoringFolder) && !FileManager.MakeDirectory(*MonitoringFolder, true))
	{
		OutError = FString::Printf(TEXT("Failed to create monitoring directory at %s"), *MonitoringFolder);
		return false;
	}

	OutTraceFilePath = MonitoringFolder / (TraceId + TEXT(".utrace"));
	if (!FFileHelper::SaveArrayToFile(TraceData, *OutTraceFilePath))
	{
		OutError = FString::Printf(TEXT("Failed to write trace file at %s"), *OutTraceFilePath);
		return false;
	}

	return true;
}

bool UCtcTraceViewerEditorSubsystem::LaunchInsights(const FString& TraceFilePath, FString& OutError) const
{
#if PLATFORM_WINDOWS
	const FString InsightsExecutable = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/Win64/UnrealInsights.exe"));
#elif PLATFORM_MAC
	const FString InsightsExecutable = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/Mac/UnrealInsights.app/Contents/MacOS/UnrealInsights"));
#elif PLATFORM_LINUX
	const FString InsightsExecutable = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Binaries/Linux/UnrealInsights"));
#else
	const FString InsightsExecutable;
#endif

	if (!InsightsExecutable.IsEmpty() && FPaths::FileExists(InsightsExecutable))
	{
		const FString Params = FString::Printf(TEXT("\"%s\""), *TraceFilePath);
		FProcHandle ProcessHandle = FPlatformProcess::CreateProc(*InsightsExecutable, *Params, true, false, false, nullptr, 0, nullptr, nullptr);
		if (!ProcessHandle.IsValid())
		{
			OutError = FString::Printf(TEXT("Failed to launch UnrealInsights executable at %s"), *InsightsExecutable);
			return false;
		}

		FPlatformProcess::CloseProc(ProcessHandle);
		return true;
	}

	// Fallback when the expected executable path does not exist.
	FPlatformProcess::LaunchFileInDefaultExternalApplication(*TraceFilePath);
	return true;
}
