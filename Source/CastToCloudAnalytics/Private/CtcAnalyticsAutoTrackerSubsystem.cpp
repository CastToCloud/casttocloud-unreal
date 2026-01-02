// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAnalyticsAutoTrackerSubsystem.h"

#include <Engine/GameEngine.h>
#include <Engine/GameInstance.h>
#include <Engine/LocalPlayer.h>
#include <Engine/World.h>
#include <Framework/Application/SlateApplication.h>
#include <GameFramework/Pawn.h>
#include <Kismet/GameplayStatics.h>
#include <Null/NullPlatformApplicationMisc.h>

#if WITH_EDITOR
#include <Editor.h>
#endif

#include "CtcAnalyticsBPFL.h"
#include "CtcAnalyticsLog.h"
#include "CtcSharedSettings.h"

namespace
{
	UWorld* GetCurrentWorld()
	{
#if WITH_EDITOR
		if (GIsEditor)
		{
			FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext();
			if (PIEWorldContext)
			{
				return PIEWorldContext->World();
			}

			return GEditor->GetEditorWorldContext().World();
		}
#endif
		if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
		{
			return GameEngine->GetGameWorld();
		}

		return nullptr;
	}

} // namespace

void UCtcAnalyticsAutoTrackerSubsystem::SetPlayerMovementTracking(bool bEnabled)
{
	SendPlayerMoveEnabled = bEnabled;
}

void UCtcAnalyticsAutoTrackerSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	RegisterApplicationEvents();

	RegisterSessionDelegates();

	FWorldDelegates::OnStartGameInstance.AddUObject(this, &UCtcAnalyticsAutoTrackerSubsystem::OnStartGameInstance);

	// NOTE: There is no FWorldDelegates::BeginPlay so we register for world creation and then bind to the World's BeginPlay delegate.
	FWorldDelegates::OnPostWorldInitialization.AddLambda(
		[this](UWorld* World, FWorldInitializationValues WorldInitializationValues)
		{
			World->OnWorldBeginPlay.AddUObject(this, &UCtcAnalyticsAutoTrackerSubsystem::OnWorldBeginPlay, World);
		}
	);

	// NOTE: There is no FWorldDelegates::EndPlay, but OnWorldBeginTearDown is called during UWorld::EndPlay.
	FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &UCtcAnalyticsAutoTrackerSubsystem::OnWorldEndPlay);
}

void UCtcAnalyticsAutoTrackerSubsystem::Deinitialize()
{
	Super::Deinitialize();

	UnregisterApplicationEvents();

	UnregisterSessionDelegates();
}

void UCtcAnalyticsAutoTrackerSubsystem::Tick(float DeltaTime)
{
	TickPlayerMoveTracking(DeltaTime);
}

ETickableTickType UCtcAnalyticsAutoTrackerSubsystem::GetTickableTickType() const
{
	const bool bIsCDO = HasAnyFlags(RF_ClassDefaultObject);
	return !bIsCDO ? ETickableTickType::Always : ETickableTickType::Never;
}

TStatId UCtcAnalyticsAutoTrackerSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UCtcAnalyticsAutoTrackerSubsystem, STATGROUP_Tickables);
}

UWorld* UCtcAnalyticsAutoTrackerSubsystem::GetTickableGameObjectWorld() const
{
	return GetWorld();
}

void UCtcAnalyticsAutoTrackerSubsystem::RegisterApplicationEvents()
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
	WindowsMessageHandler->OnAltF4Pressed.AddUObject(this, &UCtcAnalyticsAutoTrackerSubsystem::OnWindowsAltF4Pressed);
	const TSharedPtr<GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication();
	if (TSharedPtr<FWindowsApplication> WindowsApplication = StaticCastSharedPtr<FWindowsApplication>(PlatformApplication))
	{
		WindowsApplication->AddMessageHandler(*WindowsMessageHandler);
	}
#endif
}

void UCtcAnalyticsAutoTrackerSubsystem::UnregisterApplicationEvents()
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

void UCtcAnalyticsAutoTrackerSubsystem::RegisterSessionDelegates()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		FEditorDelegates::StartPIE.AddUObject(this, &UCtcAnalyticsAutoTrackerSubsystem::OnPIEStarted);
		FEditorDelegates::ShutdownPIE.AddUObject(this, &UCtcAnalyticsAutoTrackerSubsystem::OnPIEEnded);

		return;
	}
#endif

	FCoreDelegates::OnPostEngineInit.AddUObject(this, &UCtcAnalyticsAutoTrackerSubsystem::OnPostEngineInit);
	FCoreDelegates::OnEnginePreExit.AddUObject(this, &UCtcAnalyticsAutoTrackerSubsystem::OnEnginePreExit);

	FCoreDelegates::OnHandleSystemError.AddUObject(this, &UCtcAnalyticsAutoTrackerSubsystem::OnSystemError);
	FCoreDelegates::GetApplicationWillTerminateDelegate().AddUObject(this, &UCtcAnalyticsAutoTrackerSubsystem::OnApplicationWillTerminate);
}

void UCtcAnalyticsAutoTrackerSubsystem::UnregisterSessionDelegates()
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
void UCtcAnalyticsAutoTrackerSubsystem::OnPIEStarted(bool bIsSimulating)
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
void UCtcAnalyticsAutoTrackerSubsystem::OnPIEEnded(bool bIsSimulating)
{
	UE_LOG(LogCtcAnalytics, Verbose, TEXT("OnPIEEnded called. Ending session and sending cached events."));

	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (!Settings->bAutoStartSession)
	{
		return;
	}

	UCtcAnalyticsBPFL::EndSession();
}
#endif

void UCtcAnalyticsAutoTrackerSubsystem::OnPostEngineInit()
{
	UE_LOG(LogCtcAnalytics, Verbose, TEXT("OnPostEngineInit called. Starting session."));

	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (!Settings->bAutoStartSession)
	{
		return;
	}

	UCtcAnalyticsBPFL::StartSession();
}

void UCtcAnalyticsAutoTrackerSubsystem::OnEnginePreExit()
{
	UE_LOG(LogCtcAnalytics, Verbose, TEXT("OnEnginePreExit called. Ending session and sending cached events."));

	UCtcAnalyticsBPFL::RecordEvent(TEXT("EngineExit"), TArray<FAnalyticsEventAttribute>());

	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (!Settings->bAutoStartSession)
	{
		return;
	}

	UCtcAnalyticsBPFL::EndSession();
}

void UCtcAnalyticsAutoTrackerSubsystem::OnSystemError()
{
	UE_LOG(LogCtcAnalytics, Verbose, TEXT("OnSystemError called. Sending cached events."));

	UCtcAnalyticsBPFL::RecordPanicEvent(TEXT("SystemError"), TArray<FAnalyticsEventAttribute>());
}

void UCtcAnalyticsAutoTrackerSubsystem::OnApplicationWillTerminate()
{
	UE_LOG(LogCtcAnalytics, Verbose, TEXT("OnApplicationWillTerminate called. Sending cached events."));

	UCtcAnalyticsBPFL::RecordPanicEvent(TEXT("ApplicationWillTerminate"), TArray<FAnalyticsEventAttribute>());
}

void UCtcAnalyticsAutoTrackerSubsystem::OnWorldBeginPlay(UWorld* World)
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (!Settings->bAutoWorldChangeTracking)
	{
		return;
	}

	const FString WorldPath = World->GetOutermost()->GetPathName();
	if (WorldPath.IsEmpty() || WorldPath.StartsWith(TEXT("/Temp/Untitled")))
	{
		return;
	}

	if (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)
	{
		return;
	}

	UCtcAnalyticsBPFL::RecordEvent(TEXT("WorldStart"), TArray<FAnalyticsEventAttribute>());
}

void UCtcAnalyticsAutoTrackerSubsystem::OnWorldEndPlay(UWorld* World)
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	if (!Settings->bAutoWorldChangeTracking)
	{
		return;
	}

	const FString WorldPath = World->GetOutermost()->GetPathName();
	if (WorldPath.IsEmpty() || WorldPath.StartsWith(TEXT("/Temp/Untitled")))
	{
		return;
	}

	if (World->WorldType != EWorldType::Game && World->WorldType != EWorldType::PIE)
	{
		return;
	}

	UCtcAnalyticsBPFL::RecordEvent(TEXT("WorldEnd"), TArray<FAnalyticsEventAttribute>());
}

void UCtcAnalyticsAutoTrackerSubsystem::OnWindowsAltF4Pressed()
{
	TOptional<FTransform> PlayerTransform = {};
	if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		PlayerTransform = PlayerPawn->GetActorTransform();
	}
	else if (const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
	{
		// Fallback to camera in case the user ALT+F4 while unpossessed (e.g.: while dead)
		PlayerTransform = FTransform(CameraManager->GetCameraRotation(), CameraManager->GetCameraLocation());
	}

	UCtcAnalyticsBPFL::RecordEventWithOptionalTransform(TEXT("ALT+F4 Pressed"), PlayerTransform, {});
}

void UCtcAnalyticsAutoTrackerSubsystem::OnStartGameInstance(UGameInstance* GameInstance)
{
	GameInstance->OnLocalPlayerAddedEvent.AddUObject(this, &UCtcAnalyticsAutoTrackerSubsystem::OnLocalPlayerAdded);
}

void UCtcAnalyticsAutoTrackerSubsystem::OnLocalPlayerAdded(ULocalPlayer* LocalPlayer)
{
	if (LocalPlayer)
	{
		LocalPlayer->OnPlayerControllerChanged().AddUObject(this, &UCtcAnalyticsAutoTrackerSubsystem::OnLocalPlayerReceivedPlayerController);
	}
}

void UCtcAnalyticsAutoTrackerSubsystem::OnLocalPlayerReceivedPlayerController(APlayerController* PlayerController)
{
	if (PlayerController)
	{
		PlayerController->OnPossessedPawnChanged.AddDynamic(this, &UCtcAnalyticsAutoTrackerSubsystem::OnPossessedPawnChanged);
	}
}

void UCtcAnalyticsAutoTrackerSubsystem::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
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

void UCtcAnalyticsAutoTrackerSubsystem::TickPlayerMoveTracking(float DeltaTime)
{
	const UCtcSharedSettings* Settings = GetDefault<UCtcSharedSettings>();
	const bool bShouldTrack = SendPlayerMoveEnabled.Get(false) || Settings->bAutoPlayerMoveTracking;
	if (!bShouldTrack)
	{
		return;
	}

	SendPlayerMoveInterval.Tick(DeltaTime);
	if (!SendPlayerMoveInterval.HasFinished())
	{
		return;
	}

	TOptional<FTransform> AutomatedTransform;

	if (Settings->AutoPlayerMoveTrackingMethod == ECtcAnalyticsSpatialTracking::PlayerPawn)
	{
		if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetCurrentWorld(), 0))
		{
			AutomatedTransform = PlayerPawn->GetActorTransform();
		}
	}
	else if (Settings->AutoPlayerMoveTrackingMethod == ECtcAnalyticsSpatialTracking::Camera)
	{
		if (const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(GetCurrentWorld(), 0))
		{
			AutomatedTransform = FTransform(CameraManager->GetCameraRotation(), CameraManager->GetCameraLocation());
		}
	}

	if (AutomatedTransform.IsSet())
	{
		UCtcAnalyticsBPFL::RecordEventWithTransform(TEXT("PlayerMove"), *AutomatedTransform);
	}

	SendPlayerMoveInterval.Reset(Settings->AutoPlayerMoveTrackingInterval);
}
