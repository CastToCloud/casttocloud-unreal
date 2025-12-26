// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Subsystems/GameInstanceSubsystem.h>

#include "CtcAnalyticsWindowsMessageHandler.h"

#include "CtcAnalyticsAutoTrackerSubsystem.generated.h"

UCLASS()
class CASTTOCLOUDANALYTICS_API UCtcAnalyticsAutoTrackerSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

	// ~Begin UGameInstanceSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	// ~End UGameInstanceSubsystem interface

	// FTickableGameObject interface
	virtual void Tick(float DeltaTime) override;
	virtual ETickableTickType GetTickableTickType() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override;
	// ~FTickableGameObject interface

	/**
	 * Registers the more complex application delegates when possible
	 */
	void RegisterApplicationEvents();
	/**
	 * Unregisters the application specific delegates we were able to bind
	 */
	void UnregisterApplicationEvents();
	/**
	 * Callback executed when a Windows user presses the Alt+F4 key combination.
	 */
	void OnWindowsAltF4Pressed();
	/**
	 * Callback executed when the local player is added
	 */
	void OnLocalPlayerAdded(ULocalPlayer* LocalPlayer);
	/**
	 * Callback executed when the local player receives a PlayerController
	 */
	void OnLocalPlayerReceivedPlayerController(APlayerController* PlayerController);
	/**
	 * Callback executed when the local player's PlayerController pawn possession changes
	 */
	UFUNCTION()
	void OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn);

#if PLATFORM_WINDOWS
	TUniquePtr<FCtcWindowsMessageHandler> WindowsMessageHandler;
#endif
};
