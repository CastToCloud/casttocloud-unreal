// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAnalyticsLocalPlayerTrackerSubsystem.h"

#include <Camera/PlayerCameraManager.h>
#include <Engine/GameInstance.h>
#include <Engine/LocalPlayer.h>
#include <Engine/World.h>
#include <Framework/Application/SlateApplication.h>
#include <GameFramework/Pawn.h>
#include <GameFramework/PlayerController.h>
#include <HAL/Platform.h>
#include <Null/NullPlatformApplicationMisc.h>

#if WITH_EDITOR
#include <Editor.h>
#endif

#include "CtcAnalyticsBPFL.h"
#include "CtcAnalyticsLog.h"
#include "CtcSharedHelpers.h"
#include "CtcSharedSettings.h"

extern ENGINE_API float GAverageFPS;

UCtcAnalyticsLocalPlayerTrackerSubsystem::~UCtcAnalyticsLocalPlayerTrackerSubsystem() = default;

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
}

bool UCtcAnalyticsLocalPlayerTrackerSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (IsRunningDedicatedServer())
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
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCtcAnalyticsLocalPlayerTrackerSubsystem, STATGROUP_Tickables);
}

UWorld* UCtcAnalyticsLocalPlayerTrackerSubsystem::GetTickableGameObjectWorld() const
{
	return CastToCloudSharedHelpers::GetCurrentWorld();
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

void UCtcAnalyticsLocalPlayerTrackerSubsystem::RegisterSessionDelegates()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		FEditorDelegates::StartPIE.AddUObject(this, &UCtcAnalyticsLocalPlayerTrackerSubsystem::OnPIEStarted);
		FEditorDelegates::ShutdownPIE.AddUObject(this, &UCtcAnalyticsLocalPlayerTrackerSubsystem::OnPIEEnded);

		return;
	}
#endif

	FCoreDelegates::OnPostEngineInit.AddUObject(this, &UCtcAnalyticsLocalPlayerTrackerSubsystem::OnPostEngineInit);
	FCoreDelegates::OnEnginePreExit.AddUObject(this, &UCtcAnalyticsLocalPlayerTrackerSubsystem::OnEnginePreExit);

	FCoreDelegates::OnHandleSystemError.AddUObject(this, &UCtcAnalyticsLocalPlayerTrackerSubsystem::OnSystemError);
	FCoreDelegates::GetApplicationWillTerminateDelegate().AddUObject(this, &UCtcAnalyticsLocalPlayerTrackerSubsystem::OnApplicationWillTerminate);
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::UnregisterSessionDelegates()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		FEditorDelegates::StartPIE.RemoveAll(this);
		FEditorDelegates::ShutdownPIE.RemoveAll(this);

		return;
	}
#endif

	FCoreDelegates::OnPostEngineInit.RemoveAll(this);
	FCoreDelegates::OnEnginePreExit.RemoveAll(this);

	FCoreDelegates::OnHandleSystemError.RemoveAll(this);
	FCoreDelegates::GetApplicationWillTerminateDelegate().RemoveAll(this);
}

#if WITH_EDITOR
void UCtcAnalyticsLocalPlayerTrackerSubsystem::OnPIEStarted(bool bIsSimulating)
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (!Settings->bAutoStartSession)
	{
		return;
	}

	UCtcAnalyticsBPFL::StartSession();
}
#endif

#if WITH_EDITOR
void UCtcAnalyticsLocalPlayerTrackerSubsystem::OnPIEEnded(bool bIsSimulating)
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (!Settings->bAutoStartSession)
	{
		return;
	}

	UCtcAnalyticsBPFL::EndSession();
}
#endif

void UCtcAnalyticsLocalPlayerTrackerSubsystem::OnPostEngineInit()
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (!Settings->bAutoStartSession)
	{
		return;
	}

	UCtcAnalyticsBPFL::StartSession();
}

void UCtcAnalyticsLocalPlayerTrackerSubsystem::OnEnginePreExit()
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

	const TOptional<FString> WorldPackage = CastToCloudSharedHelpers::GetWorldPackage(World);
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

	const TOptional<FString> WorldPackage = CastToCloudSharedHelpers::GetWorldPackage(World);
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

	if (const APlayerController* LocalController = CastToCloudSharedHelpers::GetFirstLocalPlayerController())
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

	const APlayerController* LocalController = CastToCloudSharedHelpers::GetFirstLocalPlayerController();
	if (!LocalController)
	{
		return;
	}

	TOptional<FTransform> AutomatedTransform;

	if (Settings->AutoPlayerMoveTrackingMethod == ECtcSharedAnalyticsSpatialTracking::PlayerPawn)
	{
		if (const APawn* PlayerPawn = LocalController->GetPawn())
		{
			AutomatedTransform = PlayerPawn->GetActorTransform();
		}
	}
	else if (Settings->AutoPlayerMoveTrackingMethod == ECtcSharedAnalyticsSpatialTracking::Camera)
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
