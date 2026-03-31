// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcMessagingRelayClientSubsystem.h"

#include <Async/Async.h>
#include <Framework/Docking/TabManager.h>
#include <HttpModule.h>
#include <INetworkMessagingExtension.h>
#include <Features/IModularFeatures.h>
#include <Interfaces/IHttpRequest.h>
#include <Interfaces/IHttpResponse.h>
#include <Serialization/JsonSerializer.h>
#include <Widgets/Docking/SDockTab.h>

#include "CtcConnectLog.h"
#include "CtcSharedHelpers.h"
#include "CtcSharedSettings.h"

void UCtcMessagingRelayClientSubsystem::Deinitialize()
{
	CliProcesses.Reset();
}

void UCtcMessagingRelayClientSubsystem::Tick(float DeltaTime)
{
	if (FGlobalTabmanager::Get()->FindExistingLiveTab(FName("SessionFrontend")))
	{
		if (!bRefreshInProgress)
		{
			bRefreshInProgress = true;
			AsyncTask(
				ENamedThreads::AnyBackgroundHiPriTask,
				[this]()
				{
					RefreshTunnelsAsync();
					bRefreshInProgress = false;
				}
			);
		}
	}
}

bool UCtcMessagingRelayClientSubsystem::IsTickableInEditor() const
{
	return true;
}

ETickableTickType UCtcMessagingRelayClientSubsystem::GetTickableTickType() const
{
	const bool bIsCDO = HasAnyFlags(RF_ClassDefaultObject);
	return !bIsCDO ? ETickableTickType::Always : ETickableTickType::Never;
}

TStatId UCtcMessagingRelayClientSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCtcMessagingRelayClientSubsystem, STATGROUP_Tickables);
}

UWorld* UCtcMessagingRelayClientSubsystem::GetTickableGameObjectWorld() const
{
	return CastToCloudSharedHelpers::GetCurrentWorld();
}

void UCtcMessagingRelayClientSubsystem::RefreshTunnelsAsync()
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();

	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetVerb(TEXT("GET"));
	Request->SetURL(Settings->ApiUrl / TEXT("direct-connect/messaging-relay/list"));
	Request->SetHeader(TEXT("X-API-Key"), Settings->DeveloperApiKey);
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));

	UE_LOG(LogCtcConnect, Verbose, TEXT("RefreshTunnels requesting sessions list."));
	Request->ProcessRequestUntilComplete();

	FHttpResponsePtr Response = Request->GetResponse();

	if (!Response || !EHttpResponseCodes::IsOk(Response->GetResponseCode()))
	{
		const FString ResponseCode = Response ? LexToString(Response->GetResponseCode()) : TEXT("NoResponse");
		const FString ResponseBody = Response ? Response->GetContentAsString() : TEXT("NoResponseBody");
		UE_LOG(LogCtcConnect, Error, TEXT("RefreshTunnels request failed. ResponseCode=%s ResponseBody=%s"), *ResponseCode, *ResponseBody);
		return;
	}

	UE_LOG(LogCtcConnect, Verbose, TEXT("RefreshTunnels request completed."));

	TSharedPtr<FJsonObject> ResponseBody;
	FString InfoUploadResponseJson = Response->GetContentAsString();
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(InfoUploadResponseJson);
	if (!FJsonSerializer::Deserialize(Reader, ResponseBody) || !ResponseBody.IsValid())
	{
		UE_LOG(LogCtcConnect, Error, TEXT("RefreshTunnels response JSON is invalid. ResponseBody=%s"), *InfoUploadResponseJson);
		return;
	}

	TMap<FString, FString> ConnectionInfos;

	TArray<TSharedPtr<FJsonValue>> ConnectionsArray = ResponseBody->GetArrayField(TEXT("connections"));
	for (const TSharedPtr<FJsonValue>& Connection : ConnectionsArray)
	{
		TSharedPtr<FJsonObject> ConnectionObject = Connection->AsObject();

		FString ConnectIdentity = ConnectionObject->GetStringField(TEXT("connect_identity"));
		FString ConnectSecret = ConnectionObject->GetStringField(TEXT("connect_secret"));
		ConnectionInfos.Add(ConnectIdentity, ConnectSecret);
	}

	for (const TTuple<FString, FString>& Info : ConnectionInfos)
	{
		if (CliProcesses.Contains(Info.Key))
		{
			// Process is still in the list, no need to spawn a new tunnel.
			continue;
		}

		OpenTunnel(Info.Key, Info.Value);
	}

	for (const TTuple<FString, TUniquePtr<FMonitoredProcess>>& Process : CliProcesses)
	{
		if (ConnectionInfos.Contains(Process.Key))
		{
			// Process is still in the list, no need to clean it up.
			continue;
		}

		CloseTunnel(Process.Key);
	}
}

void UCtcMessagingRelayClientSubsystem::OpenTunnel(const FString& ConnectAddress, const FString& ConnectSecret)
{
	const FString ProcessName = FString::Printf(TEXT("MessagingRelayClient_%s"), *ConnectAddress);
	CastToCloudSharedHelpers::FSpawnCliArgs Args = { ProcessName, LogCtcConnect};

	const int32 Port = *CastToCloudSharedHelpers::GetFreeLocalPort();
	const FString BindAddress = FString::Printf(TEXT("127.0.0.1:%d"), Port);
	Args.CommandLine = FString::Printf(TEXT("direct-connect-client --connect-address %s --connect-secret %s --bind %s"), *ConnectAddress, *ConnectSecret, *BindAddress);

	INetworkMessagingExtension& Ext = IModularFeatures::Get().GetModularFeature<INetworkMessagingExtension>(INetworkMessagingExtension::ModularFeatureName);
	Ext.AddEndpoint(FString::Printf(TEXT("127.0.0.1:%d"), Port));

	UE_LOG(LogCtcConnect, Display, TEXT("Opening client tunnel. ConnectAddress=%s BindAddress=%s"), *ConnectAddress, *BindAddress);
	TUniquePtr<FMonitoredProcess> Process = CastToCloudSharedHelpers::SpawnCliProcess(Args);
	CliProcesses.Add(ConnectAddress, MoveTemp(Process));
}

void UCtcMessagingRelayClientSubsystem::CloseTunnel(const FString& ConnectAddress)
{
	UE_LOG(LogCtcConnect, Display, TEXT("Closing tunnel. ConnectAddress=%s"), *ConnectAddress);
	CliProcesses.Remove(ConnectAddress);
}
