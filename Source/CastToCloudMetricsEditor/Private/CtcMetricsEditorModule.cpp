// Copyright Cast To Cloud 2024-2026. All Rights Reserved.

#include "CtcMetricsEditorModule.h"

#include "Commandlets/IChunkDataGenerator.h"
#include "Cooker/MPCollector.h"
#include "HAL/PlatformTime.h"
#include "Interfaces/ITargetPlatform.h"
#include "Templates/RefCounting.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "TickableEditorObject.h"
#include "UObject/ICookInfo.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogCastToCloudMetricsEditor, Log, All);

namespace
{
	constexpr double CookHeartbeatIntervalSeconds = 30.0;
	bool GCastToCloudChunkFactoryRegistered = false;

	const TCHAR* BoolToString(const bool bValue)
	{
		return bValue ? TEXT("true") : TEXT("false");
	}

	const TCHAR* CookTypeToString(const UE::Cook::ECookType CookType)
	{
		switch (CookType)
		{
		case UE::Cook::ECookType::Unknown:
			return TEXT("Unknown");
		case UE::Cook::ECookType::OnTheFly:
			return TEXT("OnTheFly");
		case UE::Cook::ECookType::ByTheBook:
			return TEXT("ByTheBook");
		default:
			return TEXT("Invalid");
		}
	}

	const TCHAR* CookingDlcToString(const UE::Cook::ECookingDLC CookingDlc)
	{
		switch (CookingDlc)
		{
		case UE::Cook::ECookingDLC::Unknown:
			return TEXT("Unknown");
		case UE::Cook::ECookingDLC::Yes:
			return TEXT("Yes");
		case UE::Cook::ECookingDLC::No:
			return TEXT("No");
		default:
			return TEXT("Invalid");
		}
	}

	const TCHAR* ProcessTypeToString(const UE::Cook::EProcessType ProcessType)
	{
		switch (ProcessType)
		{
		case UE::Cook::EProcessType::SingleProcess:
			return TEXT("SingleProcess");
		case UE::Cook::EProcessType::Director:
			return TEXT("Director");
		case UE::Cook::EProcessType::Worker:
			return TEXT("Worker");
		case UE::Cook::EProcessType::AllMPCook:
			return TEXT("AllMPCook");
		default:
			return TEXT("Invalid");
		}
	}

	const TCHAR* CookResultToString(const UE::Cook::ECookResult CookResult)
	{
		switch (CookResult)
		{
		case UE::Cook::ECookResult::NotAttempted:
			return TEXT("NotAttempted");
		case UE::Cook::ECookResult::Succeeded:
			return TEXT("Succeeded");
		case UE::Cook::ECookResult::Failed:
			return TEXT("Failed");
		case UE::Cook::ECookResult::NeverCookPlaceholder:
			return TEXT("NeverCookPlaceholder");
		case UE::Cook::ECookResult::Invalid:
			return TEXT("Invalid");
		case UE::Cook::ECookResult::Count:
			return TEXT("Count");
		default:
			return TEXT("Invalid");
		}
	}

	const TCHAR* ServerEventTypeToString(const UE::Cook::FMPCollectorServerTickContext::EServerEventType EventType)
	{
		switch (EventType)
		{
		case UE::Cook::FMPCollectorServerTickContext::EServerEventType::WorkerStartup:
			return TEXT("WorkerStartup");
		default:
			return TEXT("Invalid");
		}
	}

	FString GetTargetPlatformName(const ITargetPlatform* TargetPlatform)
	{
		return TargetPlatform != nullptr ? TargetPlatform->PlatformName() : TEXT("None");
	}

	FString JoinTargetPlatforms(TConstArrayView<const ITargetPlatform*> Platforms)
	{
		TArray<FString> PlatformNames;
		PlatformNames.Reserve(Platforms.Num());

		for (const ITargetPlatform* Platform : Platforms)
		{
			PlatformNames.Add(GetTargetPlatformName(Platform));
		}

		return PlatformNames.Num() > 0 ? FString::Join(PlatformNames, TEXT(", ")) : TEXT("None");
	}

	FString JoinTargetPlatforms(const TArray<const ITargetPlatform*>& Platforms)
	{
		return JoinTargetPlatforms(TConstArrayView<const ITargetPlatform*>(Platforms));
	}

	FString JoinChunkIds(const TConstArrayView<int32> ChunkIds)
	{
		TArray<FString> ChunkIdStrings;
		ChunkIdStrings.Reserve(ChunkIds.Num());

		for (const int32 ChunkId : ChunkIds)
		{
			ChunkIdStrings.Add(LexToString(ChunkId));
		}

		return ChunkIdStrings.Num() > 0 ? FString::Join(ChunkIdStrings, TEXT(", ")) : TEXT("None");
	}

	FString JoinInstigatorChain(const TArray<UE::Cook::FInstigator>& InstigatorChain)
	{
		TArray<FString> InstigatorStrings;
		InstigatorStrings.Reserve(InstigatorChain.Num());

		for (const UE::Cook::FInstigator& Instigator : InstigatorChain)
		{
			InstigatorStrings.Add(Instigator.ToString());
		}

		return InstigatorStrings.Num() > 0 ? FString::Join(InstigatorStrings, TEXT(" -> ")) : TEXT("None");
	}

	FString BuildCookPackageKey(const FString& PackageName, const TCHAR* TargetFilename)
	{
		return FString::Printf(TEXT("%s@%s"), *PackageName, TargetFilename);
	}

	FString DescribeCookSession(UE::Cook::ICookInfo& CookInfo)
	{
		FString Summary = FString::Printf(
			TEXT("cookType=%s processType=%s incremental=%s dlc=%s platforms=[%s]"),
			CookTypeToString(CookInfo.GetCookType()),
			ProcessTypeToString(CookInfo.GetProcessType()),
			BoolToString(CookInfo.IsIncremental()),
			CookingDlcToString(CookInfo.GetCookingDLC()),
			*JoinTargetPlatforms(CookInfo.GetSessionPlatforms())
		);

		const FString DlcName = CookInfo.GetDLCName();
		if (!DlcName.IsEmpty())
		{
			Summary += FString::Printf(TEXT(" dlcName=%s"), *DlcName);
		}

		const FString BasedOnReleaseVersion = CookInfo.GetBasedOnReleaseVersion();
		if (!BasedOnReleaseVersion.IsEmpty())
		{
			Summary += FString::Printf(TEXT(" basedOnReleaseVersion=%s"), *BasedOnReleaseVersion);
		}

		const FString CreateReleaseVersion = CookInfo.GetCreateReleaseVersion();
		if (!CreateReleaseVersion.IsEmpty())
		{
			Summary += FString::Printf(TEXT(" createReleaseVersion=%s"), *CreateReleaseVersion);
		}

		return Summary;
	}

	class FCtcCookChunkDataGenerator final : public IChunkDataGenerator
	{
	public:
		explicit FCtcCookChunkDataGenerator(FString InSessionSummary)
			: SessionSummary(MoveTemp(InSessionSummary))
		{
			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[ChunkGeneratorCreate] %s"),
				*SessionSummary
			);
		}

		virtual void BeginGenerateChunkDataFiles(FBeginGenerateContext& Context) override
		{
			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[ChunkBatchStart] platform=%s chunkIds=[%s] sandbox=%p"),
				*GetTargetPlatformName(Context.GetTargetPlatform()),
				*JoinChunkIds(Context.GetCurrentChunkIds()),
				Context.GetSandboxFile()
			);
		}

		virtual void GenerateChunkDataFiles(
			const int32 InChunkId,
			const TSet<FName>& InPackagesInChunk,
			const ITargetPlatform* TargetPlatform,
			FSandboxPlatformFile* InSandboxFile,
			TArray<FString>& OutChunkFilenames
		) override
		{
			const int32 InitialFilenameCount = OutChunkFilenames.Num();

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[ChunkCookStart] chunkId=%d platform=%s packageCount=%d sandbox=%p"),
				InChunkId,
				*GetTargetPlatformName(TargetPlatform),
				InPackagesInChunk.Num(),
				InSandboxFile
			);

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[ChunkCookEnd] chunkId=%d platform=%s addedFiles=%d totalFiles=%d"),
				InChunkId,
				*GetTargetPlatformName(TargetPlatform),
				OutChunkFilenames.Num() - InitialFilenameCount,
				OutChunkFilenames.Num()
			);
		}

	private:
		FString SessionSummary;
	};

	TSharedRef<IChunkDataGenerator> CreateCastToCloudChunkDataGenerator(const UE::Cook::ICookInfo& CookInfo)
	{
		// IChunkDataGenerator exposes a const ICookInfo&, but ICookInfo accessors are non-const in this engine branch.
		UE::Cook::ICookInfo& MutableCookInfo = const_cast<UE::Cook::ICookInfo&>(CookInfo);
		return StaticCastSharedRef<IChunkDataGenerator>(MakeShared<FCtcCookChunkDataGenerator>(DescribeCookSession(MutableCookInfo)));
	}

	class FCtcCookMpCollector final : public UE::Cook::IMPCollector
	{
	public:
		virtual FGuid GetMessageType() const override
		{
			return FGuid(0x4F052865, 0xD63442C4, 0xB89FBD18, 0x1AC2A831);
		}

		virtual const TCHAR* GetDebugName() const override
		{
			return TEXT("CastToCloudMetricsCookCollector");
		}

		virtual void ServerTick(UE::Cook::FMPCollectorServerTickContext& Context) override
		{
			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[MPCookServerTick] event=%s platforms=[%s]"),
				ServerEventTypeToString(Context.GetEventType()),
				*JoinTargetPlatforms(Context.GetPlatforms())
			);
		}

		virtual void ClientTick(UE::Cook::FMPCollectorClientTickContext& Context) override
		{
			if (!Context.IsFlush())
			{
				return;
			}

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[MPCookClientFlush] platforms=[%s]"),
				*JoinTargetPlatforms(Context.GetPlatforms())
			);
		}

		virtual void ServerTickPackage(UE::Cook::FMPCollectorServerTickPackageContext& Context) override
		{
			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[MPCookPackageAssign] package=%s"),
				*Context.GetPackageName().ToString()
			);
		}

		virtual void ClientTickPackage(UE::Cook::FMPCollectorClientTickPackageContext& Context) override
		{
			TArray<FString> PlatformResults;
			PlatformResults.Reserve(Context.GetPlatformDatas().Num());

			for (const UE::Cook::FMPCollectorClientTickPackageContext::FPlatformData& PlatformData : Context.GetPlatformDatas())
			{
				PlatformResults.Add(
					FString::Printf(
						TEXT("%s=%s"),
						*GetTargetPlatformName(PlatformData.TargetPlatform),
						CookResultToString(PlatformData.CookResults)
					)
				);
			}

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[MPCookPackageResult] package=%s results=[%s]"),
				*Context.GetPackageName().ToString(),
				PlatformResults.Num() > 0 ? *FString::Join(PlatformResults, TEXT(", ")) : TEXT("None")
			);
		}
	};

	class FCtcCookObserver final : public FTickableCookObject
	{
	public:
		void Startup()
		{
			CookStartedHandle = UE::Cook::FDelegates::CookStarted.AddRaw(this, &FCtcCookObserver::HandleCookStarted);
			CookFinishedHandle = UE::Cook::FDelegates::CookFinished.AddRaw(this, &FCtcCookObserver::HandleCookFinished);
			CookUpdateDisplayHandle = UE::Cook::FDelegates::CookUpdateDisplay.AddRaw(this, &FCtcCookObserver::HandleCookUpdateDisplay);
			CookSaveIdleHandle = UE::Cook::FDelegates::CookSaveIdle.AddRaw(this, &FCtcCookObserver::HandleCookSaveIdle);
			CookLoadIdleHandle = UE::Cook::FDelegates::CookLoadIdle.AddRaw(this, &FCtcCookObserver::HandleCookLoadIdle);
			PreSavePackageHandle = UPackage::PreSavePackageWithContextEvent.AddRaw(this, &FCtcCookObserver::HandlePreSavePackage);
			PackageSavedHandle = UPackage::PackageSavedWithContextEvent.AddRaw(this, &FCtcCookObserver::HandlePackageSaved);

			RegisterChunkGeneratorFactory();

			UE_LOG(LogCastToCloudMetricsEditor, Display, TEXT("Cook observability hooks registered."));
		}

		void Shutdown()
		{
			if (CookStartedHandle.IsValid())
			{
				UE::Cook::FDelegates::CookStarted.Remove(CookStartedHandle);
				CookStartedHandle.Reset();
			}

			if (CookFinishedHandle.IsValid())
			{
				UE::Cook::FDelegates::CookFinished.Remove(CookFinishedHandle);
				CookFinishedHandle.Reset();
			}

			if (CookUpdateDisplayHandle.IsValid())
			{
				UE::Cook::FDelegates::CookUpdateDisplay.Remove(CookUpdateDisplayHandle);
				CookUpdateDisplayHandle.Reset();
			}

			if (CookSaveIdleHandle.IsValid())
			{
				UE::Cook::FDelegates::CookSaveIdle.Remove(CookSaveIdleHandle);
				CookSaveIdleHandle.Reset();
			}

			if (CookLoadIdleHandle.IsValid())
			{
				UE::Cook::FDelegates::CookLoadIdle.Remove(CookLoadIdleHandle);
				CookLoadIdleHandle.Reset();
			}

			if (PreSavePackageHandle.IsValid())
			{
				UPackage::PreSavePackageWithContextEvent.Remove(PreSavePackageHandle);
				PreSavePackageHandle.Reset();
			}

			if (PackageSavedHandle.IsValid())
			{
				UPackage::PackageSavedWithContextEvent.Remove(PackageSavedHandle);
				PackageSavedHandle.Reset();
			}

			if (ActiveCookInfo != nullptr && MpCollector.IsValid())
			{
				ActiveCookInfo->UnregisterCollector(MpCollector.GetReference());
			}

			MpCollector.SafeRelease();
			FlushActivePackageSaves(TEXT("ModuleShutdown"));

			ActiveCookInfo = nullptr;
			bCookActive = false;
		}

		virtual void Tick(float DeltaTime) override
		{
		}

		virtual void TickCook(float DeltaTime, bool bCookComplete) override
		{
			if (!bCookActive)
			{
				return;
			}

			HeartbeatAccumulatorSeconds += DeltaTime;

			if (HeartbeatAccumulatorSeconds < CookHeartbeatIntervalSeconds && !(bCookComplete && !bLoggedCookCompleteTick))
			{
				return;
			}

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[CookHeartbeat] cookComplete=%s trackedPackageSaves=%d"),
				BoolToString(bCookComplete),
				ActivePackageSaveStartTimes.Num()
			);

			HeartbeatAccumulatorSeconds = 0.0;
			bLoggedCookCompleteTick |= bCookComplete;
		}

		virtual bool IsTickable() const override
		{
			return bCookActive;
		}

		virtual TStatId GetStatId() const override
		{
			RETURN_QUICK_DECLARE_CYCLE_STAT(FCtcCookObserver, STATGROUP_Tickables);
		}

	private:
		void HandleCookStarted(UE::Cook::ICookInfo& CookInfo)
		{
			if (ActiveCookInfo != nullptr && ActiveCookInfo != &CookInfo && MpCollector.IsValid())
			{
				ActiveCookInfo->UnregisterCollector(MpCollector.GetReference());
			}

			ActiveCookInfo = &CookInfo;
			bCookActive = true;
			bLoggedCookCompleteTick = false;
			HeartbeatAccumulatorSeconds = 0.0;
			LastCookedPackagesCount = INDEX_NONE;
			LastCookPendingCount = INDEX_NONE;

			if (!MpCollector.IsValid())
			{
				MpCollector = new FCtcCookMpCollector();
			}

			CookInfo.RegisterCollector(MpCollector.GetReference(), UE::Cook::EProcessType::AllMPCook);

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[CookSessionStart] %s"),
				*DescribeCookSession(CookInfo)
			);
		}

		void HandleCookFinished(UE::Cook::ICookInfo& CookInfo)
		{
			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[CookSessionEnd] %s trackedPackageSaves=%d"),
				*DescribeCookSession(CookInfo),
				ActivePackageSaveStartTimes.Num()
			);

			FlushActivePackageSaves(TEXT("CookFinished"));

			if (MpCollector.IsValid())
			{
				CookInfo.UnregisterCollector(MpCollector.GetReference());
			}

			ActiveCookInfo = nullptr;
			bCookActive = false;
			bLoggedCookCompleteTick = false;
			HeartbeatAccumulatorSeconds = 0.0;
			LastCookedPackagesCount = INDEX_NONE;
			LastCookPendingCount = INDEX_NONE;
		}

		void HandleCookUpdateDisplay(UE::Cook::ICookInfo& CookInfo, int32 CookedPackagesCount, int32 CookPendingCount)
		{
			if (CookedPackagesCount == LastCookedPackagesCount && CookPendingCount == LastCookPendingCount)
			{
				return;
			}

			LastCookedPackagesCount = CookedPackagesCount;
			LastCookPendingCount = CookPendingCount;

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[CookProgress] cookType=%s cooked=%d pending=%d"),
				CookTypeToString(CookInfo.GetCookType()),
				CookedPackagesCount,
				CookPendingCount
			);
		}

		void HandleCookSaveIdle(
			UE::Cook::ICookInfo& CookInfo,
			int32 NumPackagesInSaveQueue,
			int32 NumPendingCookedPlatformData,
			bool bExpectedDueToSlowBuildOperations
		)
		{
			UE_LOG(
				LogCastToCloudMetricsEditor,
				Warning,
				TEXT("[CookSaveIdle] cookType=%s saveQueue=%d pendingPlatformData=%d expectedSlowBuild=%s"),
				CookTypeToString(CookInfo.GetCookType()),
				NumPackagesInSaveQueue,
				NumPendingCookedPlatformData,
				BoolToString(bExpectedDueToSlowBuildOperations)
			);
		}

		void HandleCookLoadIdle(UE::Cook::ICookInfo& CookInfo, int32 NumPackagesInLoadQueue)
		{
			UE_LOG(
				LogCastToCloudMetricsEditor,
				Warning,
				TEXT("[CookLoadIdle] cookType=%s loadQueue=%d"),
				CookTypeToString(CookInfo.GetCookType()),
				NumPackagesInLoadQueue
			);
		}

		void HandlePreSavePackage(UPackage* Package, FObjectPreSaveContext SaveContext)
		{
			if (Package == nullptr || !SaveContext.IsCooking())
			{
				return;
			}

			const FString PackageName = Package->GetName();
			const FString PackageKey = BuildCookPackageKey(PackageName, SaveContext.GetTargetFilename());
			const double NowSeconds = FPlatformTime::Seconds();

			if (ActivePackageSaveStartTimes.Contains(PackageKey))
			{
				UE_LOG(
					LogCastToCloudMetricsEditor,
					Warning,
					TEXT("[PackageCookStartDuplicate] package=%s platform=%s"),
					*PackageName,
					*GetTargetPlatformName(SaveContext.GetTargetPlatform())
				);
			}

			ActivePackageSaveStartTimes.Add(PackageKey, NowSeconds);

			FString Instigator = TEXT("Unavailable");
			FString InstigatorChain = TEXT("Unavailable");

			if (ActiveCookInfo != nullptr)
			{
				const TArray<UE::Cook::FInstigator> CookInstigatorChain = ActiveCookInfo->GetInstigatorChain(Package->GetFName());

				if (CookInstigatorChain.Num() > 0)
				{
					Instigator = CookInstigatorChain[0].ToString();
					InstigatorChain = JoinInstigatorChain(CookInstigatorChain);
				}
				else
				{
					Instigator = ActiveCookInfo->GetInstigator(Package->GetFName()).ToString();
					InstigatorChain = Instigator;
				}
			}

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[PackageCookStart] package=%s platform=%s cookType=%s procedural=%s target=%s instigator=%s instigatorChain=%s"),
				*PackageName,
				*GetTargetPlatformName(SaveContext.GetTargetPlatform()),
				CookTypeToString(SaveContext.GetCookType()),
				BoolToString(SaveContext.IsProceduralSave()),
				SaveContext.GetTargetFilename(),
				*Instigator,
				*InstigatorChain
			);
		}

		void HandlePackageSaved(const FString& Filename, UPackage* Package, FObjectPostSaveContext SaveContext)
		{
			if (Package == nullptr || !SaveContext.IsCooking())
			{
				return;
			}

			const FString PackageName = Package->GetName();
			const FString PackageKey = BuildCookPackageKey(PackageName, *Filename);

			double DurationMilliseconds = -1.0;
			if (const double* StartTimeSeconds = ActivePackageSaveStartTimes.Find(PackageKey))
			{
				DurationMilliseconds = (FPlatformTime::Seconds() - *StartTimeSeconds) * 1000.0;
				ActivePackageSaveStartTimes.Remove(PackageKey);
			}

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[PackageCookEnd] package=%s platform=%s cookType=%s procedural=%s durationMs=%.2f file=%s"),
				*PackageName,
				*GetTargetPlatformName(SaveContext.GetTargetPlatform()),
				CookTypeToString(SaveContext.GetCookType()),
				BoolToString(SaveContext.IsProceduralSave()),
				DurationMilliseconds,
				*Filename
			);
		}

		void FlushActivePackageSaves(const TCHAR* Reason)
		{
			if (ActivePackageSaveStartTimes.IsEmpty())
			{
				return;
			}

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("[PackageCookUnpairedStartSummary] reason=%s count=%d"),
				Reason,
				ActivePackageSaveStartTimes.Num()
			);

			int32 SampleCount = 0;
			for (const TPair<FString, double>& ActiveSave : ActivePackageSaveStartTimes)
			{
				if (SampleCount >= 5)
				{
					break;
				}

				UE_LOG(
					LogCastToCloudMetricsEditor,
					Display,
					TEXT("[PackageCookUnpairedStartSample] reason=%s packageKey=%s ageMs=%.2f"),
					Reason,
					*ActiveSave.Key,
					(FPlatformTime::Seconds() - ActiveSave.Value) * 1000.0
				);

				++SampleCount;
			}

			ActivePackageSaveStartTimes.Empty();
		}

		void RegisterChunkGeneratorFactory()
		{
			if (GCastToCloudChunkFactoryRegistered)
			{
				return;
			}

			IChunkDataGenerator::AddChunkDataGeneratorFactory(&CreateCastToCloudChunkDataGenerator);
			GCastToCloudChunkFactoryRegistered = true;

			UE_LOG(
				LogCastToCloudMetricsEditor,
				Display,
				TEXT("Registered chunk data generator factory. Unreal does not expose an unregister API for this hook, so it remains registered for the lifetime of the process.")
			);
		}

	private:
		FDelegateHandle CookStartedHandle;
		FDelegateHandle CookFinishedHandle;
		FDelegateHandle CookUpdateDisplayHandle;
		FDelegateHandle CookSaveIdleHandle;
		FDelegateHandle CookLoadIdleHandle;
		FDelegateHandle PreSavePackageHandle;
		FDelegateHandle PackageSavedHandle;

		UE::Cook::ICookInfo* ActiveCookInfo = nullptr;
		TRefCountPtr<FCtcCookMpCollector> MpCollector;
		TMap<FString, double> ActivePackageSaveStartTimes;
		double HeartbeatAccumulatorSeconds = 0.0;
		int32 LastCookedPackagesCount = INDEX_NONE;
		int32 LastCookPendingCount = INDEX_NONE;
		bool bCookActive = false;
		bool bLoggedCookCompleteTick = false;
	};

	TUniquePtr<FCtcCookObserver> GCookObserver;
} // namespace

void FCtcMetricsEditorModule::StartupModule()
{
	if (!GCookObserver.IsValid())
	{
		GCookObserver = MakeUnique<FCtcCookObserver>();
		GCookObserver->Startup();
	}
}

void FCtcMetricsEditorModule::ShutdownModule()
{
	if (GCookObserver.IsValid())
	{
		GCookObserver->Shutdown();
		GCookObserver.Reset();
	}
}

IMPLEMENT_MODULE(FCtcMetricsEditorModule, CastToCloudMetricsEditor)
