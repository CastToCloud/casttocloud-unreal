// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcSharedHelpers.h"

#include <Engine/GameEngine.h>
#include <Engine/LocalPlayer.h>
#include <GameFramework/Pawn.h>
#include <HAL/FileManager.h>
#include <Sockets.h>
#include <SocketSubsystem.h>
#include <Misc/CommandLine.h>
#include <Misc/ConfigCacheIni.h>
#include <Misc/ConfigContext.h>
#include <UObject/Package.h>
#include <Interfaces/IPluginManager.h>
#include <Misc/Paths.h>
#include <Misc/MonitoredProcess.h>

#if WITH_EDITOR
#include <Editor.h>
#endif

TOptional<FString> GetCliPath()
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("CastToCloud"));
	if (!Plugin.IsValid())
	{
		return {};
	}

#if PLATFORM_WINDOWS
	const FString ExpectedPath = Plugin->GetBaseDir() / TEXT("Binaries") / TEXT("Win64") / TEXT("casttocloud-cli.exe");
#elif PLATFORM_MAC
	const FString ExpectedPath = Plugin->GetBaseDir() / TEXT("Binaries") / TEXT("Mac") / TEXT("casttocloud-cli"));
#elif PLATFORM_LINUX
	const FString ExpectedPath = Plugin->GetBaseDir() / TEXT("Binaries") / TEXT("Mac") / TEXT("casttocloud-cli"));
#else
	const FString ExpectedPath = TEXT("");
#endif
	
	if (!FPaths::FileExists(ExpectedPath))
	{
		return {};
	}

	return ExpectedPath;
}

UWorld* CastToCloudSharedHelpers::GetCurrentWorld()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		if (FWorldContext* PIEWorldContext = GEditor->GetPIEWorldContext())
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

TOptional<FString> CastToCloudSharedHelpers::GetWorldPackage(const UWorld* World)
{
	const UWorld* Target = World ? World : GetCurrentWorld();

	if (!Target || !Target->GetPackage())
	{
		return {};
	}

	const FString CleanPackageName = UWorld::StripPIEPrefixFromPackageName(Target->GetPackage()->GetName(), Target->StreamingLevelsPrefix);
	if (CleanPackageName.StartsWith(TEXT("/Temp/Untitled")))
	{
		return {};
	}

	return CleanPackageName;
}

APlayerController* CastToCloudSharedHelpers::GetFirstLocalPlayerController(const UWorld* World)
{
	const UWorld* Target = World ? World : GetCurrentWorld();

	if (!Target)
	{
		return nullptr;
	}

	const ULocalPlayer* LocalPlayer = Target->GetFirstLocalPlayerFromController();
	if (!LocalPlayer)
	{
		return nullptr;
	}

	return LocalPlayer->PlayerController;
}

TValueOrError<FConfigFile, FString> CastToCloudSharedHelpers::GetPreInitConfig()
{
	// NOTE: This is basically an inline re-implementation of FTempCommandLineScope
	const bool bWasCommandLineInitialized = FCommandLine::IsInitialized();
	if (!bWasCommandLineInitialized)
	{
		FCommandLine::Set(TEXT(""));
	}

	ON_SCOPE_EXIT
	{
		if (!bWasCommandLineInitialized)
		{
			FCommandLine::Reset();
		}
	};

	const FString PreInitIni = FPaths::ProjectIntermediateDir() / TEXT("CastToCloud") / TEXT("PreInitCastToCloud.ini");
	if (!IFileManager::Get().FileExists(*PreInitIni))
	{
		const FString MissingFileError = FString::Printf(TEXT("PreInitIni not found at %s"), *PreInitIni);
		return MakeError(MissingFileError);
	}

	FConfigFile Ini;
	FConfigContext::ReadSingleIntoLocalFile(Ini).Load(*PreInitIni);

	return MakeValue(Ini);
}

CastToCloudSharedHelpers::FGetAutoTransformContextDelegate& CastToCloudSharedHelpers::GetAutoTransformContextDelegate()
{
	static FGetAutoTransformContextDelegate Delegate;
	return Delegate;
}

TOptional<FTransform> CastToCloudSharedHelpers::GetAutoTransformContext()
{
	FGetAutoTransformContextDelegate& Delegate = GetAutoTransformContextDelegate();
	if (Delegate.IsBound())
	{
		return Delegate.Execute();
	}

	return GetAutoTransformContextFromPlayer();
}

TOptional<FTransform> CastToCloudSharedHelpers::GetAutoTransformContextFromPlayer()
{
	const APlayerController* LocalController = GetFirstLocalPlayerController();
	if (!LocalController)
	{
		return {};
	}

	if (const APawn* PlayerPawn = LocalController->GetPawn())
	{
		return PlayerPawn->GetActorTransform();
	}

	if (const APlayerCameraManager* CameraManager = LocalController->PlayerCameraManager)
	{
		return FTransform(CameraManager->GetCameraRotation(), CameraManager->GetCameraLocation());
	}

	return {};
}

TUniquePtr<FMonitoredProcess> CastToCloudSharedHelpers::SpawnCliProcess(const FSpawnCliArgs& Args)
{
	UE_LOG_REF(Args.LogCategory, Verbose, TEXT("CLI starting for %s with args: %s"), *Args.ProcessName, *Args.CommandLine);

	TOptional<FString> CliPath = GetCliPath();
	if (!CliPath)
	{
		UE_LOG_REF(Args.LogCategory, Warning, TEXT("CLI doesn't have a valid path"));
		return nullptr;
	}

	TUniquePtr<FMonitoredProcess> Process = MakeUnique<FMonitoredProcess>(*CliPath, Args.CommandLine, /*bInHidden=*/ true);
	const bool bLaunchSuccess = Process->Launch();

	if (!bLaunchSuccess)
	{
		UE_LOG_REF(Args.LogCategory, Warning, TEXT("CLI failed to launch"));
		return nullptr;
	}

	Process->OnOutput().BindLambda([Args](const FString& Line)
		{
			UE_LOG_REF(Args.LogCategory, Verbose, TEXT("[%s] %s"), *Args.ProcessName, *Line);
		});

	return Process;
}

TOptional<int32> CastToCloudSharedHelpers::GetFreeLocalPort()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	FSocket* Socket = SocketSubsystem ? SocketSubsystem->CreateSocket(NAME_Stream, TEXT("CtcPortProbe"), false) : nullptr;
	if (!Socket)
	{
		return {};
	}

	ON_SCOPE_EXIT 
	{
		Socket->Close();
		SocketSubsystem->DestroySocket(Socket);
	};

	TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();
	Addr->SetLoopbackAddress();
	Addr->SetPort(0);

	if (Socket->Bind(*Addr))
	{
		Socket->GetAddress(*Addr);
		return Addr->GetPort();
	}

	return {};
}
