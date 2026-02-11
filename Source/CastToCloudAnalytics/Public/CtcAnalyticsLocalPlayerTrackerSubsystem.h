// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#pragma once

#include <Engine/GameInstance.h>
#include <HAL/Platform.h>
#include <Subsystems/EngineSubsystem.h>
#include <Tickable.h>

#if PLATFORM_WINDOWS
#include "CtcAnalyticsWindowsMessageHandler.h" //NOTE: Full include needed in header so TUniquePtr can access the destructor
#endif

#include "CtcAnalyticsLocalPlayerTrackerSubsystem.generated.h"

struct FIntervalTracker
{
	bool HasFinished() const { return TimeLeft < 0.0f; }
	void Tick(float DeltaTime) { TimeLeft -= DeltaTime; }
	void Reset(float FullTime) { TimeLeft = FullTime; }

	float TimeLeft = FLT_MIN;
};

struct FFpsTracker
{
	void AddEntry(double Fps) { Total += Fps; Counter++; }
	void Reset() { Total = 0.0; Counter = 0; }
	bool HasData() const { return Counter > 0; }
	double GetAverage() const { return Counter > 0 ? Total / Counter : 0.0; }

	int Counter = 0;
	double Total = 0.0;
};

UCLASS()
class CASTTOCLOUDANALYTICS_API UCtcAnalyticsLocalPlayerTrackerSubsystem : public UEngineSubsystem, public FTickableGameObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "CastToCloud|Analytics")
	void SetMovementTracking(bool bEnabled);

private:
	// ~Begin UEngineSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;
	// ~End UEngineSubsystem interface

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
	 * Registers delegates we should use to start & end the session
	 */
	void RegisterSessionDelegates();
	/**
	 * Registers delegates we should use to start & end the session
	 */
	void UnregisterSessionDelegates();

#if WITH_EDITOR
	/**
	 * Callback executed when the Play In Editor (PIE) session starts
	 */
	void OnPIEStarted(bool bIsSimulating);
#endif

#if WITH_EDITOR
	/**
	 * Callback executed when the Play In Editor (PIE) session ends
	 */
	void OnPIEEnded(bool bIsSimulating);
#endif
	/**
	 * Callback executed when the engine has started
	 */
	void OnPostEngineInit();
	/**
	 * Callback executed when the engine is about to exit
	 */
	void OnEnginePreExit();
	/**
	 * Callback executed when the system encounters an error (e.g.: a crash)
	 */
	void OnSystemError();
	/**
	 * Callback executed when the operating system tries to terminate the application
	 */
	void OnApplicationWillTerminate();
	/**
	 * Callback executed when a world's BeginPlay is executed
	 */
	void OnWorldBeginPlay(UWorld* World);
	/**
	 * Callback executed when a world's EndPlay is executed
	 */
	void OnWorldEndPlay(UWorld* World);
	/**
	 * Callback executed when a Windows user presses the Alt+F4 key combination.
	 */
	void OnWindowsAltF4Pressed();
	/**
	 * Callback executed when the GameInstance is started
	 */
	void OnStartGameInstance(UGameInstance* GameInstance);
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
	/**
	 * Called every frame to update the automated player move tracking
	 */
	void TickMovementTracking(float DeltaTime);

#if PLATFORM_WINDOWS
	TUniquePtr<FCtcWindowsMessageHandler> WindowsMessageHandler;
#endif

	FFpsTracker FpsTracker;
	FIntervalTracker TrackMovementInterval;
	TOptional<bool> TrackMovementEnabled;
};
