// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAnalyticsLocalPlayerTrackerSubsystem.h"

#include <Engine/GameInstance.h>
#include <Engine/LocalPlayer.h>
#include <Engine/World.h>
#include <Framework/Application/SlateApplication.h>
#include <GameFramework/Pawn.h>
#include <HAL/Platform.h>
#include <Null/NullPlatformApplicationMisc.h>
#include <UObject/UObjectHash.h>

#include "CtcAnalyticsBPFL.h"
#include "CtcAnalyticsLog.h"
#include "CtcHelpers.h"
#include "CtcSharedSettings.h"

extern ENGINE_API float GAverageFPS;

void UCtcAnalyticsLocalPlayerTrackerSubsystem::SetMovementTracking(bool bEnabled)
{
	TrackMovementEnabled = bEnabled;
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	RegisterApplicationEvents();

	RegisterSessionDelegates();

	FWorldDelegates::OnStartGameInstance.AddUObject(this, &UCtcAnalyticsLocalPlayerTrackerSubsystem::OnStartGameInstance);

	// NOTE: There is no FWorldDelegates::BeginPlay so we register for world creation and then bind to the World's BeginPlay delegate.
	FWorldDelegates::OnPostWorldInitialization.AddLambda(
		[this](UWorld* World, FWorldInitializationValues WorldInitializationValues)
		{
			World->OnWorldBeginPlay.AddUObject(this, &UCtcAnalyticsLocalPlayerTrackerSubsystem::OnWorldBeginPlay, World);
		}
	);

	// NOTE: There is no FWorldDelegates::EndPlay, but OnWorldBeginTearDown is called during UWorld::EndPlay.
	FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &UCtcAnalyticsLocalPlayerTrackerSubsystem::OnWorldEndPlay);

	FCoreDelegates::OnHandleSystemError.AddUObject(this, &UCtcAnalyticsLocalPlayerTrackerSubsystem::OnSystemError);
	FCoreDelegates::GetApplicationWillTerminateDelegate().AddUObject(this, &UCtcAnalyticsLocalPlayerTrackerSubsystem::OnApplicationWillTerminate);
}

bool UCtcAnalyticsLocalPlayerTrackerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (IsRunningDedicatedServer())
	{
		return false;
	}

	TArray<UClass*> ChildClasses;
	GetDerivedClasses(GetClass(), ChildClasses, false);
	if (ChildClasses.Num() > 0)
	{
		return false;
	}

	return Super::ShouldCreateSubsystem(Outer);
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::Deinitialize()
{
	Super::Deinitialize();

	UnregisterApplicationEvents();

	UnregisterSessionDelegates();
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::Tick(float DeltaTime)
{
	TickMovementTracking(DeltaTime);
}

ETickableTickType UCtcAnalyticsLocalPlayerTrackerSubsystem::GetTickableTickType() const
{
	const bool bIsCDO = HasAnyFlags(RF_ClassDefaultObject);
	return !bIsCDO ? ETickableTickType::Always : ETickableTickType::Never;
}

TStatId UCtcAnalyticsLocalPlayerTrackerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCtcAnalyticsAutoTrackerSubsystem, STATGROUP_Tickables);
}

UWorld* UCtcAnalyticsLocalPlayerTrackerSubsystem::GetTickableGameObjectWorld() const
{
	return CastToCloudHelpers::GetCurrentWorld();
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::RegisterSessionDelegates()
{
	FCoreDelegates::OnPostEngineInit.AddUObject(this, &UCtcAnalyticsLocalPlayerTrackerSubsystem::OnSessionStart);
	FCoreDelegates::OnEnginePreExit.AddUObject(this, &UCtcAnalyticsLocalPlayerTrackerSubsystem::OnSessionEnd);
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::UnregisterSessionDelegates()
{
	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
	FCoreDelegates::OnEnginePreExit.RemoveAll(this);
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::RegisterApplicationEvents()
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}
	if (FNullPlatformApplicationMisc::IsUsingNullApplication())
	{
		return;
	}

#if PLATFORM_WINDOWS
	WindowsMessageHandler = TUniquePtr<FCtcWindowsMessageHandler>(new FCtcWindowsMessageHandler);
	WindowsMessageHandler->OnAltF4Pressed.AddUObject(this, &UCtcAnalyticsLocalPlayerTrackerSubsystem::OnWindowsAltF4Pressed);
	const TSharedPtr<GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication();
	if (TSharedPtr<FWindowsApplication> WindowsApplication = StaticCastSharedPtr<FWindowsApplication>(PlatformApplication))
	{
		WindowsApplication->AddMessageHandler(*WindowsMessageHandler);
	}
#endif
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::UnregisterApplicationEvents()
{
	if (!FSlateApplication::IsInitialized())
	{
		return;
	}
	if (FNullPlatformApplicationMisc::IsUsingNullApplication())
	{
		return;
	}

#if PLATFORM_WINDOWS
	const TSharedPtr<GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication();
	if (TSharedPtr<FWindowsApplication> WindowsApplication = StaticCastSharedPtr<FWindowsApplication>(PlatformApplication))
	{
		WindowsApplication->RemoveMessageHandler(*WindowsMessageHandler);
	}
	WindowsMessageHandler.Reset();
#endif
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::OnSessionStart()
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (!Settings->bAutoStartSession)
	{
		return;
	}

	UCtcAnalyticsBPFL::StartSession();
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::OnSessionEnd()
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (!Settings->bAutoStartSession)
	{
		return;
	}

	UCtcAnalyticsBPFL::EndSession();
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::OnSystemError()
{
	UE_LOG(LogCtcAnalytics, Verbose, TEXT("OnSystemError called. Sending cached events."));

	UCtcAnalyticsBPFL::RecordPanicEvent(TEXT("SystemError"), TArray<FAnalyticsEventAttribute>());
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::OnApplicationWillTerminate()
{
	UE_LOG(LogCtcAnalytics, Verbose, TEXT("OnApplicationWillTerminate called. Sending cached events."));

	UCtcAnalyticsBPFL::RecordPanicEvent(TEXT("ApplicationWillTerminate"), TArray<FAnalyticsEventAttribute>());
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::OnWorldBeginPlay(UWorld* World)
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (!Settings->bAutoWorldChangeTracking)
	{
		return;
	}

	const TOptional<FString> WorldPackage = CastToCloudHelpers::GetWorldPackage(World);
	if (WorldPackage.Get(TEXT("")).IsEmpty())
	{
		return;
	}

	if (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)
	{
		return;
	}

	UCtcAnalyticsBPFL::RecordEvent(TEXT("WorldStart"), TArray<FAnalyticsEventAttribute>());
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::OnWorldEndPlay(UWorld* World)
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (!Settings->bAutoWorldChangeTracking)
	{
		return;
	}

	const TOptional<FString> WorldPackage = CastToCloudHelpers::GetWorldPackage(World);
	if (WorldPackage.Get(TEXT("")).IsEmpty())
	{
		return;
	}

	if (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)
	{
		return;
	}

	UCtcAnalyticsBPFL::RecordEvent(TEXT("WorldEnd"), TArray<FAnalyticsEventAttribute>());
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::OnWindowsAltF4Pressed()
{
	TOptional<FTransform> PlayerTransform = {};

	if (const APlayerController* LocalController = CastToCloudHelpers::GetFirstLocalPlayerController())
	{
		if (const APawn* PlayerPawn = LocalController->GetPawn())
		{
			PlayerTransform = PlayerPawn->GetActorTransform();
		}
		else if (const APlayerCameraManager* CameraManager = LocalController->PlayerCameraManager)
		{
			// Fallback to camera in case the user ALT+F4 while unpossessed (e.g.: while dead)
			PlayerTransform = FTransform(CameraManager->GetCameraRotation(), CameraManager->GetCameraLocation());
		}
	}

	UCtcAnalyticsBPFL::RecordEventWithOptionalTransform(TEXT("ALT+F4 Pressed"), PlayerTransform, {});
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::OnStartGameInstance(UGameInstance* GameInstance)
{
	GameInstance->OnLocalPlayerAddedEvent.AddUObject(this, &UCtcAnalyticsLocalPlayerTrackerSubsystem::OnLocalPlayerAdded);
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::OnLocalPlayerAdded(ULocalPlayer* LocalPlayer)
{
	if (LocalPlayer)
	{
		LocalPlayer->OnPlayerControllerChanged().AddUObject(this, &UCtcAnalyticsLocalPlayerTrackerSubsystem::OnLocalPlayerReceivedPlayerController);
	}
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::OnLocalPlayerReceivedPlayerController(APlayerController* PlayerController)
{
	if (PlayerController)
	{
		PlayerController->OnPossessedPawnChanged.AddDynamic(this, &UCtcAnalyticsLocalPlayerTrackerSubsystem::OnPossessedPawnChanged);
	}
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
{
	const FString EventType = [OldPawn, NewPawn]()
	{
		if (OldPawn == nullptr && NewPawn != nullptr)
		{
			return TEXT("PlayerPawnPossess Begin");
		}
		if (OldPawn != nullptr && NewPawn == nullptr)
		{
			return TEXT("PlayerPawnPossess End");
		}
		if (OldPawn != nullptr && NewPawn != nullptr)
		{
			return TEXT("PlayerPawnPossess Switch");
		}

		return TEXT("PlayerPawnPossess Unknown");
	}();

	const APawn* RelevantPawn = NewPawn ? NewPawn : OldPawn;
	const TOptional<FTransform> RelevantTransform = RelevantPawn ? RelevantPawn->GetActorTransform() : TOptional<FTransform>();

	UCtcAnalyticsBPFL::RecordEventWithOptionalTransform(EventType, RelevantTransform, {});
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::TickMovementTracking(float DeltaTime)
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	const bool bShouldTrack = TrackMovementEnabled.IsSet() ? *TrackMovementEnabled : Settings->bAutoPlayerMoveTracking;
	if (!bShouldTrack)
	{
		return;
	}

	FpsTracker.AddEntry(GAverageFPS);

	TrackMovementInterval.Tick(DeltaTime);
	if (!TrackMovementInterval.HasFinished())
	{
		return;
	}

	const APlayerController* LocalController = CastToCloudHelpers::GetFirstLocalPlayerController();
	if (!LocalController)
	{
		return;
	}

	TOptional<FTransform> AutomatedTransform;

	if (Settings->AutoPlayerMoveTrackingMethod == ECtcAnalyticsSpatialTracking::PlayerPawn)
	{
		if (const APawn* PlayerPawn = LocalController->GetPawn())
		{
			AutomatedTransform = PlayerPawn->GetActorTransform();
		}
	}
	else if (Settings->AutoPlayerMoveTrackingMethod == ECtcAnalyticsSpatialTracking::Camera)
	{
		if (const APlayerCameraManager* CameraManager = LocalController->PlayerCameraManager)
		{
			AutomatedTransform = FTransform(CameraManager->GetCameraRotation(), CameraManager->GetCameraLocation());
		}
	}

	if (AutomatedTransform.IsSet())
	{
		TArray<FAnalyticsEventAttribute> Attributes;
		if (FpsTracker.HasData())
		{
			const double AverageFps = FpsTracker.GetAverage();
			Attributes.Add(FAnalyticsEventAttribute(TEXT("fps"), FString::Printf(TEXT("%.1f"), AverageFps)));
			FpsTracker.Reset();
		}

		UCtcAnalyticsBPFL::RecordEventWithTransform(TEXT("PlayerMove"), *AutomatedTransform, Attributes);
	}

	TrackMovementInterval.Reset(Settings->AutoPlayerMoveTrackingInterval);
}
