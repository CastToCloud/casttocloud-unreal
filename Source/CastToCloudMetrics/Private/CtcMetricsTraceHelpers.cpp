// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcMetricsTraceHelpers.h"

#include <Async/Async.h>
#include <Dom/JsonObject.h>
#include <HAL/FileManager.h>
#include <HttpModule.h>
#include <Interfaces/IHttpRequest.h>
#include <Interfaces/IHttpResponse.h>
#include <Misc/App.h>
#include <Misc/FileHelper.h>
#include <Misc/Paths.h>
#include <ProfilingDebugging/TraceAuxiliary.h>
#include <Serialization/JsonReader.h>
#include <Serialization/JsonSerializer.h>
#include <Serialization/JsonWriter.h>

#include "CtcMetricsLog.h"
#include "CtcSharedSettings.h"

namespace
{
	void UploadTraceBlocking(const FString& TraceId, const FString& TraceFile)
	{
		UE_LOG(LogCtcMetrics, Display, TEXT("[%s] Starting upload of trace from %s."), *TraceId, *TraceFile);

		const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();

		TSharedRef<FJsonObject> InfoUploadRequestBody = MakeShared<FJsonObject>();
		InfoUploadRequestBody->SetStringField(TEXT("traceId"), TraceId);

		FString InfoUploadRequestJson;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&InfoUploadRequestJson);
		ensure(FJsonSerializer::Serialize(InfoUploadRequestBody, Writer));

		UE_LOG(LogCtcMetrics, Display, TEXT("[%s] Requesting upload URL."), *TraceId);

		FHttpRequestRef InfoUploadRequest = FHttpModule::Get().CreateRequest();
		InfoUploadRequest->SetVerb(TEXT("POST"));
		InfoUploadRequest->SetURL(Settings->ApiUrl / TEXT("metrics/traces/upload"));
		InfoUploadRequest->SetHeader(TEXT("X-API-Key"), Settings->RuntimeApiKey);
		InfoUploadRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		InfoUploadRequest->SetContentAsString(InfoUploadRequestJson);

		InfoUploadRequest->ProcessRequestUntilComplete();

		FHttpResponsePtr InfoUploadResponse = InfoUploadRequest->GetResponse();
		if (!InfoUploadResponse || !EHttpResponseCodes::IsOk(InfoUploadResponse->GetResponseCode()))
		{
			const FString ResponseCode = InfoUploadResponse ? LexToString(InfoUploadResponse->GetResponseCode()) : TEXT("NoResponse");
			const FString ResponseBody = InfoUploadResponse ? InfoUploadResponse->GetContentAsString() : TEXT("NoResponseBody");
			UE_LOG(LogCtcMetrics, Error, TEXT("[%s] Upload URL request failed. ResponseCode=%s ResponseBody=%s"), *TraceId, *ResponseCode, *ResponseBody);
			return;
		}

		UE_LOG(LogCtcMetrics, Display, TEXT("[%s] Upload URL request completed."), *TraceId);

		TSharedPtr<FJsonObject> InfoUploadResponseBody;
		FString InfoUploadResponseJson = InfoUploadResponse->GetContentAsString();
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InfoUploadResponseJson);
		if (!FJsonSerializer::Deserialize(Reader, InfoUploadResponseBody) || !InfoUploadResponseBody.IsValid())
		{
			UE_LOG(LogCtcMetrics, Error, TEXT("[%s] Upload URL response JSON is invalid. ResponseBody=%s"), *TraceId, *InfoUploadResponseJson);
			return;
		}

		const FString FileUploadUrl = InfoUploadResponseBody->GetStringField(TEXT("uploadUrl"));
		UE_LOG(LogCtcMetrics, Display, TEXT("[%s] Upload URL retrieved"), *TraceId);

		TArray<uint8> FileUploadContent;
		if (!FFileHelper::LoadFileToArray(FileUploadContent, *TraceFile))
		{
			UE_LOG(LogCtcMetrics, Error, TEXT("[%s] Failed to read trace file for upload. TraceFile=%s"), *TraceId, *TraceFile);
			return;
		}

		const int64 TotalBytes = FileUploadContent.Num();

		FHttpRequestRef FileUploadRequest = FHttpModule::Get().CreateRequest();
		FileUploadRequest->SetVerb(TEXT("PUT"));
		FileUploadRequest->SetURL(FileUploadUrl);
		FileUploadRequest->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
		FileUploadRequest->SetContent(FileUploadContent);

		UE_LOG(LogCtcMetrics, Display, TEXT("[%s] Uploading trace file bytes."), *TraceId);

		FileUploadRequest->OnRequestProgress64().BindLambda([TraceId, TotalBytes](FHttpRequestPtr Request, uint64 BytesSent, uint64 BytesReceived)
		{
			const double PercentComplete = TotalBytes > 0 ? static_cast<double>(BytesSent) * 100.0 / static_cast<double>(TotalBytes) : 0.0;
			UE_LOG(LogCtcMetrics, Display, TEXT("[%s] Upload progress. Percent=%s BytesSent=%s BytesReceived=%s TotalBytes=%s"), *TraceId, *LexToString(PercentComplete), *LexToString(BytesSent), *LexToString(BytesReceived), *LexToString(TotalBytes));
		});

		FileUploadRequest->ProcessRequestUntilComplete();

		FHttpResponsePtr FileUploadResponse = FileUploadRequest->GetResponse();
		if (!FileUploadResponse || !EHttpResponseCodes::IsOk(FileUploadResponse->GetResponseCode()))
		{
			const FString ResponseCode = FileUploadResponse ? LexToString(FileUploadResponse->GetResponseCode()) : TEXT("NoResponse");
			const FString ResponseBody = FileUploadResponse ? FileUploadResponse->GetContentAsString() : TEXT("NoResponseBody");
			UE_LOG(LogCtcMetrics, Error, TEXT("[%s] Trace file upload failed. ResponseCode=%s ResponseBody=%s"), *TraceId, *ResponseCode, *ResponseBody);
			return;
		}
		UE_LOG(LogCtcMetrics, Display, TEXT("[%s] Trace file upload completed."), *TraceId);

		UE_LOG(LogCtcMetrics, Display, TEXT("[%s] Cleaning up trace data from disk."), *TraceId);
		const FString TraceDirectory = FPaths::GetPath(TraceFile);
		const bool bDeleteSucceeded = IFileManager::Get().DeleteDirectory(*TraceDirectory);
		UE_LOG(LogCtcMetrics, Display, TEXT("[%s] Trace directory cleanup completed. DeleteSucceeded=%s"), *TraceId, *LexToString(bDeleteSucceeded));
	}
} // namespace

void CastToCloudMetricsTraceHelpers::WriteAndUploadTrace(const FString& TraceId)
{
	UE_LOG(LogCtcMetrics, Display, TEXT("[%s] Generating trace."), *TraceId);

	const FString Folder = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()) / TEXT("CastToCloud") / TEXT("Monitoring") / TEXT("Trace_") + FDateTime::UtcNow().ToString();
	const FString TraceFile = Folder / TraceId + TEXT(".utrace");

	const bool bSnapshotWrite = FTraceAuxiliary::WriteSnapshot(*TraceFile);
	if (!bSnapshotWrite)
	{
		UE_LOG(LogCtcMetrics, Error, TEXT("[%s] Failed to write trace snapshot at %s"), *TraceId, *TraceFile);
		return;
	}

	UE_LOG(LogCtcMetrics, Display, TEXT("[%s] Trace snapshot created at %s. Uploading in the background."), *TraceId, *TraceFile);

	AsyncTask(ENamedThreads::AnyBackgroundHiPriTask, [TraceId, TraceFile]()
	{
		UploadTraceBlocking(TraceId, TraceFile);
	});
}

