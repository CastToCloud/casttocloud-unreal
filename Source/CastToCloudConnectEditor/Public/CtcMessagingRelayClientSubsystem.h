// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Subsystems/EngineSubsystem.h>
#include <Tickable.h>

#include <Misc/MonitoredProcess.h>

#include "CtcMessagingRelayClientSubsystem.generated.h"

UCLASS()
class CASTTOCLOUDCONNECTEDITOR_API UCtcMessagingRelayClientSubsystem : public UEngineSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

	// ~Begin UEngineSubsystem interface
	virtual void Deinitialize() override;
	// ~End UEngineSubsystem interface

	// ~Begin FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickableInEditor() const override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	// ~End FTickableGameObject interface

	void RefreshTunnelsAsync();

	void OpenTunnel(const FString& ConnectAddress, const FString& ConnectSecret);
	void CloseTunnel(const FString& ConnectAddress);

	TMap<FString, TUniquePtr<FMonitoredProcess>> CliProcesses;
	bool bRefreshInProgress = false;
};
