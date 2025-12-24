// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcAnalyticsNotifierSubsystem.h"

#include <Engine/GameInstance.h>
#include <Engine/LocalPlayer.h>
#include <Engine/World.h>
#include <Framework/Application/SlateApplication.h>
#include <GameFramework/Pawn.h>
#include <Kismet/GameplayStatics.h>
#include <Null/NullPlatformApplicationMisc.h>

#include "CtcAnalyticsBPFL.h"

void UCtcAnalyticsNotifierSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	RegisterApplicationEvents();

	GetGameInstance()->OnLocalPlayerAddedEvent.AddUObject(this, &UCtcAnalyticsNotifierSubsystem::OnLocalPlayerAdded);
}

void UCtcAnalyticsNotifierSubsystem::Deinitialize()
{
	Super::Deinitialize();

	UnregisterApplicationEvents();
}

void UCtcAnalyticsNotifierSubsystem::RegisterApplicationEvents()
{
	if (FNullPlatformApplicationMisc::IsUsingNullApplication())
	{
		return;
	}

#if PLATFORM_WINDOWS
	WindowsMessageHandler = TUniquePtr<FCtcWindowsMessageHandler>(new FCtcWindowsMessageHandler);
	WindowsMessageHandler->OnAltF4Pressed.AddUObject(this, &UCtcAnalyticsNotifierSubsystem::OnWindowsAltF4Pressed);
	const TSharedPtr<GenericApplication> PlatformApplication = FSlateApplication::Get().GetPlatformApplication();
	if (TSharedPtr<FWindowsApplication> WindowsApplication = StaticCastSharedPtr<FWindowsApplication>(PlatformApplication))
	{
		WindowsApplication->AddMessageHandler(*WindowsMessageHandler);
	}
#endif
}

void UCtcAnalyticsNotifierSubsystem::UnregisterApplicationEvents()
{
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

void UCtcAnalyticsNotifierSubsystem::OnWindowsAltF4Pressed()
{
	TOptional<FVector> PlayerPosition = {};
	if (const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0))
	{
		PlayerPosition = PlayerPawn->GetActorLocation();
	}
	else if (const APlayerCameraManager* CameraManager = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
	{
		// Fallback to camera position in case the user ALT+F4 while unpossessed (e.g.: while dead)
		PlayerPosition = CameraManager->GetCameraLocation();
	}

	UCtcAnalyticsBPFL::RecordEventWithPossibleLocation(TEXT("ALT+F4 Pressed"), PlayerPosition, {});
}

void UCtcAnalyticsNotifierSubsystem::OnLocalPlayerAdded(ULocalPlayer* LocalPlayer)
{
	if (LocalPlayer)
	{
		LocalPlayer->OnPlayerControllerChanged().AddUObject(this, &UCtcAnalyticsNotifierSubsystem::OnLocalPlayerReceivedPlayerController);
	}
}

void UCtcAnalyticsNotifierSubsystem::OnLocalPlayerReceivedPlayerController(APlayerController* PlayerController)
{
	if (PlayerController)
	{
		PlayerController->OnPossessedPawnChanged.AddDynamic(this, &UCtcAnalyticsNotifierSubsystem::OnPossessedPawnChanged);
	}
}

void UCtcAnalyticsNotifierSubsystem::OnPossessedPawnChanged(APawn* OldPawn, APawn* NewPawn)
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
	const TOptional<FVector> RelevantPosition = RelevantPawn ? RelevantPawn->GetActorLocation() : TOptional<FVector>();

	UCtcAnalyticsBPFL::RecordEventWithPossibleLocation(EventType, RelevantPosition, {});
}
