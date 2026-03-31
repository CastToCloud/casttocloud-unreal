// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Subsystems/EngineSubsystem.h>

#include <Misc/MonitoredProcess.h>

#include "CtcMessagingRelayServerSubsystem.generated.h"

class FMonitoredProcess;

UCLASS()
class CASTTOCLOUDCONNECT_API UCtcMessagingRelayServerSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

	// ~Begin UEngineSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// ~End UEngineSubsystem interface

	TUniquePtr<FMonitoredProcess> CliProcess;
};
