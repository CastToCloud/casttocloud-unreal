// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcMessagingRelayServerSubsystem.h"

#include <INetworkMessagingExtension.h>
#include <Features/IModularFeatures.h>
#include <Shared/UdpMessagingSettings.h>
#include <Misc/CommandLine.h>

#include "CtcConnectLog.h"
#include "CtcSharedHelpers.h"
#include "CtcSharedSettings.h"

namespace
{
	bool IsMessagingRelayEnabled()
	{
		const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
		const bool bAllowAnyConfiguration = FParse::Param(FCommandLine::Get(), TEXT("MessagingRelayAnyConfiguration"));
		return Settings->MessagingRelayEnabled.IsCurrentConfigurationAllowed() || bAllowAnyConfiguration;
	}
}

static FDelayedAutoRegisterHelper MessagingRelayServerObjectSystemReady(
	EDelayedRegisterRunPhase::ObjectSystemReady,
	[]
	{
		
		if (IsMessagingRelayEnabled())
		{
			UUdpMessagingSettings* UdpSettings = GetMutableDefault<UUdpMessagingSettings>();
			UdpSettings->EnabledByDefault = true;

			const TOptional<int> PortToUse = CastToCloudSharedHelpers::GetFreeLocalPort();
			UdpSettings->UnicastEndpoint = FString::Printf(TEXT("127.0.0.1:%d"), *PortToUse);
		}
	}
);

void UCtcMessagingRelayServerSubsystem::Deinitialize()
{
	CliProcess.Reset();
}

void UCtcMessagingRelayServerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	
	if (IsMessagingRelayEnabled())
	{
		CastToCloudSharedHelpers::FSpawnCliArgs Args = {TEXT("MessagingRelayServer"), LogCtcConnect};

		const UCtcSharedSettings* SharedSettings = GetDefault<UCtcSharedSettings>();
		const FString ApiUrl = SharedSettings->ApiUrl;
		const FString ApiKey = SharedSettings->RuntimeApiKey.ApiKey;

		const UUdpMessagingSettings* UdpMessagingSettings = GetDefault<UUdpMessagingSettings>();
		const FString ForwardAddress = UdpMessagingSettings->UnicastEndpoint;

		Args.CommandLine = FString::Printf(TEXT("direct-connect-server --api-key %s --api-url %s --register-path direct-connect/messaging-relay/register --forward %s"), *ApiKey, *ApiUrl, *ForwardAddress);

		UE_LOG(LogCtcConnect, Display, TEXT("Opening server tunnel. ApiUrl=%s ForwardAddress=%s"), *ApiUrl, *ForwardAddress);
		CliProcess = CastToCloudSharedHelpers::SpawnCliProcess(Args);
	}
}
